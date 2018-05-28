/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#include <znc/Modules.h>
#include <znc/FileUtils.h>
#include <znc/Template.h>
#include <znc/User.h>
#include <znc/Query.h>
#include <znc/IRCNetwork.h>
#include <znc/WebModules.h>
#include <znc/znc.h>
#include <dlfcn.h>

using std::map;
using std::set;
using std::vector;

bool ZNC_NO_NEED_TO_DO_ANYTHING_ON_MODULE_CALL_EXITER;

#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#warning "your crap box doesn't define RTLD_LOCAL !?"
#endif

#define MODUNLOADCHK(func)                              \
    for (CModule * pMod : *this) {                      \
        try {                                           \
            CClient* pOldClient = pMod->GetClient();    \
            pMod->SetClient(m_pClient);                 \
            CUser* pOldUser = nullptr;                  \
            if (m_pUser) {                              \
                pOldUser = pMod->GetUser();             \
                pMod->SetUser(m_pUser);                 \
            }                                           \
            CIRCNetwork* pNetwork = nullptr;            \
            if (m_pNetwork) {                           \
                pNetwork = pMod->GetNetwork();          \
                pMod->SetNetwork(m_pNetwork);           \
            }                                           \
            pMod->func;                                 \
            if (m_pUser) pMod->SetUser(pOldUser);       \
            if (m_pNetwork) pMod->SetNetwork(pNetwork); \
            pMod->SetClient(pOldClient);                \
        } catch (const CModule::EModException& e) {     \
            if (e == CModule::UNLOAD) {                 \
                UnloadModule(pMod->GetModName());       \
            }                                           \
        }                                               \
    }

#define MODHALTCHK(func)                                \
    bool bHaltCore = false;                             \
    for (CModule * pMod : *this) {                      \
        try {                                           \
            CModule::EModRet e = CModule::CONTINUE;     \
            CClient* pOldClient = pMod->GetClient();    \
            pMod->SetClient(m_pClient);                 \
            CUser* pOldUser = nullptr;                  \
            if (m_pUser) {                              \
                pOldUser = pMod->GetUser();             \
                pMod->SetUser(m_pUser);                 \
            }                                           \
            CIRCNetwork* pNetwork = nullptr;            \
            if (m_pNetwork) {                           \
                pNetwork = pMod->GetNetwork();          \
                pMod->SetNetwork(m_pNetwork);           \
            }                                           \
            e = pMod->func;                             \
            if (m_pUser) pMod->SetUser(pOldUser);       \
            if (m_pNetwork) pMod->SetNetwork(pNetwork); \
            pMod->SetClient(pOldClient);                \
            if (e == CModule::HALTMODS) {               \
                break;                                  \
            } else if (e == CModule::HALTCORE) {        \
                bHaltCore = true;                       \
            } else if (e == CModule::HALT) {            \
                bHaltCore = true;                       \
                break;                                  \
            }                                           \
        } catch (const CModule::EModException& e) {     \
            if (e == CModule::UNLOAD) {                 \
                UnloadModule(pMod->GetModName());       \
            }                                           \
        }                                               \
    }                                                   \
    return bHaltCore;

/////////////////// Timer ///////////////////
CTimer::CTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles,
               const CString& sLabel, const CString& sDescription)
    : CCron(), m_pModule(pModule), m_sDescription(sDescription) {
    SetName(sLabel);

    // Make integration test faster
    char* szDebugTimer = getenv("ZNC_DEBUG_TIMER");
    if (szDebugTimer && *szDebugTimer == '1') {
        uInterval = std::max(1u, uInterval / 4u);
    }

    if (uCycles) {
        StartMaxCycles(uInterval, uCycles);
    } else {
        Start(uInterval);
    }
}

CTimer::~CTimer() { m_pModule->UnlinkTimer(this); }

void CTimer::SetModule(CModule* p) { m_pModule = p; }
void CTimer::SetDescription(const CString& s) { m_sDescription = s; }
CModule* CTimer::GetModule() const { return m_pModule; }
const CString& CTimer::GetDescription() const { return m_sDescription; }
/////////////////// !Timer ///////////////////

CModule::CModule(ModHandle pDLL, CUser* pUser, CIRCNetwork* pNetwork,
                 const CString& sModName, const CString& sDataDir,
                 CModInfo::EModuleType eType)
    : m_eType(eType),
      m_sDescription(""),
      m_sTimers(),
      m_sSockets(),
#ifdef HAVE_PTHREAD
      m_sJobs(),
#endif
      m_pDLL(pDLL),
      m_pManager(&(CZNC::Get().GetManager())),
      m_pUser(pUser),
      m_pNetwork(pNetwork),
      m_pClient(nullptr),
      m_sModName(sModName),
      m_sDataDir(sDataDir),
      m_sSavePath(""),
      m_sArgs(""),
      m_sModPath(""),
      m_Translation("znc-" + sModName),
      m_mssRegistry(),
      m_vSubPages(),
      m_mCommands() {
    if (m_pNetwork) {
        m_sSavePath = m_pNetwork->GetNetworkPath() + "/moddata/" + m_sModName;
    } else if (m_pUser) {
        m_sSavePath = m_pUser->GetUserPath() + "/moddata/" + m_sModName;
    } else {
        m_sSavePath = CZNC::Get().GetZNCPath() + "/moddata/" + m_sModName;
    }
    LoadRegistry();
}

CModule::~CModule() {
    while (!m_sTimers.empty()) {
        RemTimer(*m_sTimers.begin());
    }

    while (!m_sSockets.empty()) {
        RemSocket(*m_sSockets.begin());
    }

    SaveRegistry();

#ifdef HAVE_PTHREAD
    CancelJobs(m_sJobs);
#endif
}

void CModule::SetUser(CUser* pUser) { m_pUser = pUser; }
void CModule::SetNetwork(CIRCNetwork* pNetwork) { m_pNetwork = pNetwork; }
void CModule::SetClient(CClient* pClient) { m_pClient = pClient; }

CString CModule::ExpandString(const CString& sStr) const {
    CString sRet;
    return ExpandString(sStr, sRet);
}

CString& CModule::ExpandString(const CString& sStr, CString& sRet) const {
    sRet = sStr;

    if (m_pNetwork) {
        return m_pNetwork->ExpandString(sRet, sRet);
    }

    if (m_pUser) {
        return m_pUser->ExpandString(sRet, sRet);
    }

    return sRet;
}

const CString& CModule::GetSavePath() const {
    if (!CFile::Exists(m_sSavePath)) {
        CDir::MakeDir(m_sSavePath);
    }
    return m_sSavePath;
}

CString CModule::GetWebPath() {
    switch (m_eType) {
        case CModInfo::GlobalModule:
            return "/mods/global/" + GetModName() + "/";
        case CModInfo::UserModule:
            return "/mods/user/" + GetModName() + "/";
        case CModInfo::NetworkModule:
            return "/mods/network/" + m_pNetwork->GetName() + "/" +
                   GetModName() + "/";
        default:
            return "/";
    }
}

CString CModule::GetWebFilesPath() {
    switch (m_eType) {
        case CModInfo::GlobalModule:
            return "/modfiles/global/" + GetModName() + "/";
        case CModInfo::UserModule:
            return "/modfiles/user/" + GetModName() + "/";
        case CModInfo::NetworkModule:
            return "/modfiles/network/" + m_pNetwork->GetName() + "/" +
                   GetModName() + "/";
        default:
            return "/";
    }
}

bool CModule::LoadRegistry() {
    // CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";
    return (m_mssRegistry.ReadFromDisk(GetSavePath() + "/.registry") ==
            MCString::MCS_SUCCESS);
}

bool CModule::SaveRegistry() const {
    // CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";
    return (m_mssRegistry.WriteToDisk(GetSavePath() + "/.registry", 0600) ==
            MCString::MCS_SUCCESS);
}

bool CModule::MoveRegistry(const CString& sPath) {
    if (m_sSavePath != sPath) {
        CFile fOldNVFile = CFile(m_sSavePath + "/.registry");
        if (!fOldNVFile.Exists()) {
            return false;
        }
        if (!CFile::Exists(sPath) && !CDir::MakeDir(sPath)) {
            return false;
        }
        fOldNVFile.Copy(sPath + "/.registry");
        m_sSavePath = sPath;
        return true;
    }
    return false;
}

bool CModule::SetNV(const CString& sName, const CString& sValue,
                    bool bWriteToDisk) {
    m_mssRegistry[sName] = sValue;
    if (bWriteToDisk) {
        return SaveRegistry();
    }

    return true;
}

CString CModule::GetNV(const CString& sName) const {
    MCString::const_iterator it = m_mssRegistry.find(sName);

    if (it != m_mssRegistry.end()) {
        return it->second;
    }

    return "";
}

bool CModule::DelNV(const CString& sName, bool bWriteToDisk) {
    MCString::iterator it = m_mssRegistry.find(sName);

    if (it != m_mssRegistry.end()) {
        m_mssRegistry.erase(it);
    } else {
        return false;
    }

    if (bWriteToDisk) {
        return SaveRegistry();
    }

    return true;
}

bool CModule::ClearNV(bool bWriteToDisk) {
    m_mssRegistry.clear();

    if (bWriteToDisk) {
        return SaveRegistry();
    }
    return true;
}

bool CModule::AddTimer(CTimer* pTimer) {
    if ((!pTimer) ||
        (!pTimer->GetName().empty() && FindTimer(pTimer->GetName()))) {
        delete pTimer;
        return false;
    }

    if (!m_sTimers.insert(pTimer).second)
        // Was already added
        return true;

    m_pManager->AddCron(pTimer);
    return true;
}

bool CModule::AddTimer(FPTimer_t pFBCallback, const CString& sLabel,
                       u_int uInterval, u_int uCycles,
                       const CString& sDescription) {
    CFPTimer* pTimer =
        new CFPTimer(this, uInterval, uCycles, sLabel, sDescription);
    pTimer->SetFPCallback(pFBCallback);

    return AddTimer(pTimer);
}

bool CModule::RemTimer(CTimer* pTimer) {
    if (m_sTimers.erase(pTimer) == 0) return false;
    m_pManager->DelCronByAddr(pTimer);
    return true;
}

bool CModule::RemTimer(const CString& sLabel) {
    CTimer* pTimer = FindTimer(sLabel);
    if (!pTimer) return false;
    return RemTimer(pTimer);
}

bool CModule::UnlinkTimer(CTimer* pTimer) { return m_sTimers.erase(pTimer); }

CTimer* CModule::FindTimer(const CString& sLabel) {
    if (sLabel.empty()) {
        return nullptr;
    }

    for (CTimer* pTimer : m_sTimers) {
        if (pTimer->GetName().Equals(sLabel)) {
            return pTimer;
        }
    }

    return nullptr;
}

void CModule::ListTimers() {
    if (m_sTimers.empty()) {
        PutModule("You have no timers running.");
        return;
    }

    CTable Table;
    Table.AddColumn("Name");
    Table.AddColumn("Secs");
    Table.AddColumn("Cycles");
    Table.AddColumn("Description");

    for (const CTimer* pTimer : m_sTimers) {
        unsigned int uCycles = pTimer->GetCyclesLeft();
        timeval Interval = pTimer->GetInterval();

        Table.AddRow();
        Table.SetCell("Name", pTimer->GetName());
        Table.SetCell(
            "Secs", CString(Interval.tv_sec) + "seconds" +
                        (Interval.tv_usec
                             ? " " + CString(Interval.tv_usec) + " microseconds"
                             : ""));
        Table.SetCell("Cycles", ((uCycles) ? CString(uCycles) : "INF"));
        Table.SetCell("Description", pTimer->GetDescription());
    }

    PutModule(Table);
}

bool CModule::AddSocket(CSocket* pSocket) {
    if (!pSocket) {
        return false;
    }

    m_sSockets.insert(pSocket);
    return true;
}

bool CModule::RemSocket(CSocket* pSocket) {
    if (m_sSockets.erase(pSocket)) {
        m_pManager->DelSockByAddr(pSocket);
        return true;
    }

    return false;
}

bool CModule::RemSocket(const CString& sSockName) {
    for (CSocket* pSocket : m_sSockets) {
        if (pSocket->GetSockName().Equals(sSockName)) {
            m_sSockets.erase(pSocket);
            m_pManager->DelSockByAddr(pSocket);
            return true;
        }
    }

    return false;
}

bool CModule::UnlinkSocket(CSocket* pSocket) {
    return m_sSockets.erase(pSocket);
}

CSocket* CModule::FindSocket(const CString& sSockName) {
    for (CSocket* pSocket : m_sSockets) {
        if (pSocket->GetSockName().Equals(sSockName)) {
            return pSocket;
        }
    }

    return nullptr;
}

void CModule::ListSockets() {
    if (m_sSockets.empty()) {
        PutModule("You have no open sockets.");
        return;
    }

    CTable Table;
    Table.AddColumn("Name");
    Table.AddColumn("State");
    Table.AddColumn("LocalPort");
    Table.AddColumn("SSL");
    Table.AddColumn("RemoteIP");
    Table.AddColumn("RemotePort");

    for (const CSocket* pSocket : m_sSockets) {
        Table.AddRow();
        Table.SetCell("Name", pSocket->GetSockName());

        if (pSocket->GetType() == CSocket::LISTENER) {
            Table.SetCell("State", "Listening");
        } else {
            Table.SetCell("State", (pSocket->IsConnected() ? "Connected" : ""));
        }

        Table.SetCell("LocalPort", CString(pSocket->GetLocalPort()));
        Table.SetCell("SSL", (pSocket->GetSSL() ? "yes" : "no"));
        Table.SetCell("RemoteIP", pSocket->GetRemoteIP());
        Table.SetCell("RemotePort", (pSocket->GetRemotePort())
                                        ? CString(pSocket->GetRemotePort())
                                        : CString(""));
    }

    PutModule(Table);
}

#ifdef HAVE_PTHREAD
CModuleJob::~CModuleJob() { m_pModule->UnlinkJob(this); }

void CModule::AddJob(CModuleJob* pJob) {
    CThreadPool::Get().addJob(pJob);
    m_sJobs.insert(pJob);
}

void CModule::CancelJob(CModuleJob* pJob) {
    if (pJob == nullptr) return;
    // Destructor calls UnlinkJob and removes the job from m_sJobs
    CThreadPool::Get().cancelJob(pJob);
}

bool CModule::CancelJob(const CString& sJobName) {
    for (CModuleJob* pJob : m_sJobs) {
        if (pJob->GetName().Equals(sJobName)) {
            CancelJob(pJob);
            return true;
        }
    }
    return false;
}

void CModule::CancelJobs(const std::set<CModuleJob*>& sJobs) {
    set<CJob*> sPlainJobs(sJobs.begin(), sJobs.end());

    // Destructor calls UnlinkJob and removes the jobs from m_sJobs
    CThreadPool::Get().cancelJobs(sPlainJobs);
}

bool CModule::UnlinkJob(CModuleJob* pJob) { return 0 != m_sJobs.erase(pJob); }
#endif

bool CModule::AddCommand(const CModCommand& Command) {
    if (Command.GetFunction() == nullptr) return false;
    if (Command.GetCommand().Contains(" ")) return false;
    if (FindCommand(Command.GetCommand()) != nullptr) return false;

    m_mCommands[Command.GetCommand()] = Command;
    return true;
}

bool CModule::AddCommand(const CString& sCmd, CModCommand::ModCmdFunc func,
                         const CString& sArgs, const CString& sDesc) {
    CModCommand cmd(sCmd, this, func, sArgs, sDesc);
    return AddCommand(cmd);
}

bool CModule::AddCommand(const CString& sCmd, const COptionalTranslation& Args,
                         const COptionalTranslation& Desc,
                         std::function<void(const CString& sLine)> func) {
    CModCommand cmd(sCmd, std::move(func), Args, Desc);
    return AddCommand(std::move(cmd));
}

void CModule::AddHelpCommand() {
    AddCommand("Help", t_d("<search>", "modhelpcmd"),
               t_d("Generate this output", "modhelpcmd"),
               [=](const CString& sLine) { HandleHelpCommand(sLine); });
}

bool CModule::RemCommand(const CString& sCmd) {
    return m_mCommands.erase(sCmd) > 0;
}

const CModCommand* CModule::FindCommand(const CString& sCmd) const {
    for (const auto& it : m_mCommands) {
        if (!it.first.Equals(sCmd)) continue;
        return &it.second;
    }
    return nullptr;
}

bool CModule::HandleCommand(const CString& sLine) {
    const CString& sCmd = sLine.Token(0);
    const CModCommand* pCmd = FindCommand(sCmd);

    if (pCmd) {
        pCmd->Call(sLine);
        return true;
    }

    OnUnknownModCommand(sLine);

    return false;
}

void CModule::HandleHelpCommand(const CString& sLine) {
    CString sFilter = sLine.Token(1).AsLower();
    CTable Table;

    CModCommand::InitHelp(Table);
    for (const auto& it : m_mCommands) {
        CString sCmd = it.second.GetCommand().AsLower();
        if (sFilter.empty() ||
            (sCmd.StartsWith(sFilter, CString::CaseSensitive)) ||
            sCmd.WildCmp(sFilter)) {
            it.second.AddHelp(Table);
        }
    }
    if (Table.empty()) {
        PutModule(t_f("No matches for '{1}'")(sFilter));
    } else {
        PutModule(Table);
    }
}

CString CModule::GetModNick() const {
    return ((m_pUser) ? m_pUser->GetStatusPrefix() : "*") + m_sModName;
}

// Webmods
bool CModule::OnWebPreRequest(CWebSock& WebSock, const CString& sPageName) {
    return false;
}
bool CModule::OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                           CTemplate& Tmpl) {
    return false;
}
bool CModule::ValidateWebRequestCSRFCheck(CWebSock& WebSock,
    const CString& sPageName) {
    return WebSock.ValidateCSRFCheck(WebSock.GetURI());
}
bool CModule::OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName,
                                   CTemplate& Tmpl) {
    return false;
}
// !Webmods

bool CModule::OnLoad(const CString& sArgs, CString& sMessage) {
    sMessage = "";
    return true;
}
bool CModule::OnBoot() { return true; }
void CModule::OnPreRehash() {}
void CModule::OnPostRehash() {}
void CModule::OnIRCDisconnected() {}
void CModule::OnIRCConnected() {}
CModule::EModRet CModule::OnIRCConnecting(CIRCSock* IRCSock) {
    return CONTINUE;
}
void CModule::OnIRCConnectionError(CIRCSock* IRCSock) {}
CModule::EModRet CModule::OnIRCRegistration(CString& sPass, CString& sNick,
                                            CString& sIdent,
                                            CString& sRealName) {
    return CONTINUE;
}
CModule::EModRet CModule::OnBroadcast(CString& sMessage) { return CONTINUE; }

void CModule::OnChanPermission3(const CNick* pOpNick, const CNick& Nick,
                                CChan& Channel, char cMode,
                                bool bAdded, bool bNoChange) {
    OnChanPermission2(pOpNick, Nick, Channel, cMode, bAdded, bNoChange);
}
void CModule::OnChanPermission2(const CNick* pOpNick, const CNick& Nick,
                                CChan& Channel, unsigned char uMode,
                                bool bAdded, bool bNoChange) {
    if (pOpNick)
        OnChanPermission(*pOpNick, Nick, Channel, uMode, bAdded, bNoChange);
}
void CModule::OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                    bool bNoChange) {
    if (pOpNick) OnOp(*pOpNick, Nick, Channel, bNoChange);
}
void CModule::OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                      bool bNoChange) {
    if (pOpNick) OnDeop(*pOpNick, Nick, Channel, bNoChange);
}
void CModule::OnVoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                       bool bNoChange) {
    if (pOpNick) OnVoice(*pOpNick, Nick, Channel, bNoChange);
}
void CModule::OnDevoice2(const CNick* pOpNick, const CNick& Nick,
                         CChan& Channel, bool bNoChange) {
    if (pOpNick) OnDevoice(*pOpNick, Nick, Channel, bNoChange);
}
void CModule::OnRawMode2(const CNick* pOpNick, CChan& Channel,
                         const CString& sModes, const CString& sArgs) {
    if (pOpNick) OnRawMode(*pOpNick, Channel, sModes, sArgs);
}
void CModule::OnMode2(const CNick* pOpNick, CChan& Channel, char uMode,
                      const CString& sArg, bool bAdded, bool bNoChange) {
    if (pOpNick) OnMode(*pOpNick, Channel, uMode, sArg, bAdded, bNoChange);
}

void CModule::OnChanPermission(const CNick& pOpNick, const CNick& Nick,
                               CChan& Channel, unsigned char uMode, bool bAdded,
                               bool bNoChange) {}
void CModule::OnOp(const CNick& pOpNick, const CNick& Nick, CChan& Channel,
                   bool bNoChange) {}
void CModule::OnDeop(const CNick& pOpNick, const CNick& Nick, CChan& Channel,
                     bool bNoChange) {}
void CModule::OnVoice(const CNick& pOpNick, const CNick& Nick, CChan& Channel,
                      bool bNoChange) {}
void CModule::OnDevoice(const CNick& pOpNick, const CNick& Nick, CChan& Channel,
                        bool bNoChange) {}
void CModule::OnRawMode(const CNick& pOpNick, CChan& Channel,
                        const CString& sModes, const CString& sArgs) {}
void CModule::OnMode(const CNick& pOpNick, CChan& Channel, char uMode,
                     const CString& sArg, bool bAdded, bool bNoChange) {}

CModule::EModRet CModule::OnRaw(CString& sLine) { return CONTINUE; }
CModule::EModRet CModule::OnRawMessage(CMessage& Message) { return CONTINUE; }
CModule::EModRet CModule::OnNumericMessage(CNumericMessage& Message) {
    return CONTINUE;
}

CModule::EModRet CModule::OnStatusCommand(CString& sCommand) {
    return CONTINUE;
}
void CModule::OnModNotice(const CString& sMessage) {}
void CModule::OnModCTCP(const CString& sMessage) {}

void CModule::OnModCommand(const CString& sCommand) { HandleCommand(sCommand); }
void CModule::OnUnknownModCommand(const CString& sLine) {
    if (m_mCommands.empty())
        // This function is only called if OnModCommand wasn't
        // overriden, so no false warnings for modules which don't use
        // CModCommand for command handling.
        PutModule(t_s("This module doesn't implement any commands."));
    else
        PutModule(t_s("Unknown command!"));
}

void CModule::OnQuit(const CNick& Nick, const CString& sMessage,
                     const vector<CChan*>& vChans) {}
void CModule::OnQuitMessage(CQuitMessage& Message,
                            const vector<CChan*>& vChans) {
    OnQuit(Message.GetNick(), Message.GetReason(), vChans);
}
void CModule::OnNick(const CNick& Nick, const CString& sNewNick,
                     const vector<CChan*>& vChans) {}
void CModule::OnNickMessage(CNickMessage& Message,
                            const vector<CChan*>& vChans) {
    OnNick(Message.GetNick(), Message.GetNewNick(), vChans);
}
void CModule::OnKick(const CNick& Nick, const CString& sKickedNick,
                     CChan& Channel, const CString& sMessage) {}
void CModule::OnKickMessage(CKickMessage& Message) {
    OnKick(Message.GetNick(), Message.GetKickedNick(), *Message.GetChan(),
           Message.GetReason());
}
CModule::EModRet CModule::OnJoining(CChan& Channel) { return CONTINUE; }
void CModule::OnJoin(const CNick& Nick, CChan& Channel) {}
void CModule::OnJoinMessage(CJoinMessage& Message) {
    OnJoin(Message.GetNick(), *Message.GetChan());
}
void CModule::OnPart(const CNick& Nick, CChan& Channel,
                     const CString& sMessage) {}
void CModule::OnPartMessage(CPartMessage& Message) {
    OnPart(Message.GetNick(), *Message.GetChan(), Message.GetReason());
}
CModule::EModRet CModule::OnInvite(const CNick& Nick, const CString& sChan) {
    return CONTINUE;
}

CModule::EModRet CModule::OnChanBufferStarting(CChan& Chan, CClient& Client) {
    return CONTINUE;
}
CModule::EModRet CModule::OnChanBufferEnding(CChan& Chan, CClient& Client) {
    return CONTINUE;
}
CModule::EModRet CModule::OnChanBufferPlayLine(CChan& Chan, CClient& Client,
                                               CString& sLine) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivBufferStarting(CQuery& Query, CClient& Client) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivBufferEnding(CQuery& Query, CClient& Client) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivBufferPlayLine(CClient& Client,
                                               CString& sLine) {
    return CONTINUE;
}

CModule::EModRet CModule::OnChanBufferPlayLine2(CChan& Chan, CClient& Client,
                                                CString& sLine,
                                                const timeval& tv) {
    return OnChanBufferPlayLine(Chan, Client, sLine);
}
CModule::EModRet CModule::OnPrivBufferPlayLine2(CClient& Client, CString& sLine,
                                                const timeval& tv) {
    return OnPrivBufferPlayLine(Client, sLine);
}

CModule::EModRet CModule::OnChanBufferPlayMessage(CMessage& Message) {
    CString sOriginal, sModified;
    sOriginal = sModified = Message.ToString(CMessage::ExcludeTags);
    EModRet ret = OnChanBufferPlayLine2(
        *Message.GetChan(), *Message.GetClient(), sModified, Message.GetTime());
    if (sOriginal != sModified) {
        Message.Parse(sModified);
    }
    return ret;
}
CModule::EModRet CModule::OnPrivBufferPlayMessage(CMessage& Message) {
    CString sOriginal, sModified;
    sOriginal = sModified = Message.ToString(CMessage::ExcludeTags);
    EModRet ret = OnPrivBufferPlayLine2(*Message.GetClient(), sModified,
                                        Message.GetTime());
    if (sOriginal != sModified) {
        Message.Parse(sModified);
    }
    return ret;
}

void CModule::OnClientLogin() {}
void CModule::OnClientDisconnect() {}
CModule::EModRet CModule::OnUserRaw(CString& sLine) { return CONTINUE; }
CModule::EModRet CModule::OnUserRawMessage(CMessage& Message) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserCTCPReply(CString& sTarget, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserCTCPReplyMessage(CCTCPMessage& Message) {
    CString sTarget = Message.GetTarget();
    CString sText = Message.GetText();
    EModRet ret = OnUserCTCPReply(sTarget, sText);
    Message.SetTarget(sTarget);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnUserCTCP(CString& sTarget, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserCTCPMessage(CCTCPMessage& Message) {
    CString sTarget = Message.GetTarget();
    CString sText = Message.GetText();
    EModRet ret = OnUserCTCP(sTarget, sText);
    Message.SetTarget(sTarget);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnUserAction(CString& sTarget, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserActionMessage(CActionMessage& Message) {
    CString sTarget = Message.GetTarget();
    CString sText = Message.GetText();
    EModRet ret = OnUserAction(sTarget, sText);
    Message.SetTarget(sTarget);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnUserMsg(CString& sTarget, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserTextMessage(CTextMessage& Message) {
    CString sTarget = Message.GetTarget();
    CString sText = Message.GetText();
    EModRet ret = OnUserMsg(sTarget, sText);
    Message.SetTarget(sTarget);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnUserNotice(CString& sTarget, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserNoticeMessage(CNoticeMessage& Message) {
    CString sTarget = Message.GetTarget();
    CString sText = Message.GetText();
    EModRet ret = OnUserNotice(sTarget, sText);
    Message.SetTarget(sTarget);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnUserJoin(CString& sChannel, CString& sKey) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserJoinMessage(CJoinMessage& Message) {
    CString sChan = Message.GetTarget();
    CString sKey = Message.GetKey();
    EModRet ret = OnUserJoin(sChan, sKey);
    Message.SetTarget(sChan);
    Message.SetKey(sKey);
    return ret;
}
CModule::EModRet CModule::OnUserPart(CString& sChannel, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserPartMessage(CPartMessage& Message) {
    CString sChan = Message.GetTarget();
    CString sReason = Message.GetReason();
    EModRet ret = OnUserPart(sChan, sReason);
    Message.SetTarget(sChan);
    Message.SetReason(sReason);
    return ret;
}
CModule::EModRet CModule::OnUserTopic(CString& sChannel, CString& sTopic) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserTopicMessage(CTopicMessage& Message) {
    CString sChan = Message.GetTarget();
    CString sTopic = Message.GetTopic();
    EModRet ret = OnUserTopic(sChan, sTopic);
    Message.SetTarget(sChan);
    Message.SetTopic(sTopic);
    return ret;
}
CModule::EModRet CModule::OnUserTopicRequest(CString& sChannel) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUserQuit(CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserQuitMessage(CQuitMessage& Message) {
    CString sReason = Message.GetReason();
    EModRet ret = OnUserQuit(sReason);
    Message.SetReason(sReason);
    return ret;
}

CModule::EModRet CModule::OnCTCPReply(CNick& Nick, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnCTCPReplyMessage(CCTCPMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnCTCPReply(Message.GetNick(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnPrivCTCP(CNick& Nick, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivCTCPMessage(CCTCPMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnPrivCTCP(Message.GetNick(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnChanCTCP(CNick& Nick, CChan& Channel,
                                     CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnChanCTCPMessage(CCTCPMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnChanCTCP(Message.GetNick(), *Message.GetChan(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnPrivAction(CNick& Nick, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivActionMessage(CActionMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnPrivAction(Message.GetNick(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnChanAction(CNick& Nick, CChan& Channel,
                                       CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnChanActionMessage(CActionMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnChanAction(Message.GetNick(), *Message.GetChan(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnPrivMsg(CNick& Nick, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivTextMessage(CTextMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnPrivMsg(Message.GetNick(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnChanMsg(CNick& Nick, CChan& Channel,
                                    CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnChanTextMessage(CTextMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnChanMsg(Message.GetNick(), *Message.GetChan(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnPrivNotice(CNick& Nick, CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnPrivNoticeMessage(CNoticeMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnPrivNotice(Message.GetNick(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnChanNotice(CNick& Nick, CChan& Channel,
                                       CString& sMessage) {
    return CONTINUE;
}
CModule::EModRet CModule::OnChanNoticeMessage(CNoticeMessage& Message) {
    CString sText = Message.GetText();
    EModRet ret = OnChanNotice(Message.GetNick(), *Message.GetChan(), sText);
    Message.SetText(sText);
    return ret;
}
CModule::EModRet CModule::OnTopic(CNick& Nick, CChan& Channel,
                                  CString& sTopic) {
    return CONTINUE;
}
CModule::EModRet CModule::OnTopicMessage(CTopicMessage& Message) {
    CString sTopic = Message.GetTopic();
    EModRet ret = OnTopic(Message.GetNick(), *Message.GetChan(), sTopic);
    Message.SetTopic(sTopic);
    return ret;
}
CModule::EModRet CModule::OnTimerAutoJoin(CChan& Channel) { return CONTINUE; }
CModule::EModRet CModule::OnAddNetwork(CIRCNetwork& Network,
                                       CString& sErrorRet) {
    return CONTINUE;
}
CModule::EModRet CModule::OnDeleteNetwork(CIRCNetwork& Network) {
    return CONTINUE;
}

CModule::EModRet CModule::OnSendToClient(CString& sLine, CClient& Client) {
    return CONTINUE;
}
CModule::EModRet CModule::OnSendToClientMessage(CMessage& Message) {
    return CONTINUE;
}

CModule::EModRet CModule::OnSendToIRC(CString& sLine) { return CONTINUE; }
CModule::EModRet CModule::OnSendToIRCMessage(CMessage& Message) {
    return CONTINUE;
}

bool CModule::OnServerCapAvailable(const CString& sCap) { return false; }
void CModule::OnServerCapResult(const CString& sCap, bool bSuccess) {}

bool CModule::PutIRC(const CString& sLine) {
    return m_pNetwork ? m_pNetwork->PutIRC(sLine) : false;
}
bool CModule::PutIRC(const CMessage& Message) {
    return m_pNetwork ? m_pNetwork->PutIRC(Message) : false;
}
bool CModule::PutUser(const CString& sLine) {
    return m_pNetwork ? m_pNetwork->PutUser(sLine, m_pClient) : false;
}
bool CModule::PutStatus(const CString& sLine) {
    return m_pNetwork ? m_pNetwork->PutStatus(sLine, m_pClient) : false;
}
unsigned int CModule::PutModule(const CTable& table) {
    if (!m_pUser) return 0;

    unsigned int idx = 0;
    CString sLine;
    while (table.GetLine(idx++, sLine)) PutModule(sLine);
    return idx - 1;
}
bool CModule::PutModule(const CString& sLine) {
    if (m_pClient) {
        m_pClient->PutModule(GetModName(), sLine);
        return true;
    }

    if (m_pNetwork) {
        return m_pNetwork->PutModule(GetModName(), sLine);
    }

    if (m_pUser) {
        return m_pUser->PutModule(GetModName(), sLine);
    }

    return false;
}
bool CModule::PutModNotice(const CString& sLine) {
    if (!m_pUser) return false;

    if (m_pClient) {
        m_pClient->PutModNotice(GetModName(), sLine);
        return true;
    }

    return m_pUser->PutModNotice(GetModName(), sLine);
}

///////////////////
// Global Module //
///////////////////
CModule::EModRet CModule::OnAddUser(CUser& User, CString& sErrorRet) {
    return CONTINUE;
}
CModule::EModRet CModule::OnDeleteUser(CUser& User) { return CONTINUE; }
void CModule::OnClientConnect(CZNCSock* pClient, const CString& sHost,
                              unsigned short uPort) {}
CModule::EModRet CModule::OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) {
    return CONTINUE;
}
void CModule::OnFailedLogin(const CString& sUsername,
                            const CString& sRemoteIP) {}
CModule::EModRet CModule::OnUnknownUserRaw(CClient* pClient, CString& sLine) {
    return CONTINUE;
}
CModule::EModRet CModule::OnUnknownUserRawMessage(CMessage& Message) {
    return CONTINUE;
}
void CModule::OnClientCapLs(CClient* pClient, SCString& ssCaps) {}
bool CModule::IsClientCapSupported(CClient* pClient, const CString& sCap,
                                   bool bState) {
    return false;
}
void CModule::OnClientCapRequest(CClient* pClient, const CString& sCap,
                                 bool bState) {}
CModule::EModRet CModule::OnModuleLoading(const CString& sModName,
                                          const CString& sArgs,
                                          CModInfo::EModuleType eType,
                                          bool& bSuccess, CString& sRetMsg) {
    return CONTINUE;
}
CModule::EModRet CModule::OnModuleUnloading(CModule* pModule, bool& bSuccess,
                                            CString& sRetMsg) {
    return CONTINUE;
}
CModule::EModRet CModule::OnGetModInfo(CModInfo& ModInfo,
                                       const CString& sModule, bool& bSuccess,
                                       CString& sRetMsg) {
    return CONTINUE;
}
void CModule::OnGetAvailableMods(set<CModInfo>& ssMods,
                                 CModInfo::EModuleType eType) {}

CModules::CModules()
    : m_pUser(nullptr), m_pNetwork(nullptr), m_pClient(nullptr) {}

CModules::~CModules() { UnloadAll(); }

void CModules::UnloadAll() {
    while (size()) {
        CString sRetMsg;
        CString sModName = back()->GetModName();
        UnloadModule(sModName, sRetMsg);
    }
}

bool CModules::OnBoot() {
    for (CModule* pMod : *this) {
        try {
            if (!pMod->OnBoot()) {
                return true;
            }
        } catch (const CModule::EModException& e) {
            if (e == CModule::UNLOAD) {
                UnloadModule(pMod->GetModName());
            }
        }
    }

    return false;
}

bool CModules::OnPreRehash() {
    MODUNLOADCHK(OnPreRehash());
    return false;
}
bool CModules::OnPostRehash() {
    MODUNLOADCHK(OnPostRehash());
    return false;
}
bool CModules::OnIRCConnected() {
    MODUNLOADCHK(OnIRCConnected());
    return false;
}
bool CModules::OnIRCConnecting(CIRCSock* pIRCSock) {
    MODHALTCHK(OnIRCConnecting(pIRCSock));
}
bool CModules::OnIRCConnectionError(CIRCSock* pIRCSock) {
    MODUNLOADCHK(OnIRCConnectionError(pIRCSock));
    return false;
}
bool CModules::OnIRCRegistration(CString& sPass, CString& sNick,
                                 CString& sIdent, CString& sRealName) {
    MODHALTCHK(OnIRCRegistration(sPass, sNick, sIdent, sRealName));
}
bool CModules::OnBroadcast(CString& sMessage) {
    MODHALTCHK(OnBroadcast(sMessage));
}
bool CModules::OnIRCDisconnected() {
    MODUNLOADCHK(OnIRCDisconnected());
    return false;
}

bool CModules::OnChanPermission3(const CNick* pOpNick, const CNick& Nick,
                                 CChan& Channel, char cMode,
                                 bool bAdded, bool bNoChange) {
    MODUNLOADCHK(
        OnChanPermission3(pOpNick, Nick, Channel, cMode, bAdded, bNoChange));
    return false;
}
bool CModules::OnChanPermission2(const CNick* pOpNick, const CNick& Nick,
                                 CChan& Channel, unsigned char uMode,
                                 bool bAdded, bool bNoChange) {
    MODUNLOADCHK(
        OnChanPermission2(pOpNick, Nick, Channel, uMode, bAdded, bNoChange));
    return false;
}
bool CModules::OnChanPermission(const CNick& OpNick, const CNick& Nick,
                                CChan& Channel, unsigned char uMode,
                                bool bAdded, bool bNoChange) {
    MODUNLOADCHK(
        OnChanPermission(OpNick, Nick, Channel, uMode, bAdded, bNoChange));
    return false;
}
bool CModules::OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                     bool bNoChange) {
    MODUNLOADCHK(OnOp2(pOpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                    bool bNoChange) {
    MODUNLOADCHK(OnOp(OpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                       bool bNoChange) {
    MODUNLOADCHK(OnDeop2(pOpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                      bool bNoChange) {
    MODUNLOADCHK(OnDeop(OpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnVoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                        bool bNoChange) {
    MODUNLOADCHK(OnVoice2(pOpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                       bool bNoChange) {
    MODUNLOADCHK(OnVoice(OpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnDevoice2(const CNick* pOpNick, const CNick& Nick,
                          CChan& Channel, bool bNoChange) {
    MODUNLOADCHK(OnDevoice2(pOpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                         bool bNoChange) {
    MODUNLOADCHK(OnDevoice(OpNick, Nick, Channel, bNoChange));
    return false;
}
bool CModules::OnRawMode2(const CNick* pOpNick, CChan& Channel,
                          const CString& sModes, const CString& sArgs) {
    MODUNLOADCHK(OnRawMode2(pOpNick, Channel, sModes, sArgs));
    return false;
}
bool CModules::OnRawMode(const CNick& OpNick, CChan& Channel,
                         const CString& sModes, const CString& sArgs) {
    MODUNLOADCHK(OnRawMode(OpNick, Channel, sModes, sArgs));
    return false;
}
bool CModules::OnMode2(const CNick* pOpNick, CChan& Channel, char uMode,
                       const CString& sArg, bool bAdded, bool bNoChange) {
    MODUNLOADCHK(OnMode2(pOpNick, Channel, uMode, sArg, bAdded, bNoChange));
    return false;
}
bool CModules::OnMode(const CNick& OpNick, CChan& Channel, char uMode,
                      const CString& sArg, bool bAdded, bool bNoChange) {
    MODUNLOADCHK(OnMode(OpNick, Channel, uMode, sArg, bAdded, bNoChange));
    return false;
}
bool CModules::OnRaw(CString& sLine) { MODHALTCHK(OnRaw(sLine)); }
bool CModules::OnRawMessage(CMessage& Message) {
    MODHALTCHK(OnRawMessage(Message));
}
bool CModules::OnNumericMessage(CNumericMessage& Message) {
    MODHALTCHK(OnNumericMessage(Message));
}

bool CModules::OnClientLogin() {
    MODUNLOADCHK(OnClientLogin());
    return false;
}
bool CModules::OnClientDisconnect() {
    MODUNLOADCHK(OnClientDisconnect());
    return false;
}
bool CModules::OnUserRaw(CString& sLine) { MODHALTCHK(OnUserRaw(sLine)); }
bool CModules::OnUserRawMessage(CMessage& Message) {
    MODHALTCHK(OnUserRawMessage(Message));
}
bool CModules::OnUserCTCPReply(CString& sTarget, CString& sMessage) {
    MODHALTCHK(OnUserCTCPReply(sTarget, sMessage));
}
bool CModules::OnUserCTCPReplyMessage(CCTCPMessage& Message) {
    MODHALTCHK(OnUserCTCPReplyMessage(Message));
}
bool CModules::OnUserCTCP(CString& sTarget, CString& sMessage) {
    MODHALTCHK(OnUserCTCP(sTarget, sMessage));
}
bool CModules::OnUserCTCPMessage(CCTCPMessage& Message) {
    MODHALTCHK(OnUserCTCPMessage(Message));
}
bool CModules::OnUserAction(CString& sTarget, CString& sMessage) {
    MODHALTCHK(OnUserAction(sTarget, sMessage));
}
bool CModules::OnUserActionMessage(CActionMessage& Message) {
    MODHALTCHK(OnUserActionMessage(Message));
}
bool CModules::OnUserMsg(CString& sTarget, CString& sMessage) {
    MODHALTCHK(OnUserMsg(sTarget, sMessage));
}
bool CModules::OnUserTextMessage(CTextMessage& Message) {
    MODHALTCHK(OnUserTextMessage(Message));
}
bool CModules::OnUserNotice(CString& sTarget, CString& sMessage) {
    MODHALTCHK(OnUserNotice(sTarget, sMessage));
}
bool CModules::OnUserNoticeMessage(CNoticeMessage& Message) {
    MODHALTCHK(OnUserNoticeMessage(Message));
}
bool CModules::OnUserJoin(CString& sChannel, CString& sKey) {
    MODHALTCHK(OnUserJoin(sChannel, sKey));
}
bool CModules::OnUserJoinMessage(CJoinMessage& Message) {
    MODHALTCHK(OnUserJoinMessage(Message));
}
bool CModules::OnUserPart(CString& sChannel, CString& sMessage) {
    MODHALTCHK(OnUserPart(sChannel, sMessage));
}
bool CModules::OnUserPartMessage(CPartMessage& Message) {
    MODHALTCHK(OnUserPartMessage(Message));
}
bool CModules::OnUserTopic(CString& sChannel, CString& sTopic) {
    MODHALTCHK(OnUserTopic(sChannel, sTopic));
}
bool CModules::OnUserTopicMessage(CTopicMessage& Message) {
    MODHALTCHK(OnUserTopicMessage(Message));
}
bool CModules::OnUserTopicRequest(CString& sChannel) {
    MODHALTCHK(OnUserTopicRequest(sChannel));
}
bool CModules::OnUserQuit(CString& sMessage) {
    MODHALTCHK(OnUserQuit(sMessage));
}
bool CModules::OnUserQuitMessage(CQuitMessage& Message) {
    MODHALTCHK(OnUserQuitMessage(Message));
}

bool CModules::OnQuit(const CNick& Nick, const CString& sMessage,
                      const vector<CChan*>& vChans) {
    MODUNLOADCHK(OnQuit(Nick, sMessage, vChans));
    return false;
}
bool CModules::OnQuitMessage(CQuitMessage& Message,
                             const vector<CChan*>& vChans) {
    MODUNLOADCHK(OnQuitMessage(Message, vChans));
    return false;
}
bool CModules::OnNick(const CNick& Nick, const CString& sNewNick,
                      const vector<CChan*>& vChans) {
    MODUNLOADCHK(OnNick(Nick, sNewNick, vChans));
    return false;
}
bool CModules::OnNickMessage(CNickMessage& Message,
                             const vector<CChan*>& vChans) {
    MODUNLOADCHK(OnNickMessage(Message, vChans));
    return false;
}
bool CModules::OnKick(const CNick& Nick, const CString& sKickedNick,
                      CChan& Channel, const CString& sMessage) {
    MODUNLOADCHK(OnKick(Nick, sKickedNick, Channel, sMessage));
    return false;
}
bool CModules::OnKickMessage(CKickMessage& Message) {
    MODUNLOADCHK(OnKickMessage(Message));
    return false;
}
bool CModules::OnJoining(CChan& Channel) { MODHALTCHK(OnJoining(Channel)); }
bool CModules::OnJoin(const CNick& Nick, CChan& Channel) {
    MODUNLOADCHK(OnJoin(Nick, Channel));
    return false;
}
bool CModules::OnJoinMessage(CJoinMessage& Message) {
    MODUNLOADCHK(OnJoinMessage(Message));
    return false;
}
bool CModules::OnPart(const CNick& Nick, CChan& Channel,
                      const CString& sMessage) {
    MODUNLOADCHK(OnPart(Nick, Channel, sMessage));
    return false;
}
bool CModules::OnPartMessage(CPartMessage& Message) {
    MODUNLOADCHK(OnPartMessage(Message));
    return false;
}
bool CModules::OnInvite(const CNick& Nick, const CString& sChan) {
    MODHALTCHK(OnInvite(Nick, sChan));
}
bool CModules::OnChanBufferStarting(CChan& Chan, CClient& Client) {
    MODHALTCHK(OnChanBufferStarting(Chan, Client));
}
bool CModules::OnChanBufferEnding(CChan& Chan, CClient& Client) {
    MODHALTCHK(OnChanBufferEnding(Chan, Client));
}
bool CModules::OnChanBufferPlayLine2(CChan& Chan, CClient& Client,
                                     CString& sLine, const timeval& tv) {
    MODHALTCHK(OnChanBufferPlayLine2(Chan, Client, sLine, tv));
}
bool CModules::OnChanBufferPlayLine(CChan& Chan, CClient& Client,
                                    CString& sLine) {
    MODHALTCHK(OnChanBufferPlayLine(Chan, Client, sLine));
}
bool CModules::OnPrivBufferStarting(CQuery& Query, CClient& Client) {
    MODHALTCHK(OnPrivBufferStarting(Query, Client));
}
bool CModules::OnPrivBufferEnding(CQuery& Query, CClient& Client) {
    MODHALTCHK(OnPrivBufferEnding(Query, Client));
}
bool CModules::OnPrivBufferPlayLine2(CClient& Client, CString& sLine,
                                     const timeval& tv) {
    MODHALTCHK(OnPrivBufferPlayLine2(Client, sLine, tv));
}
bool CModules::OnPrivBufferPlayLine(CClient& Client, CString& sLine) {
    MODHALTCHK(OnPrivBufferPlayLine(Client, sLine));
}
bool CModules::OnChanBufferPlayMessage(CMessage& Message) {
    MODHALTCHK(OnChanBufferPlayMessage(Message));
}
bool CModules::OnPrivBufferPlayMessage(CMessage& Message) {
    MODHALTCHK(OnPrivBufferPlayMessage(Message));
}
bool CModules::OnCTCPReply(CNick& Nick, CString& sMessage) {
    MODHALTCHK(OnCTCPReply(Nick, sMessage));
}
bool CModules::OnCTCPReplyMessage(CCTCPMessage& Message) {
    MODHALTCHK(OnCTCPReplyMessage(Message));
}
bool CModules::OnPrivCTCP(CNick& Nick, CString& sMessage) {
    MODHALTCHK(OnPrivCTCP(Nick, sMessage));
}
bool CModules::OnPrivCTCPMessage(CCTCPMessage& Message) {
    MODHALTCHK(OnPrivCTCPMessage(Message));
}
bool CModules::OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) {
    MODHALTCHK(OnChanCTCP(Nick, Channel, sMessage));
}
bool CModules::OnChanCTCPMessage(CCTCPMessage& Message) {
    MODHALTCHK(OnChanCTCPMessage(Message));
}
bool CModules::OnPrivAction(CNick& Nick, CString& sMessage) {
    MODHALTCHK(OnPrivAction(Nick, sMessage));
}
bool CModules::OnPrivActionMessage(CActionMessage& Message) {
    MODHALTCHK(OnPrivActionMessage(Message));
}
bool CModules::OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) {
    MODHALTCHK(OnChanAction(Nick, Channel, sMessage));
}
bool CModules::OnChanActionMessage(CActionMessage& Message) {
    MODHALTCHK(OnChanActionMessage(Message));
}
bool CModules::OnPrivMsg(CNick& Nick, CString& sMessage) {
    MODHALTCHK(OnPrivMsg(Nick, sMessage));
}
bool CModules::OnPrivTextMessage(CTextMessage& Message) {
    MODHALTCHK(OnPrivTextMessage(Message));
}
bool CModules::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
    MODHALTCHK(OnChanMsg(Nick, Channel, sMessage));
}
bool CModules::OnChanTextMessage(CTextMessage& Message) {
    MODHALTCHK(OnChanTextMessage(Message));
}
bool CModules::OnPrivNotice(CNick& Nick, CString& sMessage) {
    MODHALTCHK(OnPrivNotice(Nick, sMessage));
}
bool CModules::OnPrivNoticeMessage(CNoticeMessage& Message) {
    MODHALTCHK(OnPrivNoticeMessage(Message));
}
bool CModules::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
    MODHALTCHK(OnChanNotice(Nick, Channel, sMessage));
}
bool CModules::OnChanNoticeMessage(CNoticeMessage& Message) {
    MODHALTCHK(OnChanNoticeMessage(Message));
}
bool CModules::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) {
    MODHALTCHK(OnTopic(Nick, Channel, sTopic));
}
bool CModules::OnTopicMessage(CTopicMessage& Message) {
    MODHALTCHK(OnTopicMessage(Message));
}
bool CModules::OnTimerAutoJoin(CChan& Channel) {
    MODHALTCHK(OnTimerAutoJoin(Channel));
}
bool CModules::OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet) {
    MODHALTCHK(OnAddNetwork(Network, sErrorRet));
}
bool CModules::OnDeleteNetwork(CIRCNetwork& Network) {
    MODHALTCHK(OnDeleteNetwork(Network));
}
bool CModules::OnSendToClient(CString& sLine, CClient& Client) {
    MODHALTCHK(OnSendToClient(sLine, Client));
}
bool CModules::OnSendToClientMessage(CMessage& Message) {
    MODHALTCHK(OnSendToClientMessage(Message));
}
bool CModules::OnSendToIRC(CString& sLine) { MODHALTCHK(OnSendToIRC(sLine)); }
bool CModules::OnSendToIRCMessage(CMessage& Message) {
    MODHALTCHK(OnSendToIRCMessage(Message));
}
bool CModules::OnStatusCommand(CString& sCommand) {
    MODHALTCHK(OnStatusCommand(sCommand));
}
bool CModules::OnModCommand(const CString& sCommand) {
    MODUNLOADCHK(OnModCommand(sCommand));
    return false;
}
bool CModules::OnModNotice(const CString& sMessage) {
    MODUNLOADCHK(OnModNotice(sMessage));
    return false;
}
bool CModules::OnModCTCP(const CString& sMessage) {
    MODUNLOADCHK(OnModCTCP(sMessage));
    return false;
}

// Why MODHALTCHK works only with functions returning EModRet ? :(
bool CModules::OnServerCapAvailable(const CString& sCap) {
    bool bResult = false;
    for (CModule* pMod : *this) {
        try {
            CClient* pOldClient = pMod->GetClient();
            pMod->SetClient(m_pClient);
            if (m_pUser) {
                CUser* pOldUser = pMod->GetUser();
                pMod->SetUser(m_pUser);
                bResult |= pMod->OnServerCapAvailable(sCap);
                pMod->SetUser(pOldUser);
            } else {
                // WTF? Is that possible?
                bResult |= pMod->OnServerCapAvailable(sCap);
            }
            pMod->SetClient(pOldClient);
        } catch (const CModule::EModException& e) {
            if (CModule::UNLOAD == e) {
                UnloadModule(pMod->GetModName());
            }
        }
    }
    return bResult;
}

bool CModules::OnServerCapResult(const CString& sCap, bool bSuccess) {
    MODUNLOADCHK(OnServerCapResult(sCap, bSuccess));
    return false;
}

////////////////////
// Global Modules //
////////////////////
bool CModules::OnAddUser(CUser& User, CString& sErrorRet) {
    MODHALTCHK(OnAddUser(User, sErrorRet));
}

bool CModules::OnDeleteUser(CUser& User) { MODHALTCHK(OnDeleteUser(User)); }

bool CModules::OnClientConnect(CZNCSock* pClient, const CString& sHost,
                               unsigned short uPort) {
    MODUNLOADCHK(OnClientConnect(pClient, sHost, uPort));
    return false;
}

bool CModules::OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) {
    MODHALTCHK(OnLoginAttempt(Auth));
}

bool CModules::OnFailedLogin(const CString& sUsername,
                             const CString& sRemoteIP) {
    MODUNLOADCHK(OnFailedLogin(sUsername, sRemoteIP));
    return false;
}

bool CModules::OnUnknownUserRaw(CClient* pClient, CString& sLine) {
    MODHALTCHK(OnUnknownUserRaw(pClient, sLine));
}

bool CModules::OnUnknownUserRawMessage(CMessage& Message) {
    MODHALTCHK(OnUnknownUserRawMessage(Message));
}

bool CModules::OnClientCapLs(CClient* pClient, SCString& ssCaps) {
    MODUNLOADCHK(OnClientCapLs(pClient, ssCaps));
    return false;
}

// Maybe create new macro for this?
bool CModules::IsClientCapSupported(CClient* pClient, const CString& sCap,
                                    bool bState) {
    bool bResult = false;
    for (CModule* pMod : *this) {
        try {
            CClient* pOldClient = pMod->GetClient();
            pMod->SetClient(m_pClient);
            if (m_pUser) {
                CUser* pOldUser = pMod->GetUser();
                pMod->SetUser(m_pUser);
                bResult |= pMod->IsClientCapSupported(pClient, sCap, bState);
                pMod->SetUser(pOldUser);
            } else {
                // WTF? Is that possible?
                bResult |= pMod->IsClientCapSupported(pClient, sCap, bState);
            }
            pMod->SetClient(pOldClient);
        } catch (const CModule::EModException& e) {
            if (CModule::UNLOAD == e) {
                UnloadModule(pMod->GetModName());
            }
        }
    }
    return bResult;
}

bool CModules::OnClientCapRequest(CClient* pClient, const CString& sCap,
                                  bool bState) {
    MODUNLOADCHK(OnClientCapRequest(pClient, sCap, bState));
    return false;
}

bool CModules::OnModuleLoading(const CString& sModName, const CString& sArgs,
                               CModInfo::EModuleType eType, bool& bSuccess,
                               CString& sRetMsg) {
    MODHALTCHK(OnModuleLoading(sModName, sArgs, eType, bSuccess, sRetMsg));
}

bool CModules::OnModuleUnloading(CModule* pModule, bool& bSuccess,
                                 CString& sRetMsg) {
    MODHALTCHK(OnModuleUnloading(pModule, bSuccess, sRetMsg));
}

bool CModules::OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
                            bool& bSuccess, CString& sRetMsg) {
    MODHALTCHK(OnGetModInfo(ModInfo, sModule, bSuccess, sRetMsg));
}

bool CModules::OnGetAvailableMods(set<CModInfo>& ssMods,
                                  CModInfo::EModuleType eType) {
    MODUNLOADCHK(OnGetAvailableMods(ssMods, eType));
    return false;
}

CModule* CModules::FindModule(const CString& sModule) const {
    for (CModule* pMod : *this) {
        if (sModule.Equals(pMod->GetModName())) {
            return pMod;
        }
    }

    return nullptr;
}

bool CModules::LoadModule(const CString& sModule, const CString& sArgs,
                          CModInfo::EModuleType eType, CUser* pUser,
                          CIRCNetwork* pNetwork, CString& sRetMsg) {
    sRetMsg = "";

    if (FindModule(sModule) != nullptr) {
        sRetMsg = t_f("Module {1} already loaded.")(sModule);
        return false;
    }

    bool bSuccess;
    bool bHandled = false;
    _GLOBALMODULECALL(OnModuleLoading(sModule, sArgs, eType, bSuccess, sRetMsg),
                      pUser, pNetwork, nullptr, &bHandled);
    if (bHandled) return bSuccess;

    CString sModPath, sDataPath;
    CModInfo Info;

    if (!FindModPath(sModule, sModPath, sDataPath)) {
        sRetMsg = t_f("Unable to find module {1}")(sModule);
        return false;
    }
    Info.SetName(sModule);
    Info.SetPath(sModPath);

    ModHandle p = OpenModule(sModule, sModPath, Info, sRetMsg);

    if (!p) return false;

    if (!Info.SupportsType(eType)) {
        dlclose(p);
        sRetMsg = t_f("Module {1} does not support module type {2}.")(
            sModule, CModInfo::ModuleTypeToString(eType));
        return false;
    }

    if (!pUser && eType == CModInfo::UserModule) {
        dlclose(p);
        sRetMsg = t_f("Module {1} requires a user.")(sModule);
        return false;
    }

    if (!pNetwork && eType == CModInfo::NetworkModule) {
        dlclose(p);
        sRetMsg = t_f("Module {1} requires a network.")(sModule);
        return false;
    }

    CModule* pModule =
        Info.GetLoader()(p, pUser, pNetwork, sModule, sDataPath, eType);
    pModule->SetDescription(Info.GetDescription());
    pModule->SetArgs(sArgs);
    pModule->SetModPath(CDir::ChangeDir(CZNC::Get().GetCurPath(), sModPath));
    push_back(pModule);

    bool bLoaded;
    try {
        bLoaded = pModule->OnLoad(sArgs, sRetMsg);
    } catch (const CModule::EModException&) {
        bLoaded = false;
        sRetMsg = t_s("Caught an exception");
    }

    if (!bLoaded) {
        UnloadModule(sModule, sModPath);
        if (!sRetMsg.empty())
            sRetMsg = t_f("Module {1} aborted: {2}")(sModule, sRetMsg);
        else
            sRetMsg = t_f("Module {1} aborted.")(sModule);
        return false;
    }

    if (!sRetMsg.empty()) {
        sRetMsg += " ";
    }
    sRetMsg += "[" + sModPath + "]";
    return true;
}

bool CModules::UnloadModule(const CString& sModule) {
    CString s;
    return UnloadModule(sModule, s);
}

bool CModules::UnloadModule(const CString& sModule, CString& sRetMsg) {
    // Make a copy incase the reference passed in is from CModule::GetModName()
    CString sMod = sModule;

    CModule* pModule = FindModule(sMod);
    sRetMsg = "";

    if (!pModule) {
        sRetMsg = t_f("Module [{1}] not loaded.")(sMod);
        return false;
    }

    bool bSuccess;
    bool bHandled = false;
    _GLOBALMODULECALL(OnModuleUnloading(pModule, bSuccess, sRetMsg),
                      pModule->GetUser(), pModule->GetNetwork(), nullptr,
                      &bHandled);
    if (bHandled) return bSuccess;

    ModHandle p = pModule->GetDLL();

    if (p) {
        delete pModule;

        for (iterator it = begin(); it != end(); ++it) {
            if (*it == pModule) {
                erase(it);
                break;
            }
        }

        dlclose(p);
        sRetMsg = t_f("Module {1} unloaded.")(sMod);

        return true;
    }

    sRetMsg = t_f("Unable to unload module {1}.")(sMod);
    return false;
}

bool CModules::ReloadModule(const CString& sModule, const CString& sArgs,
                            CUser* pUser, CIRCNetwork* pNetwork,
                            CString& sRetMsg) {
    // Make a copy incase the reference passed in is from CModule::GetModName()
    CString sMod = sModule;

    CModule* pModule = FindModule(sMod);

    if (!pModule) {
        sRetMsg = t_f("Module [{1}] not loaded.")(sMod);
        return false;
    }

    CModInfo::EModuleType eType = pModule->GetType();
    pModule = nullptr;

    sRetMsg = "";
    if (!UnloadModule(sMod, sRetMsg)) {
        return false;
    }

    if (!LoadModule(sMod, sArgs, eType, pUser, pNetwork, sRetMsg)) {
        return false;
    }

    sRetMsg = t_f("Reloaded module {1}.")(sMod);
    return true;
}

bool CModules::GetModInfo(CModInfo& ModInfo, const CString& sModule,
                          CString& sRetMsg) {
    CString sModPath, sTmp;

    bool bSuccess;
    bool bHandled = false;
    GLOBALMODULECALL(OnGetModInfo(ModInfo, sModule, bSuccess, sRetMsg),
                     &bHandled);
    if (bHandled) return bSuccess;

    if (!FindModPath(sModule, sModPath, sTmp)) {
        sRetMsg = t_f("Unable to find module {1}.")(sModule);
        return false;
    }

    return GetModPathInfo(ModInfo, sModule, sModPath, sRetMsg);
}

bool CModules::GetModPathInfo(CModInfo& ModInfo, const CString& sModule,
                              const CString& sModPath, CString& sRetMsg) {
    ModInfo.SetName(sModule);
    ModInfo.SetPath(sModPath);

    ModHandle p = OpenModule(sModule, sModPath, ModInfo, sRetMsg);
    if (!p) return false;

    dlclose(p);

    return true;
}

void CModules::GetAvailableMods(set<CModInfo>& ssMods,
                                CModInfo::EModuleType eType) {
    ssMods.clear();

    unsigned int a = 0;
    CDir Dir;

    ModDirList dirs = GetModDirs();

    while (!dirs.empty()) {
        Dir.FillByWildcard(dirs.front().first, "*.so");
        dirs.pop();

        for (a = 0; a < Dir.size(); a++) {
            CFile& File = *Dir[a];
            CString sName = File.GetShortName();
            CString sPath = File.GetLongName();
            CModInfo ModInfo;
            sName.RightChomp(3);

            CString sIgnoreRetMsg;
            if (GetModPathInfo(ModInfo, sName, sPath, sIgnoreRetMsg)) {
                if (ModInfo.SupportsType(eType)) {
                    ssMods.insert(ModInfo);
                }
            }
        }
    }

    GLOBALMODULECALL(OnGetAvailableMods(ssMods, eType), NOTHING);
}

void CModules::GetDefaultMods(set<CModInfo>& ssMods,
                              CModInfo::EModuleType eType) {
    GetAvailableMods(ssMods, eType);

    const map<CString, CModInfo::EModuleType> ns = {
        {"chansaver", CModInfo::UserModule},
        {"controlpanel", CModInfo::UserModule},
        {"simple_away", CModInfo::NetworkModule},
        {"webadmin", CModInfo::GlobalModule}};

    auto it = ssMods.begin();
    while (it != ssMods.end()) {
        auto it2 = ns.find(it->GetName());
        if (it2 != ns.end() && it2->second == eType) {
            ++it;
        } else {
            it = ssMods.erase(it);
        }
    }
}

bool CModules::FindModPath(const CString& sModule, CString& sModPath,
                           CString& sDataPath) {
    CString sMod = sModule;
    CString sDir = sMod;
    if (!sModule.Contains(".")) sMod += ".so";

    ModDirList dirs = GetModDirs();

    while (!dirs.empty()) {
        sModPath = dirs.front().first + sMod;
        sDataPath = dirs.front().second;
        dirs.pop();

        if (CFile::Exists(sModPath)) {
            sDataPath += sDir;
            return true;
        }
    }

    return false;
}

CModules::ModDirList CModules::GetModDirs() {
    ModDirList ret;
    CString sDir;

#ifdef RUN_FROM_SOURCE
    // ./modules
    sDir = CZNC::Get().GetCurPath() + "/modules/";
    ret.push(std::make_pair(sDir, sDir + "data/"));
#endif

    // ~/.znc/modules
    sDir = CZNC::Get().GetModPath() + "/";
    ret.push(std::make_pair(sDir, sDir));

    // <moduledir> and <datadir> (<prefix>/lib/znc)
    ret.push(std::make_pair(_MODDIR_ + CString("/"),
                            _DATADIR_ + CString("/modules/")));

    return ret;
}

ModHandle CModules::OpenModule(const CString& sModule, const CString& sModPath,
                               CModInfo& Info, CString& sRetMsg) {
    // Some sane defaults in case anything errors out below
    sRetMsg.clear();

    for (unsigned int a = 0; a < sModule.length(); a++) {
        if (((sModule[a] < '0') || (sModule[a] > '9')) &&
            ((sModule[a] < 'a') || (sModule[a] > 'z')) &&
            ((sModule[a] < 'A') || (sModule[a] > 'Z')) && (sModule[a] != '_')) {
            sRetMsg =
                t_f("Module names can only contain letters, numbers and "
                    "underscores, [{1}] is invalid")(sModule);
            return nullptr;
        }
    }

    // The second argument to dlopen() has a long history. It seems clear
    // that (despite what the man page says) we must include either of
    // RTLD_NOW and RTLD_LAZY and either of RTLD_GLOBAL and RTLD_LOCAL.
    //
    // RTLD_NOW vs. RTLD_LAZY: We use RTLD_NOW to avoid ZNC dying due to
    // failed symbol lookups later on. Doesn't really seem to have much of a
    // performance impact.
    //
    // RTLD_GLOBAL vs. RTLD_LOCAL: If perl is loaded with RTLD_LOCAL and later
    // on loads own modules (which it apparently does with RTLD_LAZY), we will
    // die in a name lookup since one of perl's symbols isn't found. That's
    // worse than any theoretical issue with RTLD_GLOBAL.
    ModHandle p = dlopen((sModPath).c_str(), RTLD_NOW | RTLD_GLOBAL);

    if (!p) {
        // dlerror() returns pointer to static buffer, which may be overwritten
        // very soon with another dl call also it may just return null.
        const char* cDlError = dlerror();
        CString sDlError = cDlError ? cDlError : t_s("Unknown error");
        sRetMsg = t_f("Unable to open module {1}: {2}")(sModule, sDlError);
        return nullptr;
    }

    const CModuleEntry* (*fpZNCModuleEntry)() = nullptr;
    // man dlsym(3) explains this
    *reinterpret_cast<void**>(&fpZNCModuleEntry) = dlsym(p, "ZNCModuleEntry");
    if (!fpZNCModuleEntry) {
        dlclose(p);
        sRetMsg = t_f("Could not find ZNCModuleEntry in module {1}")(sModule);
        return nullptr;
    }
    const CModuleEntry* pModuleEntry = fpZNCModuleEntry();

    if (std::strcmp(pModuleEntry->pcVersion, VERSION_STR) ||
        std::strcmp(pModuleEntry->pcVersionExtra, VERSION_EXTRA)) {
        sRetMsg = t_f(
            "Version mismatch for module {1}: core is {2}, module is built for "
            "{3}. Recompile this module.")(
            sModule, VERSION_STR VERSION_EXTRA,
            CString(pModuleEntry->pcVersion) + pModuleEntry->pcVersionExtra);
        dlclose(p);
        return nullptr;
    }

    if (std::strcmp(pModuleEntry->pcCompileOptions,
                    ZNC_COMPILE_OPTIONS_STRING)) {
        sRetMsg = t_f(
            "Module {1} is built incompatibly: core is '{2}', module is '{3}'. "
            "Recompile this module.")(sModule, ZNC_COMPILE_OPTIONS_STRING,
                                      pModuleEntry->pcCompileOptions);
        dlclose(p);
        return nullptr;
    }

    CTranslationDomainRefHolder translation("znc-" + sModule);
    pModuleEntry->fpFillModInfo(Info);

    sRetMsg = "";
    return p;
}

CModCommand::CModCommand()
    : m_sCmd(), m_pFunc(nullptr), m_Args(""), m_Desc("") {}

CModCommand::CModCommand(const CString& sCmd, CModule* pMod, ModCmdFunc func,
                         const CString& sArgs, const CString& sDesc)
    : m_sCmd(sCmd),
      m_pFunc([pMod, func](const CString& sLine) { (pMod->*func)(sLine); }),
      m_Args(sArgs),
      m_Desc(sDesc) {}

CModCommand::CModCommand(const CString& sCmd, CmdFunc func,
                         const COptionalTranslation& Args,
                         const COptionalTranslation& Desc)
    : m_sCmd(sCmd), m_pFunc(std::move(func)), m_Args(Args), m_Desc(Desc) {}

void CModCommand::InitHelp(CTable& Table) {
    Table.AddColumn(t_s("Command", "modhelpcmd"));
    Table.AddColumn(t_s("Description", "modhelpcmd"));
}

void CModCommand::AddHelp(CTable& Table) const {
    Table.AddRow();
    Table.SetCell(t_s("Command", "modhelpcmd"), GetCommand() + " " + GetArgs());
    Table.SetCell(t_s("Description", "modhelpcmd"), GetDescription());
}

CString CModule::t_s(const CString& sEnglish, const CString& sContext) const {
    return CTranslation::Get().Singular("znc-" + GetModName(), sContext,
                                        sEnglish);
}

CInlineFormatMessage CModule::t_f(const CString& sEnglish,
                                  const CString& sContext) const {
    return CInlineFormatMessage(t_s(sEnglish, sContext));
}

CInlineFormatMessage CModule::t_p(const CString& sEnglish,
                                  const CString& sEnglishes, int iNum,
                                  const CString& sContext) const {
    return CInlineFormatMessage(CTranslation::Get().Plural(
        "znc-" + GetModName(), sContext, sEnglish, sEnglishes, iNum));
}

CDelayedTranslation CModule::t_d(const CString& sEnglish,
                                 const CString& sContext) const {
    return CDelayedTranslation("znc-" + GetModName(), sContext, sEnglish);
}

CString CModInfo::t_s(const CString& sEnglish, const CString& sContext) const {
    return CTranslation::Get().Singular("znc-" + GetName(), sContext, sEnglish);
}
