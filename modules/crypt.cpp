/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
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

//! @author prozac@rottenboy.com
//
// The encryption here was designed to be compatible with mircryption's CBC
// mode.
//
// Latest tested against:
// MircryptionSuite - Mircryption ver 1.19.01 (dll v1.15.01 , mirc v7.32) CBC
// loaded and ready.
//
// TODO:
//
// 1) Encrypt key storage file
// 2) Some way of notifying the user that the current channel is in "encryption
// mode" verses plain text
// 3) Temporarily disable a target (nick/chan)
//
// NOTE: This module is currently NOT intended to secure you from your shell
// admin.
//       The keys are currently stored in plain text, so anyone with access to
//       your account (or root) can obtain them.
//       It is strongly suggested that you enable SSL between znc and your
//       client otherwise the encryption stops at znc and gets sent to your
//       client in plain text.
//

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <znc/SHA256.h>

#define REQUIRESSL 1
// To be removed in future versions
#define NICK_PREFIX_OLD_KEY "[nick-prefix]"
#define NICK_PREFIX_KEY "@nick-prefix@"

class CCryptMod : public CModule {
  private:
    /*
     * As used in other implementations like KVIrc, fish10, Quassel, FiSH-irssi, ...
     * all the way back to the original located at http://mircryption.sourceforge.net/Extras/McpsFishDH.zip
     */
    const char* m_sPrime1080 = "FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BADB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9CA2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B";
    /* Generate our keys once and reuse, just like ssh keys */
    std::unique_ptr<DH, decltype(&DH_free)> m_pDH;
    CString m_sPrivKey;
    CString m_sPubKey;

#if OPENSSL_VERSION_NUMBER < 0X10100000L
    static int DH_set0_pqg(DH* dh, BIGNUM* p, BIGNUM* q, BIGNUM* g) {
        /* If the fields p and g in dh are nullptr, the corresponding input
         * parameters MUST be non-nullptr.  q may remain nullptr.
         */
        if (dh == nullptr || (dh->p == nullptr && p == nullptr) || (dh->g == nullptr && g == nullptr))
            return 0;

        if (p != nullptr) {
            BN_free(dh->p);
            dh->p = p;
        }
        if (g != nullptr) {
            BN_free(dh->g);
            dh->g = g;
        }
        if (q != nullptr) {
            BN_free(dh->q);
            dh->q = q;
            dh->length = BN_num_bits(q);
        }

        return 1;
    }

    static void DH_get0_key(const DH* dh, const BIGNUM** pub_key, const BIGNUM** priv_key) {
        if (dh != nullptr) {
            if (pub_key != nullptr)
                *pub_key = dh->pub_key;
            if (priv_key != nullptr)
                *priv_key = dh->priv_key;
        }
    }

#endif

    bool DH1080_gen() {
        /* Generate our keys on first call */
        if (m_sPrivKey.empty() || m_sPubKey.empty()) {
            int len;
            const BIGNUM* bPrivKey = nullptr;
            const BIGNUM* bPubKey = nullptr;
            BIGNUM* bPrime = nullptr;
            BIGNUM* bGen = nullptr;

            if (!BN_hex2bn(&bPrime, m_sPrime1080) || !BN_dec2bn(&bGen, "2") || !DH_set0_pqg(m_pDH.get(), bPrime, nullptr, bGen) || !DH_generate_key(m_pDH.get())) {
                /* one of them failed */
                if (bPrime != nullptr)
                    BN_clear_free(bPrime);
                if (bGen != nullptr)
                    BN_clear_free(bGen);
                return false;
            }

            /* Get our keys */
            DH_get0_key(m_pDH.get(), &bPubKey, &bPrivKey);

            /* Get our private key */
            len = BN_num_bytes(bPrivKey);
            m_sPrivKey.resize(len);
            BN_bn2bin(bPrivKey, (unsigned char*)m_sPrivKey.data());
            m_sPrivKey.Base64Encode();

            /* Get our public key */
            len = BN_num_bytes(bPubKey);
            m_sPubKey.resize(len);
            BN_bn2bin(bPubKey, (unsigned char*)m_sPubKey.data());
            m_sPubKey.Base64Encode();

        }

        return true;
    }


    bool DH1080_comp(CString& sOtherPubKey, CString& sSecretKey) {
        unsigned long len;
        unsigned char* key = nullptr;
        BIGNUM* bOtherPubKey = nullptr;

        /* Prepare other public key */
        len = sOtherPubKey.Base64Decode();
        bOtherPubKey = BN_bin2bn((unsigned char*)sOtherPubKey.data(), len, nullptr);

        /* Generate secret key */
        key = (unsigned char*)calloc(DH_size(m_pDH.get()), 1);
        if ((len = DH_compute_key(key, bOtherPubKey, m_pDH.get())) == -1) {
            sSecretKey = "";
            if (bOtherPubKey != nullptr)
                BN_clear_free(bOtherPubKey);
            if (key != nullptr)
                free(key);
            return false;
        }

        /* Get our secret key */
        sSecretKey.resize(SHA256_DIGEST_SIZE);
        sha256(key, len, (unsigned char*)sSecretKey.data());
        sSecretKey.Base64Encode();
        sSecretKey.TrimRight("=");

        if (bOtherPubKey != nullptr)
            BN_clear_free(bOtherPubKey);
        if (key != nullptr)
            free(key);

        return true;
    }

    CString NickPrefix() {
        MCString::iterator it = FindNV(NICK_PREFIX_KEY);
        /*
         * Check for different Prefixes to not confuse modules with nicknames
         * Also check for overlap for rare cases like:
         * SP = "*"; NP = "*s"; "tatus" sends an encrypted message appearing at "*status"
         */
        CString sStatusPrefix = GetUser()->GetStatusPrefix();
        if (it != EndNV()) {
            size_t sp = sStatusPrefix.size();
            size_t np = it->second.size();
            int min = std::min(sp, np);
            if (min == 0 || sStatusPrefix.CaseCmp(it->second, min) != 0)
                return it->second;
        }
        return sStatusPrefix.StartsWith("*") ? "." : "*";
    }


  public:
    /* MODCONSTRUCTOR(CLASS) is of form "CLASS(...) : CModule(...)" */
    MODCONSTRUCTOR(CCryptMod) , m_pDH(DH_new(), DH_free) {
        AddHelpCommand();
        AddCommand("DelKey", static_cast<CModCommand::ModCmdFunc>(
                                 &CCryptMod::OnDelKeyCommand),
                   "<#chan|Nick>", "Remove a key for nick or channel");
        AddCommand("SetKey", static_cast<CModCommand::ModCmdFunc>(
                                 &CCryptMod::OnSetKeyCommand),
                   "<#chan|Nick> <Key>", "Set a key for nick or channel");
        AddCommand("ListKeys", static_cast<CModCommand::ModCmdFunc>(
                                   &CCryptMod::OnListKeysCommand),
                   "", "List all keys");
        AddCommand("KeyX", static_cast<CModCommand::ModCmdFunc>(
                                 &CCryptMod::OnKeyXCommand),
                   "<Nick>", "Start a DH1080 key exchange with nick");
        AddCommand("GetNickPrefix", static_cast<CModCommand::ModCmdFunc>(
                                 &CCryptMod::OnGetNickPrefixCommand),
                   "", "Get the nick prefix");
        AddCommand("SetNickPrefix", static_cast<CModCommand::ModCmdFunc>(
                                 &CCryptMod::OnSetNickPrefixCommand),
                   "[Prefix]", "Set the nick prefix, with no argument it's disabled.");
    }

    ~CCryptMod() override {
    }

    bool OnLoad(const CString& sArgsi, CString& sMessage) override {
        MCString::iterator it = FindNV(NICK_PREFIX_KEY);
        if (it == EndNV()) {
            /* Don't have the new prefix key yet */
            it = FindNV(NICK_PREFIX_OLD_KEY);
            if (it != EndNV()) {
                SetNV(NICK_PREFIX_KEY, it->second);
                DelNV(NICK_PREFIX_OLD_KEY);
            }
        }
        return true;
    }

    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        sTarget.TrimPrefix(NickPrefix());

        if (sMessage.TrimPrefix("``")) {
            return CONTINUE;
        }

        MCString::iterator it = FindNV(sTarget.AsLower());

        if (it != EndNV()) {
            CChan* pChan = GetNetwork()->FindChan(sTarget);
            CString sNickMask = GetNetwork()->GetIRCNick().GetNickMask();
            if (pChan) {
                if (!pChan->AutoClearChanBuffer())
                    pChan->AddBuffer(":" + NickPrefix() + _NAMEDFMT(sNickMask) +
                                         " PRIVMSG " + _NAMEDFMT(sTarget) +
                                         " :{text}",
                                     sMessage);
                GetUser()->PutUser(":" + NickPrefix() + sNickMask +
                                       " PRIVMSG " + sTarget + " :" + sMessage,
                                   nullptr, GetClient());
            }

            CString sMsg = MakeIvec() + sMessage;
            sMsg.Encrypt(it->second);
            sMsg.Base64Encode();
            sMsg = "+OK *" + sMsg;

            PutIRC("PRIVMSG " + sTarget + " :" + sMsg);
            return HALTCORE;
        }

        return CONTINUE;
    }

    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
        sTarget.TrimPrefix(NickPrefix());

        if (sMessage.TrimPrefix("``")) {
            return CONTINUE;
        }

        MCString::iterator it = FindNV(sTarget.AsLower());

        if (it != EndNV()) {
            CChan* pChan = GetNetwork()->FindChan(sTarget);
            CString sNickMask = GetNetwork()->GetIRCNick().GetNickMask();
            if (pChan) {
                if (!pChan->AutoClearChanBuffer())
                    pChan->AddBuffer(":" + NickPrefix() + _NAMEDFMT(sNickMask) +
                                         " NOTICE " + _NAMEDFMT(sTarget) +
                                         " :{text}",
                                     sMessage);
                GetUser()->PutUser(":" + NickPrefix() + sNickMask + " NOTICE " +
                                       sTarget + " :" + sMessage,
                                   nullptr, GetClient());
            }

            CString sMsg = MakeIvec() + sMessage;
            sMsg.Encrypt(it->second);
            sMsg.Base64Encode();
            sMsg = "+OK *" + sMsg;

            PutIRC("NOTICE " + sTarget + " :" + sMsg);
            return HALTCORE;
        }

        return CONTINUE;
    }

    EModRet OnUserAction(CString& sTarget, CString& sMessage) override {
        sTarget.TrimPrefix(NickPrefix());

        if (sMessage.TrimPrefix("``")) {
            return CONTINUE;
        }

        MCString::iterator it = FindNV(sTarget.AsLower());

        if (it != EndNV()) {
            CChan* pChan = GetNetwork()->FindChan(sTarget);
            CString sNickMask = GetNetwork()->GetIRCNick().GetNickMask();
            if (pChan) {
                if (!pChan->AutoClearChanBuffer())
                    pChan->AddBuffer(":" + NickPrefix() + _NAMEDFMT(sNickMask) +
                                         " PRIVMSG " + _NAMEDFMT(sTarget) +
                                         " :\001ACTION {text}\001",
                                     sMessage);
                GetUser()->PutUser(":" + NickPrefix() + sNickMask +
                                       " PRIVMSG " + sTarget + " :\001ACTION " +
                                       sMessage + "\001",
                                   nullptr, GetClient());
            }

            CString sMsg = MakeIvec() + sMessage;
            sMsg.Encrypt(it->second);
            sMsg.Base64Encode();
            sMsg = "+OK *" + sMsg;

            PutIRC("PRIVMSG " + sTarget + " :\001ACTION " + sMsg + "\001");
            return HALTCORE;
        }

        return CONTINUE;
    }

    EModRet OnUserTopic(CString& sTarget, CString& sMessage) override {
        sTarget.TrimPrefix(NickPrefix());

        if (sMessage.TrimPrefix("``")) {
            return CONTINUE;
        }

        MCString::iterator it = FindNV(sTarget.AsLower());

        if (it != EndNV()) {
            sMessage = MakeIvec() + sMessage;
            sMessage.Encrypt(it->second);
            sMessage.Base64Encode();
            sMessage = "+OK *" + sMessage;
        }

        return CONTINUE;
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        FilterIncoming(Nick.GetNick(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        CString sCommand = sMessage.Token(0);
        CString sOtherPubKey = sMessage.Token(1);

        if ((sCommand.Equals("DH1080_INIT") || sCommand.Equals("DH1080_INIT_CBC")) && !sOtherPubKey.empty()) {
            CString sSecretKey;
            CString sTail = sMessage.Token(2); /* For fish10 */

            /* remove trailing A */
            if (sOtherPubKey.TrimSuffix("A") && DH1080_gen() && DH1080_comp(sOtherPubKey, sSecretKey)) {
                PutModule("Received DH1080 public key from " + Nick.GetNick() + ", sending mine...");
                PutIRC("NOTICE " + Nick.GetNick() + " :DH1080_FINISH " + m_sPubKey + "A" + (sTail.empty()?"":(" " + sTail)));
                SetNV(Nick.GetNick().AsLower(), sSecretKey);
                PutModule("Key for " + Nick.GetNick() + " successfully set.");
                return HALT;
            }
            PutModule("Error in " + sCommand + " with " + Nick.GetNick() + ": " + (sSecretKey.empty()?"no secret key computed":sSecretKey));
            return CONTINUE;

        } else if (sCommand.Equals("DH1080_FINISH") && !sOtherPubKey.empty()) {
            /*
             * In theory we could get a DH1080_FINISH without us having sent a DH1080_INIT first,
             * but then to have any use for the other user, they'd already have our pub key
             */
            CString sSecretKey;

            /* remove trailing A */
            if (sOtherPubKey.TrimSuffix("A") && DH1080_gen() && DH1080_comp(sOtherPubKey, sSecretKey)) {
                SetNV(Nick.GetNick().AsLower(), sSecretKey);
                PutModule("Key for " + Nick.GetNick() + " successfully set.");
                return HALT;
            }
            PutModule("Error in " + sCommand + " with " + Nick.GetNick() + ": " + (sSecretKey.empty()?"no secret key computed":sSecretKey));
            return CONTINUE;
        }

        FilterIncoming(Nick.GetNick(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivAction(CNick& Nick, CString& sMessage) override {
        FilterIncoming(Nick.GetNick(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnChanAction(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnRaw(CString& sLine) override {
        if (!sLine.Token(1).Equals("332")) {
            return CONTINUE;
        }

        CChan* pChan = GetNetwork()->FindChan(sLine.Token(3));
        if (pChan) {
            CNick* Nick = pChan->FindNick(sLine.Token(2));
            CString sTopic = sLine.Token(4, true);
            sTopic.TrimPrefix(":");

            FilterIncoming(pChan->GetName(), *Nick, sTopic);
            sLine = sLine.Token(0) + " " + sLine.Token(1) + " " +
                    sLine.Token(2) + " " + pChan->GetName() + " :" + sTopic;
        }

        return CONTINUE;
    }

    void FilterIncoming(const CString& sTarget, CNick& Nick,
                        CString& sMessage) {
        if (sMessage.TrimPrefix("+OK *")) {
            MCString::iterator it = FindNV(sTarget.AsLower());

            if (it != EndNV()) {
                sMessage.Base64Decode();
                sMessage.Decrypt(it->second);
                sMessage.LeftChomp(8);
                sMessage = sMessage.c_str();
                Nick.SetNick(NickPrefix() + Nick.GetNick());
            }
        }
    }

    void OnDelKeyCommand(const CString& sCommand) {
        CString sTarget = sCommand.Token(1);

        if (!sTarget.empty()) {
            if (DelNV(sTarget.AsLower())) {
                PutModule("Target [" + sTarget + "] deleted");
            } else {
                PutModule("Target [" + sTarget + "] not found");
            }
        } else {
            PutModule("Usage DelKey <#chan|Nick>");
        }
    }

    void OnSetKeyCommand(const CString& sCommand) {
        CString sTarget = sCommand.Token(1);
        CString sKey = sCommand.Token(2, true);

        // Strip "cbc:" from beginning of string incase someone pastes directly
        // from mircryption
        sKey.TrimPrefix("cbc:");

        if (!sKey.empty()) {
            SetNV(sTarget.AsLower(), sKey);
            PutModule("Set encryption key for [" + sTarget + "] to [" + sKey +
                      "]");
        } else {
            PutModule("Usage: SetKey <#chan|Nick> <Key>");
        }
    }

    void OnKeyXCommand(const CString& sCommand) {
        CString sTarget = sCommand.Token(1);

        if (!sTarget.empty()) {
            if (DH1080_gen()) {
                PutIRC("NOTICE " + sTarget + " :DH1080_INIT " + m_sPubKey + "A");
                PutModule("Sent my DH1080 public key to " + sTarget + ", waiting for reply ...");
            } else {
                PutModule("Error generating our keys, nothing sent.");
            }
        } else {
            PutModule("Usage: KeyX <Nick>");
        }
    }

    void OnGetNickPrefixCommand(const CString& sCommand) {
        CString sPrefix = NickPrefix();
        PutModule("Nick Prefix" + (sPrefix.empty() ? " disabled." : (": " + sPrefix)));
    }

    void OnSetNickPrefixCommand(const CString& sCommand) {
        CString sPrefix = sCommand.Token(1);

/*        if (sPrefix.empty()) {
            PutModule("Nick Prefix: " + NickPrefix());
        } else */ if (sPrefix.StartsWith(":")) {
            PutModule("You cannot use :, even followed by other symbols, as Nick Prefix.");
        } else {
            CString sStatusPrefix = GetUser()->GetStatusPrefix();
            size_t sp = sStatusPrefix.size();
            size_t np = sPrefix.size();
            int min = std::min(sp, np);
            if (min > 0 && sStatusPrefix.CaseCmp(sPrefix, min) == 0)
                PutModule("Overlap with Status Prefix (" + sStatusPrefix + "), this Nick Prefix will not be used!");
            else {
                SetNV(NICK_PREFIX_KEY, sPrefix);
                if (sPrefix.empty())
                    PutModule("Disabling Nick Prefix.");
                else
                    PutModule("Setting Nick Prefix to " + sPrefix);
            }
        }
    }

    void OnListKeysCommand(const CString& sCommand) {
        CTable Table;
        Table.AddColumn("Target");
        Table.AddColumn("Key");

        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            if (!it->first.Equals(NICK_PREFIX_KEY)) {
                Table.AddRow();
                Table.SetCell("Target", it->first);
                Table.SetCell("Key", it->second);
            }
        }
        if (Table.empty())
            PutModule("You have no encryption keys set.");
        else
            PutModule(Table);
    }

    CString MakeIvec() {
        CString sRet;
        time_t t;
        time(&t);
        int r = rand();
        sRet.append((char*)&t, 4);
        sRet.append((char*)&r, 4);

        return sRet;
    }
};

template <>
void TModInfo<CCryptMod>(CModInfo& Info) {
    Info.SetWikiPage("crypt");
}

NETWORKMODULEDEFS(CCryptMod, "Encryption for channel/private messages")
