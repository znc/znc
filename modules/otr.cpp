/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/Chan.h>
#include <znc/Client.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Threads.h>
#include <znc/User.h>
#include <znc/Utils.h>

extern "C" {
#include <libotr/proto.h>

#include <libotr/instag.h>
#include <libotr/message.h>
#include <libotr/privkey.h>
#include <libotr/userstate.h>
#include <libotr/version.h>
}

// See http://www.gnupg.org/documentation/manuals/gcrypt/Multi_002dThreading.html
GCRY_THREAD_OPTION_PTHREAD_IMPL;

#include <cassert>
#include <cstring>
#include <iostream>
#include <list>

using std::list;

#define PROTOCOL_ID "irc"

/* Due to a bug in libotr-4.0.0, passing OTRL_FRAGMENT_SEND_ALL does not work
 * because otrInjectMessage callback receives NULL as opdata. This workaround
 * passes the pointer to the COtrMod instance in a global variable. The bug was
 * fixed in d748757 thus the workaround shouldn't be needed in libotr > 4.0.0.
 */
#if (OTRL_VERSION_MAJOR == 4 && OTRL_VERSION_MINOR == 0 && \
     OTRL_VERSION_SUB == 0)
#define INJECT_WORKAROUND_NEEDED
#endif
void* inject_workaround_mod;

// Could replace class CThread if merged into upstream.
class COtrThread {
  protected:
    pthread_t m_pThr;

    void CheckError(int retcode) {
        if (retcode) {
            CUtils::PrintError("Thread error: " + CString(strerror(errno)));
            exit(1);
        }
    }

  public:
    typedef void* threadRoutine(void*);

    COtrThread(threadRoutine* func, void* arg) {
        sigset_t old_sigmask, sigmask;

        /* Block all signals. The thread will inherit our signal mask
         * and thus won't ever try to handle signals.
         */
        int i = sigfillset(&sigmask);
        i |= pthread_sigmask(SIG_SETMASK, &sigmask, &old_sigmask);
        i |= pthread_create(&m_pThr, NULL, func, arg);
        i |= pthread_sigmask(SIG_SETMASK, &old_sigmask, NULL);
        CheckError(i);
    }

    void Detach() { CheckError(pthread_detach(m_pThr)); }

    void Cancel() { CheckError(pthread_cancel(m_pThr)); }

    void Join() { CheckError(pthread_join(m_pThr, NULL)); }

    static void startThread(threadRoutine* func, void* arg) {
        COtrThread th = COtrThread(func, arg);
        th.Detach();
    }
};

class COtrTimer : public CTimer {
  public:
    COtrTimer(CModule* pModule, unsigned int uInterval)
        : CTimer(pModule, uInterval, /*run forever*/ 0, "OtrTimer",
                 "OTR message poll") {}
    virtual ~COtrTimer() {}

  protected:
    virtual void RunJob();
};

// When key generation thread finishes, it calls Notify() which invokes
// ReadLine() in the main
// thread where we can finish the key generation and clean up after the thread.
class COtrGenKeyNotify : public CSocket {
  private:
    int m_iNotifyPipe[2];
    bool m_bFatalErrors;

  public:
    COtrGenKeyNotify(CModule* pModule)
        : CSocket(pModule), m_bFatalErrors(false) {
        m_iNotifyPipe[0] = m_iNotifyPipe[1] = -1;
        EnableReadLine();
        if (pipe(m_iNotifyPipe) < 0) {
            // On failure, the notification will do nothing and the thread will
            // hang
            // forever - better than crashing the whole process?
            CUtils::PrintError("Pipe error: " + CString(strerror(errno)));
            if (m_bFatalErrors) {
                exit(1);
            }
            return;
        }
        ConnectFD(m_iNotifyPipe[0], m_iNotifyPipe[1], "OtrGenKeyPipe");
        GetModule()->GetManager()->AddSock(this, "OtrGenKeyPipe");
    }

    // Thread calls Notify when it's finished
    void Notify() const {
        if (m_iNotifyPipe[1] == -1) {
            return;
        }

        ssize_t ret = write(m_iNotifyPipe[1], "\n", 1);
        if (ret < 0) {
            CUtils::PrintError("Pipe write error: " + CString(strerror(errno)));
            if (m_bFatalErrors) {
                exit(1);
            }
        }
    }

    virtual bool CheckTimeout(time_t iNow) { return false; }

    // Handle the notification
    virtual void ReadLine(const CString& sLine);
};

struct COtrAppData {
    bool bSmpReply;

    COtrAppData() { bSmpReply = false; }

    static void Add(void* data, ConnContext* context) {
        context->app_data = new COtrAppData();
        context->app_data_free = Free;
    }

    static void Free(void* appdata) {
        delete static_cast<COtrAppData*>(appdata);
    }
};

class COtrMod : public CModule {
    friend class COtrTimer;
    friend class COtrGenKeyNotify;

  private:
    static OtrlMessageAppOps m_xOtrOps;
    OtrlUserState m_pUserState;
    CString m_sPrivkeyPath;
    CString m_sFPPath;
    CString m_sInsTagPath;
    list<CString> m_Buffer;
    VCString m_vsIgnored;
    COtrTimer* m_pOtrTimer;

    // m_GenKeyRunning acts as a lock for those following it. We don't need an
    // actual lock because
    // it is accessed only from the main thread.
    bool m_GenKeyRunning;
    void* m_NewKey;
    gcry_error_t m_GenKeyError;
    COtrGenKeyNotify* m_GenKeyNotify;
    COtrThread* m_GenKeyThread;

  public:
    MODCONSTRUCTOR(COtrMod) {}

    bool PutModuleBuffered(const CString& sLine) {
        CUser* user = GetUser();
        bool attached = user->IsUserAttached();
        if (attached) {
            PutModule(sLine);
        } else {
            m_Buffer.push_back(sLine);
        }
        return attached;
    }

    bool PutModuleContext(ConnContext* ctx, const CString& sLine) {
        assert(ctx);
        assert(ctx->username);
        return PutModuleBuffered(CString("[") + ctx->username + "] " + sLine);
    }

    enum Color { Blue = 2, Green = 3, Red = 4, Bold = 32 };

    CString Clr(Color eClr, const CString& sWhat) {
        if (eClr == Bold) {
            return CString("\x02") + sWhat + CString("\x02");
        } else {
            return CString("\x03") + CString(eClr) + sWhat + CString("\x03");
        }
    }

    static CString HumanFingerprint(Fingerprint* fprint) {
        char human[OTRL_PRIVKEY_FPRINT_HUMAN_LEN];
        otrl_privkey_hash_to_human(human, fprint->fingerprint);
        return CString(human);
    }

    CString OurFingerprint() {
        char ourfp[OTRL_PRIVKEY_FPRINT_HUMAN_LEN];
        const char* accountname = GetUser()->GetUserName().c_str();
        if (otrl_privkey_fingerprint(m_pUserState, ourfp, accountname,
                                     PROTOCOL_ID)) {
            return CString(ourfp);
        }
        return CString("ERROR");
    }

    void WriteFingerprints() {
        gcry_error_t err;
        err = otrl_privkey_write_fingerprints(m_pUserState, m_sFPPath.c_str());
        if (err) {
            PutModuleBuffered(CString("Failed to write fingerprints: ") +
                              gcry_strerror(err));
        }
    }

    void WarnIfLoggingEnabled() {
        assert(GetNetwork());
        bool bUserMod = GetUser()->GetModules().FindModule("log");
        bool bNetworkMod = GetNetwork()->GetModules().FindModule("log");

        if (bUserMod || bNetworkMod) {
            CString sMsg =
                Clr(Red, "WARNING:") + " The log module is loaded. Type ";
            if (bUserMod) {
                sMsg += Clr(Bold, "/msg *status UnloadMod log") + " ";
                if (bNetworkMod) {
                    sMsg += "and ";
                }
            }
            if (bNetworkMod) {
                sMsg += Clr(Bold, "/msg *status UnloadMod --type=network log") +
                        " ";
            }
            sMsg += "to prevent ZNC from logging the conversation to disk.";
            PutModuleBuffered(sMsg);
        }
    }

    void GenerateKey(bool bOverwrite = false) {
        assert(m_pUserState);
        const char* accountname = GetUser()->GetUserName().c_str();

        if (!bOverwrite &&
            otrl_privkey_find(m_pUserState, accountname, PROTOCOL_ID)) {

            PutModuleBuffered("Private key already exists. Use " +
                              Clr(Bold, "genkey --overwrite") +
                              " to overwrite the old one.");
            return;
        }

        if (!m_GenKeyRunning) {
            gcry_error_t err;
            err = otrl_privkey_generate_start(m_pUserState, accountname,
                                              PROTOCOL_ID, &m_NewKey);
            if (err) {
                PutModuleBuffered(CString("Key generation failed: ") +
                                  gcry_strerror(err));
                return;
            }

            PutModuleBuffered(
                "Starting key generation in a background thread.");
            m_GenKeyNotify = new COtrGenKeyNotify(this);
            m_GenKeyRunning = true;
            m_GenKeyThread =
                new COtrThread(GenKeyThreadFunc, static_cast<void*>(this));
        } else {
            PutModuleBuffered("Key generation is already running.");
        }
    }

    void CmdInfo(const CString& sLine) {
        CTable table;
        table.AddColumn("Peer");
        table.AddColumn("State");
        table.AddColumn("Fingerprint");
        table.AddColumn("Act");
        table.AddColumn("Trust");

        ConnContext* ctx;
        for (ctx = m_pUserState->context_root; ctx; ctx = ctx->next) {
            // Iterate over master contexts since only they have the fingerprint
            // list.
            if (ctx->m_context != ctx) continue;

            // Show the state for the OTRL_INSTAG_BEST context, since that's the
            // one we
            // send our messages to.
            assert(ctx->username);
            ConnContext* best_ctx = otrl_context_find(
                m_pUserState, ctx->username, GetUser()->GetUserName().c_str(),
                PROTOCOL_ID, OTRL_INSTAG_BEST, 0, NULL, NULL, NULL);
            if (!best_ctx) continue;
            const char* state =
                (best_ctx->msgstate == OTRL_MSGSTATE_PLAINTEXT
                     ? "plaintext"
                     : (best_ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED
                            ? "encrypted"
                            : (best_ctx->msgstate == OTRL_MSGSTATE_FINISHED
                                   ? "finished"
                                   : "unknown")));

            Fingerprint* fp;
            for (fp = ctx->fingerprint_root.next; fp; fp = fp->next) {
                const char* trust;
                if (!fp->trust || fp->trust[0] == '\0') {
                    trust = "not trusted";
                } else if (0 == strcmp(fp->trust, "smp")) {
                    trust = "shared secret";
                } else {
                    trust = fp->trust;
                }
                table.AddRow();
                if (fp == ctx->fingerprint_root.next) {
                    table.SetCell("Peer", ctx->username);
                    table.SetCell("State", state);
                }
                table.SetCell("Fingerprint", HumanFingerprint(fp));
                table.SetCell("Trust", trust);
                if (fp == best_ctx->active_fingerprint) {
                    table.SetCell("Act", " * ");
                }
            }
        }
        if (m_pUserState->context_root) {
            PutModule(table);
        } else {
            PutModule("No fingerprints available.");
        }

        PutModule("Your fingerprint: " + OurFingerprint() + ".");
    }

    ConnContext* GetContextFromArg(const CString& sLine,
                                   bool bWarnIfNotFound = true) {
        CString sNick = sLine.Token(1).MakeLower();
        ConnContext* ctx = otrl_context_find(
            m_pUserState, sNick.c_str(), GetUser()->GetUserName().c_str(),
            PROTOCOL_ID, OTRL_INSTAG_BEST, 0, NULL, NULL, NULL);
        if (!ctx && bWarnIfNotFound) {
            PutModuleBuffered("Context for nick '" + sNick + "' not found.");
        }
        return ctx;
    }

    bool GetFprintFromArg(const CString& sLine, ConnContext*& ctx,
                          Fingerprint*& fprint) {
        fprint = NULL;
        // Try interpreting the argument as a nick and if we don't find a
        // context, interpret
        // it as human readable form of fingerprint.
        ctx = GetContextFromArg(sLine, false);
        if (ctx) {
            fprint = ctx->active_fingerprint;
            if (!fprint) {
                PutModuleContext(ctx, "No active fingerprint.");
                return false;
            }
        } else {
            CString sNormalizedFP = sLine.Token(1, true).Replace_n(" ", "");

            for (ConnContext* curctx = m_pUserState->context_root; curctx;
                 curctx = curctx->next) {
                for (Fingerprint* curfp = curctx->fingerprint_root.next; curfp;
                     curfp = curfp->next) {
                    CString sCtxFP = HumanFingerprint(curfp).Replace_n(" ", "");
                    if (sCtxFP.Equals(sNormalizedFP, false)) {
                        fprint = curfp;
                        ctx = curctx;
                    }
                }
            }
        }
        if (!fprint) {
            PutModuleBuffered(
                "Fingerprint not found. This comand takes either nick "
                "or hexadecimal fingerprint as an argument.");
            return false;
        }
        return true;
    }

    void CmdTrust(const CString& sLine) {
        ConnContext* ctx;
        Fingerprint* fprint;
        if (!GetFprintFromArg(sLine, ctx, fprint)) {
            return;
        }

        int already_trusted = otrl_context_is_fingerprint_trusted(fprint);
        if (already_trusted) {
            PutModuleContext(ctx, CString("Fingerprint ") +
                                      HumanFingerprint(fprint) +
                                      " already trusted.");
        } else {
            otrl_context_set_trust(fprint, "manual");
            PutModuleContext(ctx, CString("Fingerprint ") +
                                      HumanFingerprint(fprint) + " trusted!");
            WriteFingerprints();
        }
    }

    void CmdDistrust(const CString& sLine) {
        ConnContext* ctx;
        Fingerprint* fprint;
        if (!GetFprintFromArg(sLine, ctx, fprint)) {
            return;
        }

        int trusted = otrl_context_is_fingerprint_trusted(fprint);
        if (!trusted) {
            PutModuleContext(ctx, CString("Already not trusting ") +
                                      HumanFingerprint(fprint) + ".");
        } else {
            otrl_context_set_trust(fprint, "");
            PutModuleContext(ctx, CString("Fingerprint ") +
                                      HumanFingerprint(fprint) +
                                      " distrusted!");
            WriteFingerprints();
        }
    }

    void CmdFinish(const CString& sLine) {
        ConnContext* ctx = GetContextFromArg(sLine);
        if (!ctx) {
            return;
        }

        otrl_message_disconnect(m_pUserState, &m_xOtrOps, this,
                                ctx->accountname, PROTOCOL_ID, ctx->username,
                                ctx->their_instance);
        PutModuleContext(ctx, "Conversation finished.");
    }

    void DoSMP(ConnContext* ctx, const CString& sQuestion,
               const CString& sSecret) {
        if (sSecret.Equals("")) {
            PutModuleContext(ctx, "No secret given!");
            return;
        }

        if (ctx->msgstate != OTRL_MSGSTATE_ENCRYPTED) {
            PutModuleContext(ctx, "Not in OTR session.");
            return;
        }

        COtrAppData* ad = static_cast<COtrAppData*>(ctx->app_data);
        assert(ad);

        if (ad->bSmpReply) {
            otrl_message_respond_smp(
                m_pUserState, &m_xOtrOps, this, ctx,
                reinterpret_cast<const unsigned char*>(sSecret.c_str()),
                sSecret.length());
            PutModuleContext(ctx, "Responded to authentication.");
        } else {
            if (sQuestion.Equals("")) {
                otrl_message_initiate_smp(
                    m_pUserState, &m_xOtrOps, this, ctx,
                    reinterpret_cast<const unsigned char*>(sSecret.c_str()),
                    sSecret.length());
            } else {
                otrl_message_initiate_smp_q(
                    m_pUserState, &m_xOtrOps, this, ctx, sQuestion.c_str(),
                    reinterpret_cast<const unsigned char*>(sSecret.c_str()),
                    sSecret.length());
            }
            PutModuleContext(ctx, "Initiated authentication.");
        }

        ad->bSmpReply = false;
    }

    void CmdAuth(const CString& sLine) {
        ConnContext* ctx = GetContextFromArg(sLine);
        if (!ctx) {
            return;
        }

        CString sSecret = sLine.Token(2, true);
        DoSMP(ctx, "", sSecret);
    }

    void CmdAuthQ(const CString& sLine) {
        ConnContext* ctx = GetContextFromArg(sLine);
        if (!ctx) {
            return;
        }

        COtrAppData* ad = static_cast<COtrAppData*>(ctx->app_data);
        assert(ad);
        if (ad->bSmpReply) {
            PutModuleContext(ctx,
                             "Authentication in progress. Use Auth to "
                             "respond with secret, or AuthAbort to abort it");
            return;
        }

        CString sRest = sLine.Token(2, true);
        if (sRest.length() == 0 || sRest[0] != '[') {
            PutModuleContext(ctx,
                             "No question found. Did you enclose it in "
                             "square brackets?");
            return;
        }

        size_t iQEnd = sRest.find(']', 1);
        if (iQEnd == CString::npos) {
            PutModuleContext(ctx, "Closing bracket missing for question.");
            return;
        }

        if (iQEnd + 1 == sRest.length()) {
            PutModuleContext(ctx, "No secret given!");
            return;
        }

        CString sQuestion = sRest.substr(1, iQEnd - 1);
        CString sSecret = sRest.substr(iQEnd + 1, CString::npos);
        sSecret.Trim();

        DoSMP(ctx, sQuestion, sSecret);
    }

    void CmdAuthAbort(const CString& sLine) {
        ConnContext* ctx = GetContextFromArg(sLine);
        if (!ctx) {
            return;
        }

        COtrAppData* ad = static_cast<COtrAppData*>(ctx->app_data);
        assert(ad);
        ad->bSmpReply = false;

        otrl_message_abort_smp(m_pUserState, &m_xOtrOps, this, ctx);
        PutModuleContext(ctx, "Authentication aborted.");
    }

    void CmdGenKey(const CString& sLine) {
        bool bOverwrite = sLine.Token(1).Equals("--overwrite");
        GenerateKey(bOverwrite);
    }

    void SaveIgnores() {
        CString sFlat = "";
        bool bFirst = true;

        for (VCString::const_iterator it = m_vsIgnored.begin();
             it != m_vsIgnored.end(); it++) {
            if (bFirst) {
                bFirst = false;
            } else {
                sFlat += " ";
            }
            sFlat += *it;
        }
        SetNV("ignore", sFlat, true);
    }

    bool IsIgnored(const CString& sNick) {
        CString sNickLower = sNick.AsLower();

        for (VCString::const_iterator it = m_vsIgnored.begin();
             it != m_vsIgnored.end(); it++) {
            if (sNickLower.WildCmp(*it)) {
                return true;
            }
        }
        return false;
    }

    void CmdIgnore(const CString& sLine) {
        if (sLine.Token(1).Equals("")) {
            PutModuleBuffered("OTR is disabled for following nicks:");
            for (VCString::const_iterator it = m_vsIgnored.begin();
                 it != m_vsIgnored.end(); it++) {
                PutModuleBuffered(*it);
            }
        } else if (sLine.Token(1).Equals("--remove")) {
            CString sNick = sLine.Token(2);
            if (sNick.Equals("")) {
                PutModuleBuffered("Usage: ignore --remove nick");
                return;
            }

            bool bFound = false;
            for (VCString::iterator it = m_vsIgnored.begin();
                 it != m_vsIgnored.end(); it++) {
                if (it->Equals(sNick)) {
                    m_vsIgnored.erase(it);
                    bFound = true;
                    break;
                }
            }

            if (bFound) {
                SaveIgnores();
                PutModuleBuffered("Removed " + Clr(Bold, sNick) +
                                  " from OTR ignore list.");
            } else {
                PutModuleBuffered("Not on OTR ignore list: " + sNick);
            }
        } else {
            CString sNick = sLine.Token(1).MakeLower();
            m_vsIgnored.push_back(sNick);
            SaveIgnores();
            PutModuleBuffered("Added " + Clr(Bold, sNick) +
                              " to OTR ignore list.");
        }
    }

    virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
        // Initialize libgcrypt for multithreaded usage
        gcry_error_t err;
        err = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
        if (err) {
            sMessage = (CString("Failed to initialize gcrypt threading: ") +
                        gcry_strerror(err));
            return false;
        }

        // Initialize libotr if needed
        static bool otrInitialized = false;
        if (!otrInitialized) {
            OTRL_INIT;
            otrInitialized = true;
        }

        // Initialize userstate
        m_pUserState = otrl_userstate_create();
        m_GenKeyNotify = NULL;
        m_GenKeyThread = NULL;
        m_GenKeyRunning = false;

        m_pOtrTimer = NULL;

        m_sPrivkeyPath = GetSavePath() + "/otr.key";
        m_sFPPath = GetSavePath() + "/otr.fp";
        m_sInsTagPath = GetSavePath() + "/otr.instag";

        // Load private key
        err = otrl_privkey_read(m_pUserState, m_sPrivkeyPath.c_str());
        if (gcry_err_code(err) == GPG_ERR_NO_ERROR) {
            // PutModuleBuffered("Private keys loaded from " + m_sPrivkeyPath +
            // ".");
        } else if (gcry_err_code(err) == gcry_err_code_from_errno(ENOENT)) {
            PutModuleBuffered("No private key found. Type " +
                              Clr(Bold, "genkey") + " to generate new one.");
        } else {
            sMessage = (CString("Failed to load private key: ") +
                        gcry_strerror(err) + ".");
            return false;
        }

        // Load fingerprints
        err = otrl_privkey_read_fingerprints(m_pUserState, m_sFPPath.c_str(),
                                             COtrAppData::Add, NULL);
        if (gcry_err_code(err) == GPG_ERR_NO_ERROR) {
            // PutModuleBuffered("Fingerprints loaded from " + m_sFPPath + ".");
        } else if (gcry_err_code(err) == gcry_err_code_from_errno(ENOENT)) {
            // PutModuleBuffered("No fingerprint file found.");
        } else {
            sMessage = (CString("Failed to load fingerprints: ") +
                        gcry_strerror(err) + ".");
            return false;
        }

        //  Load instance tags
        err = otrl_instag_read(m_pUserState, m_sInsTagPath.c_str());
        if (gcry_err_code(err) == GPG_ERR_NO_ERROR) {
            // PutModuleBuffered("Instance tags loaded from " + m_sInsTagPath +
            // ".");
        } else if (gcry_err_code(err) == gcry_err_code_from_errno(ENOENT)) {
            // PutModuleBuffered("No instance tag file found.");
        } else {
            sMessage = (CString("Failed to load instance tags: ") +
                        gcry_strerror(err) + ".");
            return false;
        }

        // Initialize commands
        AddHelpCommand();
        AddCommand("Info",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdInfo), "",
                   "List known fingerprints");
        AddCommand(
            "Trust", static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdTrust),
            "<nick|fingerprint>",
            "Mark the user's fingerprint as trusted after veryfing it over "
            "secure channel.");
        AddCommand("Distrust",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdDistrust),
                   "<nick|fingerprint>",
                   "Mark user's fingerprint as not trusted.");
        AddCommand("Finish",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdFinish),
                   "<nick>", "Terminate an OTR conversation.");
        AddCommand("Auth",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdAuth),
                   "<nick> <secret>", "Authenticate using shared secret.");
        AddCommand("AuthQ",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdAuthQ),
                   "<nick> <[question]> <secret>",
                   "Authenticate using shared secret (providing a question).");
        AddCommand("AuthAbort",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdAuthAbort),
                   "<nick>", "Abort authentication with peer.");
        AddCommand("Ignore",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdIgnore),
                   "[--remove] [nick]",
                   "Manage list of nicks excluded from OTR encryption. "
                   "Accepts wildcards.");
        AddCommand("GenKey",
                   static_cast<CModCommand::ModCmdFunc>(&COtrMod::CmdGenKey),
                   "[--overwrite]", "Generate new private key.");

        // Load list of ignored nicks
        GetNV("ignore").Split(" ", m_vsIgnored, false);

        // Warn if we are not an administrator
        // We should check if we are the only administrator but the user map may
        // not be
        // fully populated at this time.
        if (!GetUser()->IsAdmin()) {
            PutModuleBuffered(
                Clr(Red, "WARNING:") +
                " You are not a ZNC admin. "
                "The ZNC administrator has access to your private keys "
                "which can be used to read your encrypted messages and to "
                "impersonate you.");
            PutModuleBuffered(
                "Do you trust their good intentions and the ability to "
                "protect your data from other people?");
        }

        return true;
    }

    virtual ~COtrMod() {
        // Wait for keygen thread
        if (m_GenKeyRunning) {
            PutModuleBuffered("Killing key generation thread.");
            m_GenKeyThread->Cancel();
            m_GenKeyThread->Join();
        }

        // No need to deactivate timers, they are removed in
        // CModule::~CModule().
        if (m_pUserState) otrl_userstate_free(m_pUserState);
    }

    static void DefaultQueryWorkaround(CString& sMessage) {
        /* libotr replaces ?OTR? request by a string that contains html tags and
         * newlines.
         * The newlines confuse IRC server, and if we sent them in a separate
         * PRIVMSG then
         * they would show up regardless of other side's otr plugin presnece,
         * defeating
         * the purpose of the message. We replace that message here, keeping the
         * OTR tag.
         */
        CString sPattern =
            "?OTR*\n<b>*</b> has requested an "
            "<a href=\"http://otr.cypherpunks.ca/\">Off-the-Record "
            "private conversation</a>.  However, you do not have a plugin "
            "to support that.\nSee <a href=\"http://otr.cypherpunks.ca/\">"
            "http://otr.cypherpunks.ca/</a> for more information.";

        if (!sMessage.WildCmp(sPattern)) {
            return;
        }

        sMessage =
            sMessage.FirstLine() +
            " Requesting an off-the-record private "
            "conversation. However, you do not have a plugin to support that. "
            "See http://otr.cypherpunks.ca/ for more information.";
    }

    bool TargetIsChan(const CString& sTarget) {
        CIRCNetwork* network = GetNetwork();
        assert(network);

        if (sTarget.empty()) {
            return true;
        } else if (network->GetChanPrefixes().empty()) {
            // RFC 2811
            return (CString("&#!+").find(sTarget[0]) != CString::npos);
        } else {
            return (network->GetChanPrefixes().find(sTarget[0]) !=
                    CString::npos);
        }
    }

    EModRet SendEncrypted(CString& sTarget, CString& sMessage) {
        gcry_error_t err;
        char* newmessage = NULL;
        const char* accountname = GetUser()->GetUserName().c_str();
        CString sNick = sTarget.AsLower();

        inject_workaround_mod = this;
        err = otrl_message_sending(
            m_pUserState, &m_xOtrOps, this, accountname, PROTOCOL_ID,
            sNick.c_str(), OTRL_INSTAG_BEST, sMessage.c_str(), NULL,
            &newmessage, OTRL_FRAGMENT_SEND_ALL, NULL, COtrAppData::Add, NULL);
        inject_workaround_mod = NULL;

        if (err) {
            PutModuleBuffered(CString("otrl_message_sending failed: ") +
                              gcry_strerror(err));
            return HALT;
        }

        if (newmessage) {
            // libotr injected the message
            otrl_message_free(newmessage);
            return HALT;
        } else {
            // not an OTR message
            return CONTINUE;
        }
    }

    virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
        // Do not pass the message to libotr if sTarget is a channel
        if (TargetIsChan(sTarget) || IsIgnored(sTarget)) {
            return CONTINUE;
        }

        return SendEncrypted(sTarget, sMessage);
    }

    virtual EModRet OnUserAction(CString& sTarget, CString& sMessage) {
        if (TargetIsChan(sTarget) || IsIgnored(sTarget)) {
            return CONTINUE;
        }

        // http://www.cypherpunks.ca/pipermail/otr-dev/2012-December/001520.html
        // suggests using following:
        // CString sLine = "\001ACTION " + sMessage + "\001";
        // However, irssi and weechat plugins send it like this:
        CString sLine = "/me " + sMessage;

        // Try sending the formatted line. If CONTINUE is returned,
        // pass unformatted to other plugins/caller.
        return SendEncrypted(sTarget, sLine);
    }

    virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
        int res;
        char* newmessage = NULL;
        OtrlTLV* tlvs = NULL;
        ConnContext* ctx = NULL;
        CString sNick = Nick.GetNick().AsLower();

        if (IsIgnored(Nick.GetNick())) {
            return CONTINUE;
        }

        const char* accountname = GetUser()->GetUserName().c_str();
        res = otrl_message_receiving(m_pUserState, &m_xOtrOps, this,
                                     accountname, PROTOCOL_ID, sNick.c_str(),
                                     sMessage.c_str(), &newmessage, &tlvs, &ctx,
                                     COtrAppData::Add, NULL);

        if (ctx && otrl_tlv_find(tlvs, OTRL_TLV_DISCONNECTED)) {
            PutModuleContext(
                ctx,
                "Peer has finished the conversation. "
                "Type " +
                    Clr(Bold, "finish " + Nick.GetNick()) +
                    " to enter plaintext mode, or send ?OTR? to start "
                    "new OTR session.");
        }
        if (tlvs) {
            otrl_tlv_free(tlvs);
        }

        if (res == 1) {
            // PutModule("Received internal OTR message");
            return HALT;
        } else if (res != 0) {
            PutModuleBuffered(
                CString("otrl_message_receiving: unknown return code ") +
                CString(res));
            return HALT;
        } else if (newmessage == NULL) {
            // PutModule("Received non-encrypted privmsg");
            return CONTINUE;
        } else {
            // PutModule("Received encrypted privmsg");
            sMessage = CString(newmessage);
            otrl_message_free(newmessage);

            // Handle /me as sent by irssi and weechat plugins
            if (sMessage.Left(4) == "/me ") {
                sMessage.LeftChomp(4);
                sMessage = "\001ACTION " + sMessage + "\001";
            }

            return CONTINUE;
        }
    }

    virtual void OnClientLogin() {
        for (list<CString>::iterator it = m_Buffer.begin();
             it != m_Buffer.end(); it++) {
            PutModule(*it);
        }
        m_Buffer.clear();
    }

  private:
    // genkey thread routine
    static void* GenKeyThreadFunc(void* data) {
        COtrMod* mod = static_cast<COtrMod*>(data);
        assert(mod);

        gcry_error_t err = otrl_privkey_generate_calculate(mod->m_NewKey);
        mod->m_GenKeyError = err;
        mod->m_GenKeyNotify->Notify();

        return NULL;
    }

    // libotr callbacks

    static OtrlPolicy otrPolicy(void* opdata, ConnContext* context) {
        return OTRL_POLICY_DEFAULT;
    }

    static void otrCreatePrivkey(void* opdata, const char* accountname,
                                 const char* protocol) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        assert(0 == strcmp(protocol, PROTOCOL_ID));

        mod->PutModuleBuffered(
            "Someone wants to start an OTR session but you don't have a "
            "key available.");
        mod->GenerateKey();
    }

    static int otrIsLoggedIn(void* opdata, const char* accountname,
                             const char* protocol, const char* recipient) {
        // Assume always online, otrl_message_disconnect does nothing otherwise.
        return 1;
    }

    static void otrInjectMessage(void* opdata, const char* accountname,
                                 const char* protocol, const char* recipient,
                                 const char* message) {
#ifdef INJECT_WORKAROUND_NEEDED
        opdata = (opdata ? opdata : inject_workaround_mod);
#endif
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        assert(0 == strcmp(protocol, PROTOCOL_ID));

        // libotr-4.0.0 injects empty message when sending to recipient in
        // finished state
        if (!message[0]) {
            return;
        }

        CString sMessage(message);
        if (!mod->TargetIsChan(CString(recipient))) {
            DefaultQueryWorkaround(sMessage);
        }

        mod->PutIRC(CString("PRIVMSG ") + recipient + " :" + sMessage);
    }

    static void otrWriteFingerprints(void* opdata) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        mod->WriteFingerprints();
    }

    static void otrGoneSecure(void* opdata, ConnContext* context) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        mod->PutModuleContext(
            context, "Gone " + mod->Clr(Bold, "SECURE") +
                         ". Please "
                         "make sure logging is turned off on your IRC client.");

        mod->WarnIfLoggingEnabled();

        assert(context->active_fingerprint);
        if (!otrl_context_is_fingerprint_trusted(context->active_fingerprint)) {
            mod->PutModuleContext(context,
                                  "Peer is not authenticated. There are two "
                                  "ways of verifying their identity:");
            mod->PutModuleContext(
                context,
                "1. Agree on a common secret (do not type "
                "it into the chat), then type " +
                    mod->Clr(Bold, CString("auth ") + context->username +
                                       " <secret>") +
                    ".");
            mod->PutModuleContext(
                context,
                "2. Compare their fingerprint over a "
                "secure channel, then type " +
                    mod->Clr(Bold, CString("trust ") + context->username) +
                    ".");
            mod->PutModuleContext(
                context,
                "Your fingerprint:  " + mod->Clr(Bold, mod->OurFingerprint()));
            mod->PutModuleContext(
                context, "Their fingerprint: " +
                             mod->Clr(Bold, HumanFingerprint(
                                                context->active_fingerprint)));
        }
    }

    static void otrGoneInsecure(void* opdata, ConnContext* context) {
        /* This callback doesn't seem to be used by libotr-4.0.0. */
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        mod->PutModuleContext(context,
                              "Gone " + mod->Clr(Bold, "INSECURE") + ".");
    }

    static void otrStillSecure(void* opdata, ConnContext* context,
                               int is_reply) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        mod->PutModuleContext(context, "Still " + mod->Clr(Bold, "SECURE") +
                                           " (restarted by " +
                                           (is_reply ? "peer" : "us") + ").");
    }

    static int otrMaxMessageSize(void* opdata, ConnContext* context) {
        // RFC 1459 says that maximum irc line length is 512 characters which
        // would mean that the maximum size of the message itself is 512 -
        // strlen("PRIVMSG ") - recipient_len - strlen(" :") - strlen("\r\n").
        // HOWEVER, server prepends the sender to the message upon receiving
        // PRIVMSG and truncates the result to 512 chars. There doesn't seem to
        // be a maximum length of the prefix. So ... I guess 400 should work
        // with reasonably long nicks/hostnames.
        return 400;
    }

    static void otrFreeStringNop(void* opdata, const char* str) {}

    static void otrReceiveSymkey(void* opdata, ConnContext* context,
                                 unsigned int use, const unsigned char* usedata,
                                 size_t usedatalen,
                                 const unsigned char* symkey) {
        /* We don't have any use for a symmetric key. */
        return;
    }

    static const char* otrErrorMessage(void* opdata, ConnContext* context,
                                       OtrlErrorCode err_code) {
        switch (err_code) {
            case OTRL_ERRCODE_ENCRYPTION_ERROR:
                return "Error encrypting message.";
            case OTRL_ERRCODE_MSG_NOT_IN_PRIVATE:
                return "Sent an encrypted message to somebody who is not in "
                       "OTR session.";
            case OTRL_ERRCODE_MSG_UNREADABLE:
                return "Sent an unreadable encrypted message.";
            case OTRL_ERRCODE_MSG_MALFORMED:
                return "Malformed message sent.";
            default:
                return "Unknown error.";
        }
    }

    static void otrHandleSMPEvent(void* opdata, OtrlSMPEvent smp_event,
                                  ConnContext* context,
                                  unsigned short progress_percent,
                                  char* question) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);

        assert(context);
        COtrAppData* ad = static_cast<COtrAppData*>(context->app_data);
        assert(ad);

        switch (smp_event) {
            case OTRL_SMPEVENT_ASK_FOR_ANSWER:
                assert(question);
                mod->PutModuleContext(
                    context,
                    "Peer wants to authenticate. To complete, "
                    "answer the following question:");
                mod->PutModuleContext(context, question);
                mod->PutModuleContext(context,
                                      CString("Answer by typing auth ") +
                                          context->username + " <answer>.");
                ad->bSmpReply = true;
                break;
            case OTRL_SMPEVENT_ASK_FOR_SECRET:
                mod->PutModuleContext(context,
                                      CString("Peer wants to authenticate. To "
                                              "complete, type auth ") +
                                          context->username + " <secret>.");
                ad->bSmpReply = true;
                break;
            case OTRL_SMPEVENT_IN_PROGRESS:
                mod->PutModuleContext(
                    context, "Peer replied to authentication request.");
                break;
            case OTRL_SMPEVENT_SUCCESS:
                mod->PutModuleContext(context, "Successfully authenticated.");
                break;
            case OTRL_SMPEVENT_ABORT:
                mod->PutModuleContext(context, "Authentication aborted.");
                break;
            case OTRL_SMPEVENT_FAILURE:
            case OTRL_SMPEVENT_ERROR:
            case OTRL_SMPEVENT_CHEATED:
                mod->PutModuleContext(context, "Authentication failed.");
                if (smp_event != OTRL_SMPEVENT_FAILURE) {
                    otrl_message_abort_smp(mod->m_pUserState, &mod->m_xOtrOps,
                                           opdata, context);
                }
                break;
            default:
                mod->PutModuleContext(context, "Unknown authentication event.");
                break;
        }
    }

    static void otrHandleMsgEvent(void* opdata, OtrlMessageEvent msg_event,
                                  ConnContext* ctx, const char* message,
                                  gcry_error_t err) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);

        CString sTo = mod->GetNetwork()->GetCurNick();
        CString sWarn = "[" + mod->Clr(Red, "NOT ENCRYPTED") + "]";

        switch (msg_event) {
            case OTRL_MSGEVENT_ENCRYPTION_REQUIRED:
                mod->PutModuleContext(
                    ctx,
                    "Our policy requires encryption but we are trying "
                    "to send an unencrypted message out.");
                break;
            case OTRL_MSGEVENT_ENCRYPTION_ERROR:
                mod->PutModuleContext(
                    ctx,
                    "An error occured while encrypting a message and "
                    "the message was not sent.");
                break;
            case OTRL_MSGEVENT_CONNECTION_ENDED:
                mod->PutModuleContext(
                    ctx,
                    "Message has not been sent because our peer has "
                    "ended the private conversation. You should either close "
                    "the "
                    "connection (by typing " +
                        mod->Clr(Bold, CString("finish ") + ctx->username) +
                        "), " + "or restart it by sending them " +
                        mod->Clr(Bold, "?OTR?") + ".");
                break;
            case OTRL_MSGEVENT_SETUP_ERROR:
                mod->PutModuleContext(
                    ctx,
                    CString("A private conversation could not be set up: ") +
                        gcry_strerror(err));
                break;
            case OTRL_MSGEVENT_MSG_REFLECTED:
                mod->PutModuleContext(ctx, "Received our own OTR messages.");
                break;
            case OTRL_MSGEVENT_MSG_RESENT:
                mod->PutModuleContext(ctx, "The previous message was resent.");
                break;
            case OTRL_MSGEVENT_RCVDMSG_NOT_IN_PRIVATE:
                mod->PutModuleContext(
                    ctx,
                    "Received an encrypted message but cannot read it "
                    "because no private connection is established yet.");
                break;
            case OTRL_MSGEVENT_RCVDMSG_UNREADABLE:
                mod->PutModuleContext(ctx, "Cannot read the received message.");
                break;
            case OTRL_MSGEVENT_RCVDMSG_MALFORMED:
                mod->PutModuleContext(
                    ctx, "The message received contains malformed data.");
                break;
            case OTRL_MSGEVENT_LOG_HEARTBEAT_RCVD:
                // mod->PutModuleContext(ctx, "Received a heartbeat.");
                break;
            case OTRL_MSGEVENT_LOG_HEARTBEAT_SENT:
                // mod->PutModuleContext(ctx, "Sent a heartbeat.");
                break;
            case OTRL_MSGEVENT_RCVDMSG_GENERAL_ERR:
                mod->PutModuleContext(
                    ctx, CString("Received a general OTR error: ") + message);
                break;
            case OTRL_MSGEVENT_RCVDMSG_UNENCRYPTED:
                mod->PutModuleContext(
                    ctx,
                    CString("Received an unencrypted message: ") + message);
                mod->PutUser(CString(":") + ctx->username + " PRIVMSG " + sTo +
                             " :" + sWarn + " " + message);
                break;
            case OTRL_MSGEVENT_RCVDMSG_UNRECOGNIZED:
                mod->PutModuleContext(
                    ctx,
                    "Cannot recognize the type of OTR message "
                    "received.");
                break;
            case OTRL_MSGEVENT_RCVDMSG_FOR_OTHER_INSTANCE:
                mod->PutModuleContext(
                    ctx,
                    "Received and discarded a message intended for "
                    "another instance.");
                break;
            default:
                mod->PutModuleContext(ctx, "Unknown message event.");
                break;
        }
    }

    static void otrCreateInsTag(void* opdata, const char* accountname,
                                const char* protocol) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);
        assert(!mod->m_sInsTagPath.empty());

        OtrlUserState us = mod->m_pUserState;
        assert(us);

        otrl_instag_generate(us, mod->m_sInsTagPath.c_str(), accountname,
                             protocol);
    }

    static void otrTimerControl(void* opdata, unsigned int interval) {
        COtrMod* mod = static_cast<COtrMod*>(opdata);
        assert(mod);

        if (mod->m_pOtrTimer) {
            if (interval > 0) {
                // Change the timer interval
                mod->m_pOtrTimer->Start(interval);
            } else {
                // Mark the timer for deletion - we can't directly remove it
                // because
                // this callback can be invoked from the timer's RunJob method
                mod->m_pOtrTimer->Stop();
                mod->m_pOtrTimer = NULL;
            }
        } else {
            if (interval > 0) {
                // Create new timer
                mod->m_pOtrTimer = new COtrTimer(mod, interval);
                if (!mod->AddTimer(mod->m_pOtrTimer)) {
                    mod->PutModuleBuffered(
                        "Error: failed to create timer, "
                        "forward secrecy not guaranteed.");
                }
            } else {
                // Should never happen?
                mod->PutModuleBuffered(
                    "Error: trying to delete nonexistent timer.");
            }
        }
    }

    static OtrlMessageAppOps InitOps() {
        OtrlMessageAppOps ops;

        ops.policy = otrPolicy;
        ops.create_privkey = otrCreatePrivkey;
        ops.is_logged_in = otrIsLoggedIn;
        ops.inject_message = otrInjectMessage;
        ops.update_context_list = NULL;  // do nothing
        ops.new_fingerprint = NULL;      // do nothing
        ops.write_fingerprints = otrWriteFingerprints;
        ops.gone_secure = otrGoneSecure;
        ops.gone_insecure = otrGoneInsecure;
        ops.still_secure = otrStillSecure;
        ops.max_message_size = otrMaxMessageSize;
        ops.account_name = NULL;  // unused, deprecated
        ops.account_name_free = NULL;
        ops.received_symkey = otrReceiveSymkey;
        ops.otr_error_message = otrErrorMessage;
        ops.otr_error_message_free = otrFreeStringNop;
        ops.resent_msg_prefix = NULL;  // uses [resent] by default
        ops.resent_msg_prefix_free = NULL;
        ops.handle_smp_event = otrHandleSMPEvent;
        ops.handle_msg_event = otrHandleMsgEvent;
        ops.create_instag = otrCreateInsTag;
        ops.convert_msg = NULL;  // no conversion
        ops.convert_free = NULL;
        ops.timer_control = otrTimerControl;

        return ops;
    }

    // end of callbacks
};

OtrlMessageAppOps COtrMod::m_xOtrOps = COtrMod::InitOps();

template <>
void TModInfo<COtrMod>(CModInfo& Info) {
    Info.SetWikiPage("otr");
    Info.SetHasArgs(false);
    // Info.SetArgsHelpText("No args.");
}

NETWORKMODULEDEFS(COtrMod,
                  "Off-the-Record (OTR) encryption for private messages")

void COtrTimer::RunJob() {
    COtrMod* mod = static_cast<COtrMod*>(m_pModule);
    assert(mod);
    otrl_message_poll(mod->m_pUserState, &mod->m_xOtrOps, mod);
}

void COtrGenKeyNotify::ReadLine(const CString& sLine) {
    COtrMod* mod = static_cast<COtrMod*>(GetModule());
    assert(mod);
    assert(!mod->m_sPrivkeyPath.empty());

    if (!mod->m_GenKeyError) {
        mod->m_GenKeyError = otrl_privkey_generate_finish(
            mod->m_pUserState, mod->m_NewKey, mod->m_sPrivkeyPath.c_str());
    }
    if (mod->m_GenKeyError) {
        mod->PutModuleBuffered(CString("Key generation failed: ") +
                               gcry_strerror(mod->m_GenKeyError));
    } else {
        mod->PutModuleBuffered("Key generation finished.");
    }

    mod->m_GenKeyThread->Join();
    delete mod->m_GenKeyThread;
    mod->m_GenKeyThread = NULL;

    Close();  // mark socket for deletion - socket manager takes care of
    mod->m_GenKeyNotify = NULL;  // it, we can drop the reference now

    mod->m_NewKey = NULL;
    mod->m_GenKeyRunning = false;
}
