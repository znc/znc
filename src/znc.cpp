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

#include <znc/znc.h>
#include <znc/FileUtils.h>
#include <znc/IRCSock.h>
#include <znc/Server.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Config.h>
#include <time.h>
#include <tuple>
#include <algorithm>

using std::endl;
using std::cout;
using std::map;
using std::set;
using std::vector;
using std::list;
using std::tuple;
using std::make_tuple;

CZNC::CZNC()
    : m_TimeStarted(time(nullptr)),
      m_eConfigState(ECONFIG_NOTHING),
      m_vpListeners(),
      m_msUsers(),
      m_msDelUsers(),
      m_Manager(),
      m_sCurPath(""),
      m_sZNCPath(""),
      m_sConfigFile(""),
      m_sSkinName(""),
      m_sStatusPrefix(""),
      m_sPidFile(""),
      m_sSSLCertFile(""),
      m_sSSLKeyFile(""),
      m_sSSLDHParamFile(""),
      m_sSSLCiphers(""),
      m_sSSLProtocols(""),
      m_vsBindHosts(),
      m_vsTrustedProxies(),
      m_vsMotd(),
      m_pLockFile(nullptr),
      m_uiConnectDelay(5),
      m_uiAnonIPLimit(10),
      m_uiMaxBufferSize(500),
      m_uDisabledSSLProtocols(Csock::EDP_SSL),
      m_pModules(new CModules),
      m_uBytesRead(0),
      m_uBytesWritten(0),
      m_lpConnectQueue(),
      m_pConnectQueueTimer(nullptr),
      m_uiConnectPaused(0),
      m_uiForceEncoding(0),
      m_sConnectThrottle(),
      m_bProtectWebSessions(true),
      m_bHideVersion(false),
      m_bAuthOnlyViaModule(false),
      m_Translation("znc"),
      m_uiConfigWriteDelay(0),
      m_pConfigTimer(nullptr) {
    if (!InitCsocket()) {
        CUtils::PrintError("Could not initialize Csocket!");
        exit(-1);
    }
    m_sConnectThrottle.SetTTL(30000);
}

CZNC::~CZNC() {
    m_pModules->UnloadAll();

    for (const auto& it : m_msUsers) {
        it.second->GetModules().UnloadAll();

        const vector<CIRCNetwork*>& networks = it.second->GetNetworks();
        for (CIRCNetwork* pNetwork : networks) {
            pNetwork->GetModules().UnloadAll();
        }
    }

    for (CListener* pListener : m_vpListeners) {
        delete pListener;
    }

    for (const auto& it : m_msUsers) {
        it.second->SetBeingDeleted(true);
    }

    m_pConnectQueueTimer = nullptr;
    // This deletes m_pConnectQueueTimer
    m_Manager.Cleanup();
    DeleteUsers();

    delete m_pModules;
    delete m_pLockFile;

    ShutdownCsocket();
    DeletePidFile();
}

CString CZNC::GetVersion() {
    return CString(VERSION_STR) + CString(ZNC_VERSION_EXTRA);
}

CString CZNC::GetTag(bool bIncludeVersion, bool bHTML) {
    if (!Get().m_bHideVersion) {
        bIncludeVersion = true;
    }
    CString sAddress = bHTML ? "<a href=\"https://znc.in\">https://znc.in</a>"
                             : "https://znc.in";

    if (!bIncludeVersion) {
        return "ZNC - " + sAddress;
    }

    CString sVersion = GetVersion();

    return "ZNC " + sVersion + " - " + sAddress;
}

CString CZNC::GetCompileOptionsString() {
    // Build system doesn't affect ABI
    return ZNC_COMPILE_OPTIONS_STRING + CString(
                                            ", build: "
#ifdef BUILD_WITH_CMAKE
                                            "cmake"
#else
                                            "autoconf"
#endif
                                            );
}

CString CZNC::GetUptime() const {
    time_t now = time(nullptr);
    return CString::ToTimeStr(now - TimeStarted());
}

bool CZNC::OnBoot() {
    bool bFail = false;
    ALLMODULECALL(OnBoot(), &bFail);
    if (bFail) return false;

    return true;
}

bool CZNC::HandleUserDeletion() {
    if (m_msDelUsers.empty()) return false;

    for (const auto& it : m_msDelUsers) {
        CUser* pUser = it.second;
        pUser->SetBeingDeleted(true);

        if (GetModules().OnDeleteUser(*pUser)) {
            pUser->SetBeingDeleted(false);
            continue;
        }
        m_msUsers.erase(pUser->GetUserName());
        CWebSock::FinishUserSessions(*pUser);
        delete pUser;
    }

    m_msDelUsers.clear();

    return true;
}

class CConfigWriteTimer : public CCron {
  public:
    CConfigWriteTimer(int iSecs) : CCron() {
        SetName("Config write timer");
        Start(iSecs);
    }

  protected:
    void RunJob() override {
        CZNC::Get().SetConfigState(CZNC::ECONFIG_NEED_WRITE);

        CZNC::Get().DisableConfigTimer();
    }
};

void CZNC::Loop() {
    while (true) {
        CString sError;

        ConfigState eState = GetConfigState();
        switch (eState) {
            case ECONFIG_NEED_REHASH:
                SetConfigState(ECONFIG_NOTHING);

                if (RehashConfig(sError)) {
                    Broadcast("Rehashing succeeded", true);
                } else {
                    Broadcast("Rehashing failed: " + sError, true);
                    Broadcast("ZNC is in some possibly inconsistent state!",
                              true);
                }
                break;
            case ECONFIG_DELAYED_WRITE:
                SetConfigState(ECONFIG_NOTHING);

                if (GetConfigWriteDelay() > 0) {
                    if (m_pConfigTimer == nullptr) {
                        m_pConfigTimer = new CConfigWriteTimer(GetConfigWriteDelay());
                        GetManager().AddCron(m_pConfigTimer);
                    }
                    break;
                }
                /* Fall through */
            case ECONFIG_NEED_WRITE:
            case ECONFIG_NEED_VERBOSE_WRITE:
                SetConfigState(ECONFIG_NOTHING);

                // stop pending configuration timer
                DisableConfigTimer();

                if (!WriteConfig()) {
                    Broadcast("Writing the config file failed", true);
                } else if (eState == ECONFIG_NEED_VERBOSE_WRITE) {
                    Broadcast("Writing the config succeeded", true);
                }
                break;
            case ECONFIG_NOTHING:
                break;
            case ECONFIG_NEED_QUIT:
                return;
        }

        // Check for users that need to be deleted
        if (HandleUserDeletion()) {
            // Also remove those user(s) from the config file
            WriteConfig();
        }

        // Csocket wants micro seconds
        // 100 msec to 5 min
        m_Manager.DynamicSelectLoop(100 * 1000, 5 * 60 * 1000 * 1000);
    }
}

CFile* CZNC::InitPidFile() {
    if (!m_sPidFile.empty()) {
        CString sFile;

        // absolute path or relative to the data dir?
        if (m_sPidFile[0] != '/')
            sFile = GetZNCPath() + "/" + m_sPidFile;
        else
            sFile = m_sPidFile;

        return new CFile(sFile);
    }

    return nullptr;
}

bool CZNC::WritePidFile(int iPid) {
    CFile* File = InitPidFile();
    if (File == nullptr) return false;

    CUtils::PrintAction("Writing pid file [" + File->GetLongName() + "]");

    bool bRet = false;
    if (File->Open(O_WRONLY | O_TRUNC | O_CREAT)) {
        File->Write(CString(iPid) + "\n");
        File->Close();
        bRet = true;
    }

    delete File;
    CUtils::PrintStatus(bRet);
    return bRet;
}

bool CZNC::DeletePidFile() {
    CFile* File = InitPidFile();
    if (File == nullptr) return false;

    CUtils::PrintAction("Deleting pid file [" + File->GetLongName() + "]");

    bool bRet = File->Delete();

    delete File;
    CUtils::PrintStatus(bRet);
    return bRet;
}

bool CZNC::WritePemFile() {
#ifndef HAVE_LIBSSL
    CUtils::PrintError("ZNC was not compiled with ssl support.");
    return false;
#else
    CString sPemFile = GetPemLocation();

    CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");
#ifndef _WIN32
    int fd = creat(sPemFile.c_str(), 0600);
    if (fd == -1) {
        CUtils::PrintStatus(false, "Unable to open");
        return false;
    }
    FILE* f = fdopen(fd, "w");
#else
    FILE* f = fopen(sPemFile.c_str(), "w");
#endif

    if (!f) {
        CUtils::PrintStatus(false, "Unable to open");
        return false;
    }

    CUtils::GenerateCert(f, "");
    fclose(f);

    CUtils::PrintStatus(true);
    return true;
#endif
}

void CZNC::DeleteUsers() {
    for (const auto& it : m_msUsers) {
        it.second->SetBeingDeleted(true);
        delete it.second;
    }

    m_msUsers.clear();
    DisableConnectQueue();
}

bool CZNC::IsHostAllowed(const CString& sHostMask) const {
    for (const auto& it : m_msUsers) {
        if (it.second->IsHostAllowed(sHostMask)) {
            return true;
        }
    }

    return false;
}

bool CZNC::AllowConnectionFrom(const CString& sIP) const {
    if (m_uiAnonIPLimit == 0) return true;
    return (GetManager().GetAnonConnectionCount(sIP) < m_uiAnonIPLimit);
}

void CZNC::InitDirs(const CString& sArgvPath, const CString& sDataDir) {
    // If the bin was not ran from the current directory, we need to add that
    // dir onto our cwd
    CString::size_type uPos = sArgvPath.rfind('/');
    if (uPos == CString::npos)
        m_sCurPath = "./";
    else
        m_sCurPath = CDir::ChangeDir("./", sArgvPath.Left(uPos), "");

    // Try to set the user's home dir, default to binpath on failure
    CFile::InitHomePath(m_sCurPath);

    if (sDataDir.empty()) {
        m_sZNCPath = CFile::GetHomePath() + "/.znc";
    } else {
        m_sZNCPath = sDataDir;
    }

    m_sSSLCertFile = m_sZNCPath + "/znc.pem";
}

CString CZNC::GetConfPath(bool bAllowMkDir) const {
    CString sConfPath = m_sZNCPath + "/configs";
    if (bAllowMkDir && !CFile::Exists(sConfPath)) {
        CDir::MakeDir(sConfPath);
    }

    return sConfPath;
}

CString CZNC::GetUserPath() const {
    CString sUserPath = m_sZNCPath + "/users";
    if (!CFile::Exists(sUserPath)) {
        CDir::MakeDir(sUserPath);
    }

    return sUserPath;
}

CString CZNC::GetModPath() const {
    CString sModPath = m_sZNCPath + "/modules";

    return sModPath;
}

const CString& CZNC::GetCurPath() const {
    if (!CFile::Exists(m_sCurPath)) {
        CDir::MakeDir(m_sCurPath);
    }
    return m_sCurPath;
}

const CString& CZNC::GetHomePath() const { return CFile::GetHomePath(); }

const CString& CZNC::GetZNCPath() const {
    if (!CFile::Exists(m_sZNCPath)) {
        CDir::MakeDir(m_sZNCPath);
    }
    return m_sZNCPath;
}

CString CZNC::GetPemLocation() const {
    return CDir::ChangeDir("", m_sSSLCertFile);
}

CString CZNC::GetKeyLocation() const {
    return CDir::ChangeDir(
        "", m_sSSLKeyFile.empty() ? m_sSSLCertFile : m_sSSLKeyFile);
}

CString CZNC::GetDHParamLocation() const {
    return CDir::ChangeDir(
        "", m_sSSLDHParamFile.empty() ? m_sSSLCertFile : m_sSSLDHParamFile);
}

CString CZNC::ExpandConfigPath(const CString& sConfigFile, bool bAllowMkDir) {
    CString sRetPath;

    if (sConfigFile.empty()) {
        sRetPath = GetConfPath(bAllowMkDir) + "/znc.conf";
    } else {
        if (sConfigFile.StartsWith("./") || sConfigFile.StartsWith("../")) {
            sRetPath = GetCurPath() + "/" + sConfigFile;
        } else if (!sConfigFile.StartsWith("/")) {
            sRetPath = GetConfPath(bAllowMkDir) + "/" + sConfigFile;
        } else {
            sRetPath = sConfigFile;
        }
    }

    return sRetPath;
}

bool CZNC::WriteConfig() {
    if (GetConfigFile().empty()) {
        DEBUG("Config file name is empty?!");
        return false;
    }

    // We first write to a temporary file and then move it to the right place
    CFile* pFile = new CFile(GetConfigFile() + "~");

    if (!pFile->Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
        DEBUG("Could not write config to " + GetConfigFile() + "~: " +
              CString(strerror(errno)));
        delete pFile;
        return false;
    }

    // We have to "transfer" our lock on the config to the new file.
    // The old file (= inode) is going away and thus a lock on it would be
    // useless. These lock should always succeed (races, anyone?).
    if (!pFile->TryExLock()) {
        DEBUG("Error while locking the new config file, errno says: " +
              CString(strerror(errno)));
        pFile->Delete();
        delete pFile;
        return false;
    }

    pFile->Write(MakeConfigHeader() + "\n");

    CConfig config;
    config.AddKeyValuePair("AnonIPLimit", CString(m_uiAnonIPLimit));
    config.AddKeyValuePair("MaxBufferSize", CString(m_uiMaxBufferSize));
    config.AddKeyValuePair("SSLCertFile", CString(GetPemLocation()));
    config.AddKeyValuePair("SSLKeyFile", CString(GetKeyLocation()));
    config.AddKeyValuePair("SSLDHParamFile", CString(GetDHParamLocation()));
    config.AddKeyValuePair("ProtectWebSessions",
                           CString(m_bProtectWebSessions));
    config.AddKeyValuePair("HideVersion", CString(m_bHideVersion));
    config.AddKeyValuePair("AuthOnlyViaModule", CString(m_bAuthOnlyViaModule));
    config.AddKeyValuePair("Version", CString(VERSION_STR));
    config.AddKeyValuePair("ConfigWriteDelay", CString(m_uiConfigWriteDelay));

    unsigned int l = 0;
    for (CListener* pListener : m_vpListeners) {
        CConfig listenerConfig;

        listenerConfig.AddKeyValuePair("Host", pListener->GetBindHost());
        listenerConfig.AddKeyValuePair("URIPrefix",
                                       pListener->GetURIPrefix() + "/");
        listenerConfig.AddKeyValuePair("Port", CString(pListener->GetPort()));

        listenerConfig.AddKeyValuePair(
            "IPv4", CString(pListener->GetAddrType() != ADDR_IPV6ONLY));
        listenerConfig.AddKeyValuePair(
            "IPv6", CString(pListener->GetAddrType() != ADDR_IPV4ONLY));

        listenerConfig.AddKeyValuePair("SSL", CString(pListener->IsSSL()));

        listenerConfig.AddKeyValuePair(
            "AllowIRC",
            CString(pListener->GetAcceptType() != CListener::ACCEPT_HTTP));
        listenerConfig.AddKeyValuePair(
            "AllowWeb",
            CString(pListener->GetAcceptType() != CListener::ACCEPT_IRC));

        config.AddSubConfig("Listener", "listener" + CString(l++),
                            listenerConfig);
    }

    config.AddKeyValuePair("ConnectDelay", CString(m_uiConnectDelay));
    config.AddKeyValuePair("ServerThrottle",
                           CString(m_sConnectThrottle.GetTTL() / 1000));

    if (!m_sPidFile.empty()) {
        config.AddKeyValuePair("PidFile", m_sPidFile.FirstLine());
    }

    if (!m_sSkinName.empty()) {
        config.AddKeyValuePair("Skin", m_sSkinName.FirstLine());
    }

    if (!m_sStatusPrefix.empty()) {
        config.AddKeyValuePair("StatusPrefix", m_sStatusPrefix.FirstLine());
    }

    if (!m_sSSLCiphers.empty()) {
        config.AddKeyValuePair("SSLCiphers", CString(m_sSSLCiphers));
    }

    if (!m_sSSLProtocols.empty()) {
        config.AddKeyValuePair("SSLProtocols", m_sSSLProtocols);
    }

    for (const CString& sLine : m_vsMotd) {
        config.AddKeyValuePair("Motd", sLine.FirstLine());
    }

    for (const CString& sProxy : m_vsTrustedProxies) {
        config.AddKeyValuePair("TrustedProxy", sProxy.FirstLine());
    }

    CModules& Mods = GetModules();

    for (const CModule* pMod : Mods) {
        CString sName = pMod->GetModName();
        CString sArgs = pMod->GetArgs();

        if (!sArgs.empty()) {
            sArgs = " " + sArgs.FirstLine();
        }

        config.AddKeyValuePair("LoadModule", sName.FirstLine() + sArgs);
    }

    for (const auto& it : m_msUsers) {
        CString sErr;

        if (!it.second->IsValid(sErr)) {
            DEBUG("** Error writing config for user [" << it.first << "] ["
                                                       << sErr << "]");
            continue;
        }

        config.AddSubConfig("User", it.second->GetUserName(),
                            it.second->ToConfig());
    }

    config.Write(*pFile);

    // If Sync() fails... well, let's hope nothing important breaks..
    pFile->Sync();

    if (pFile->HadError()) {
        DEBUG("Error while writing the config, errno says: " +
              CString(strerror(errno)));
        pFile->Delete();
        delete pFile;
        return false;
    }

    // We wrote to a temporary name, move it to the right place
    if (!pFile->Move(GetConfigFile(), true)) {
        DEBUG(
            "Error while replacing the config file with a new version, errno "
            "says "
            << strerror(errno));
        pFile->Delete();
        delete pFile;
        return false;
    }

    // Everything went fine, just need to update the saved path.
    pFile->SetFileName(GetConfigFile());

    // Make sure the lock is kept alive as long as we need it.
    delete m_pLockFile;
    m_pLockFile = pFile;

    return true;
}

CString CZNC::MakeConfigHeader() {
    return "// WARNING\n"
           "//\n"
           "// Do NOT edit this file while ZNC is running!\n"
           "// Use webadmin or *controlpanel instead.\n"
           "//\n"
           "// Altering this file by hand will forfeit all support.\n"
           "//\n"
           "// But if you feel risky, you might want to read help on /znc "
           "saveconfig and /znc rehash.\n"
           "// Also check https://wiki.znc.in/Configuration\n";
}

bool CZNC::WriteNewConfig(const CString& sConfigFile) {
    CString sAnswer, sUser, sNetwork;
    VCString vsLines;

    vsLines.push_back(MakeConfigHeader());
    vsLines.push_back("Version = " + CString(VERSION_STR));

    m_sConfigFile = ExpandConfigPath(sConfigFile);

    if (CFile::Exists(m_sConfigFile)) {
        CUtils::PrintStatus(
            false, "WARNING: config [" + m_sConfigFile + "] already exists.");
    }

    CUtils::PrintMessage("");
    CUtils::PrintMessage("-- Global settings --");
    CUtils::PrintMessage("");

// Listen
#ifdef HAVE_IPV6
    bool b6 = true;
#else
    bool b6 = false;
#endif
    CString sListenHost;
    CString sURIPrefix;
    bool bListenSSL = false;
    unsigned int uListenPort = 0;
    bool bSuccess;

    do {
        bSuccess = true;
        while (true) {
            if (!CUtils::GetNumInput("Listen on port", uListenPort, 1025,
                                     65534)) {
                continue;
            }
            if (uListenPort == 6667) {
                CUtils::PrintStatus(false,
                                    "WARNING: Some web browsers reject port "
                                    "6667. If you intend to");
                CUtils::PrintStatus(false,
                                    "use ZNC's web interface, you might want "
                                    "to use another port.");
                if (!CUtils::GetBoolInput("Proceed with port 6667 anyway?",
                                          true)) {
                    continue;
                }
            }
            break;
        }

#ifdef HAVE_LIBSSL
        bListenSSL = CUtils::GetBoolInput("Listen using SSL", bListenSSL);
#endif

#ifdef HAVE_IPV6
        b6 = CUtils::GetBoolInput("Listen using both IPv4 and IPv6", b6);
#endif

        // Don't ask for listen host, it may be configured later if needed.

        CUtils::PrintAction("Verifying the listener");
        CListener* pListener = new CListener(
            (unsigned short int)uListenPort, sListenHost, sURIPrefix,
            bListenSSL, b6 ? ADDR_ALL : ADDR_IPV4ONLY, CListener::ACCEPT_ALL);
        if (!pListener->Listen()) {
            CUtils::PrintStatus(false, FormatBindError());
            bSuccess = false;
        } else
            CUtils::PrintStatus(true);
        delete pListener;
    } while (!bSuccess);

#ifdef HAVE_LIBSSL
    CString sPemFile = GetPemLocation();
    if (!CFile::Exists(sPemFile)) {
        CUtils::PrintMessage("Unable to locate pem file: [" + sPemFile +
                             "], creating it");
        WritePemFile();
    }
#endif

    vsLines.push_back("<Listener l>");
    vsLines.push_back("\tPort = " + CString(uListenPort));
    vsLines.push_back("\tIPv4 = true");
    vsLines.push_back("\tIPv6 = " + CString(b6));
    vsLines.push_back("\tSSL = " + CString(bListenSSL));
    if (!sListenHost.empty()) {
        vsLines.push_back("\tHost = " + sListenHost);
    }
    vsLines.push_back("</Listener>");
    // !Listen

    set<CModInfo> ssGlobalMods;
    GetModules().GetDefaultMods(ssGlobalMods, CModInfo::GlobalModule);
    vector<CString> vsGlobalModNames;
    for (const CModInfo& Info : ssGlobalMods) {
        vsGlobalModNames.push_back(Info.GetName());
        vsLines.push_back("LoadModule = " + Info.GetName());
    }
    CUtils::PrintMessage(
        "Enabled global modules [" +
        CString(", ").Join(vsGlobalModNames.begin(), vsGlobalModNames.end()) +
        "]");

    // User
    CUtils::PrintMessage("");
    CUtils::PrintMessage("-- Admin user settings --");
    CUtils::PrintMessage("");

    vsLines.push_back("");
    CString sNick;
    do {
        CUtils::GetInput("Username", sUser, "", "alphanumeric");
    } while (!CUser::IsValidUserName(sUser));

    vsLines.push_back("<User " + sUser + ">");
    CString sSalt;
    sAnswer = CUtils::GetSaltedHashPass(sSalt);
    vsLines.push_back("\tPass       = " + CUtils::sDefaultHash + "#" + sAnswer +
                      "#" + sSalt + "#");

    vsLines.push_back("\tAdmin      = true");

    CUtils::GetInput("Nick", sNick, CUser::MakeCleanUserName(sUser));
    vsLines.push_back("\tNick       = " + sNick);
    CUtils::GetInput("Alternate nick", sAnswer, sNick + "_");
    if (!sAnswer.empty()) {
        vsLines.push_back("\tAltNick    = " + sAnswer);
    }
    CUtils::GetInput("Ident", sAnswer, sUser);
    vsLines.push_back("\tIdent      = " + sAnswer);
    CUtils::GetInput("Real name", sAnswer, "", "optional");
    if (!sAnswer.empty()) {
        vsLines.push_back("\tRealName   = " + sAnswer);
    }
    CUtils::GetInput("Bind host", sAnswer, "", "optional");
    if (!sAnswer.empty()) {
        vsLines.push_back("\tBindHost   = " + sAnswer);
    }

    set<CModInfo> ssUserMods;
    GetModules().GetDefaultMods(ssUserMods, CModInfo::UserModule);
    vector<CString> vsUserModNames;
    for (const CModInfo& Info : ssUserMods) {
        vsUserModNames.push_back(Info.GetName());
        vsLines.push_back("\tLoadModule = " + Info.GetName());
    }
    CUtils::PrintMessage(
        "Enabled user modules [" +
        CString(", ").Join(vsUserModNames.begin(), vsUserModNames.end()) + "]");

    CUtils::PrintMessage("");
    if (CUtils::GetBoolInput("Set up a network?", true)) {
        vsLines.push_back("");

        CUtils::PrintMessage("");
        CUtils::PrintMessage("-- Network settings --");
        CUtils::PrintMessage("");

        do {
            CUtils::GetInput("Name", sNetwork, "freenode");
        } while (!CIRCNetwork::IsValidNetwork(sNetwork));

        vsLines.push_back("\t<Network " + sNetwork + ">");

        set<CModInfo> ssNetworkMods;
        GetModules().GetDefaultMods(ssNetworkMods, CModInfo::NetworkModule);
        vector<CString> vsNetworkModNames;
        for (const CModInfo& Info : ssNetworkMods) {
            vsNetworkModNames.push_back(Info.GetName());
            vsLines.push_back("\t\tLoadModule = " + Info.GetName());
        }

        CString sHost, sPass, sHint;
        bool bSSL = false;
        unsigned int uServerPort = 0;

        if (sNetwork.Equals("freenode")) {
            sHost = "chat.freenode.net";
#ifdef HAVE_LIBSSL
            bSSL = true;
#endif
        } else {
            sHint = "host only";
        }

        while (!CUtils::GetInput("Server host", sHost, sHost, sHint) ||
               !CServer::IsValidHostName(sHost))
            ;
#ifdef HAVE_LIBSSL
        bSSL = CUtils::GetBoolInput("Server uses SSL?", bSSL);
#endif
        while (!CUtils::GetNumInput("Server port", uServerPort, 1, 65535,
                                    bSSL ? 6697 : 6667))
            ;
        CUtils::GetInput("Server password (probably empty)", sPass);

        vsLines.push_back("\t\tServer     = " + sHost + ((bSSL) ? " +" : " ") +
                          CString(uServerPort) + " " + sPass);

        CString sChans;
        if (CUtils::GetInput("Initial channels", sChans)) {
            vsLines.push_back("");
            VCString vsChans;
            sChans.Replace(",", " ");
            sChans.Replace(";", " ");
            sChans.Split(" ", vsChans, false, "", "", true, true);
            for (const CString& sChan : vsChans) {
                vsLines.push_back("\t\t<Chan " + sChan + ">");
                vsLines.push_back("\t\t</Chan>");
            }
        }

        CUtils::PrintMessage("Enabled network modules [" +
                             CString(", ").Join(vsNetworkModNames.begin(),
                                                vsNetworkModNames.end()) +
                             "]");

        vsLines.push_back("\t</Network>");
    }

    vsLines.push_back("</User>");

    CUtils::PrintMessage("");
    // !User

    CFile File;
    bool bFileOK, bFileOpen = false;
    do {
        CUtils::PrintAction("Writing config [" + m_sConfigFile + "]");

        bFileOK = true;
        if (CFile::Exists(m_sConfigFile)) {
            if (!File.TryExLock(m_sConfigFile)) {
                CUtils::PrintStatus(false,
                                    "ZNC is currently running on this config.");
                bFileOK = false;
            } else {
                File.Close();
                CUtils::PrintStatus(false, "This config already exists.");
                if (CUtils::GetBoolInput(
                        "Are you sure you want to overwrite it?", false))
                    CUtils::PrintAction("Overwriting config [" + m_sConfigFile +
                                        "]");
                else
                    bFileOK = false;
            }
        }

        if (bFileOK) {
            File.SetFileName(m_sConfigFile);
            if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
                bFileOpen = true;
            } else {
                CUtils::PrintStatus(false, "Unable to open file");
                bFileOK = false;
            }
        }
        if (!bFileOK) {
            while (!CUtils::GetInput("Please specify an alternate location",
                                     m_sConfigFile, "",
                                     "or \"stdout\" for displaying the config"))
                ;
            if (m_sConfigFile.Equals("stdout"))
                bFileOK = true;
            else
                m_sConfigFile = ExpandConfigPath(m_sConfigFile);
        }
    } while (!bFileOK);

    if (!bFileOpen) {
        CUtils::PrintMessage("");
        CUtils::PrintMessage("Printing the new config to stdout:");
        CUtils::PrintMessage("");
        cout << endl << "------------------------------------------------------"
                        "----------------------" << endl << endl;
    }

    for (const CString& sLine : vsLines) {
        if (bFileOpen) {
            File.Write(sLine + "\n");
        } else {
            cout << sLine << endl;
        }
    }

    if (bFileOpen) {
        File.Close();
        if (File.HadError())
            CUtils::PrintStatus(false,
                                "There was an error while writing the config");
        else
            CUtils::PrintStatus(true);
    } else {
        cout << endl << "------------------------------------------------------"
                        "----------------------" << endl << endl;
    }

    if (File.HadError()) {
        bFileOpen = false;
        CUtils::PrintMessage("Printing the new config to stdout instead:");
        cout << endl << "------------------------------------------------------"
                        "----------------------" << endl << endl;
        for (const CString& sLine : vsLines) {
            cout << sLine << endl;
        }
        cout << endl << "------------------------------------------------------"
                        "----------------------" << endl << endl;
    }

    const CString sProtocol(bListenSSL ? "https" : "http");
    const CString sSSL(bListenSSL ? "+" : "");
    CUtils::PrintMessage("");
    CUtils::PrintMessage(
        "To connect to this ZNC you need to connect to it as your IRC server",
        true);
    CUtils::PrintMessage(
        "using the port that you supplied.  You have to supply your login info",
        true);
    CUtils::PrintMessage(
        "as the IRC server password like this: user/network:pass.", true);
    CUtils::PrintMessage("");
    CUtils::PrintMessage("Try something like this in your IRC client...", true);
    CUtils::PrintMessage("/server <znc_server_ip> " + sSSL +
                             CString(uListenPort) + " " + sUser + ":<pass>",
                         true);
    CUtils::PrintMessage("");
    CUtils::PrintMessage(
        "To manage settings, users and networks, point your web browser to",
        true);
    CUtils::PrintMessage(
        sProtocol + "://<znc_server_ip>:" + CString(uListenPort) + "/", true);
    CUtils::PrintMessage("");

    File.UnLock();

    bool bWantLaunch = bFileOpen;
    if (bWantLaunch) {
        // "export ZNC_NO_LAUNCH_AFTER_MAKECONF=1" would cause znc --makeconf to
        // not offer immediate launch.
        // Useful for distros which want to create config when znc package is
        // installed.
        // See https://github.com/znc/znc/pull/257
        char* szNoLaunch = getenv("ZNC_NO_LAUNCH_AFTER_MAKECONF");
        if (szNoLaunch && *szNoLaunch == '1') {
            bWantLaunch = false;
        }
    }
    if (bWantLaunch) {
        bWantLaunch = CUtils::GetBoolInput("Launch ZNC now?", true);
    }
    return bWantLaunch;
}

void CZNC::BackupConfigOnce(const CString& sSuffix) {
    static bool didBackup = false;
    if (didBackup) return;
    didBackup = true;

    CUtils::PrintAction("Creating a config backup");

    CString sBackup = CDir::ChangeDir(m_sConfigFile, "../znc.conf." + sSuffix);
    if (CFile::Copy(m_sConfigFile, sBackup))
        CUtils::PrintStatus(true, sBackup);
    else
        CUtils::PrintStatus(false, strerror(errno));
}

bool CZNC::ParseConfig(const CString& sConfig, CString& sError) {
    m_sConfigFile = ExpandConfigPath(sConfig, false);

    CConfig config;
    if (!ReadConfig(config, sError)) return false;

    if (!LoadGlobal(config, sError)) return false;

    if (!LoadUsers(config, sError)) return false;

    return true;
}

bool CZNC::ReadConfig(CConfig& config, CString& sError) {
    sError.clear();

    CUtils::PrintAction("Opening config [" + m_sConfigFile + "]");

    if (!CFile::Exists(m_sConfigFile)) {
        sError = "No such file";
        CUtils::PrintStatus(false, sError);
        CUtils::PrintMessage(
            "Restart ZNC with the --makeconf option if you wish to create this "
            "config.");
        return false;
    }

    if (!CFile::IsReg(m_sConfigFile)) {
        sError = "Not a file";
        CUtils::PrintStatus(false, sError);
        return false;
    }

    CFile* pFile = new CFile(m_sConfigFile);

    // need to open the config file Read/Write for fcntl()
    // exclusive locking to work properly!
    if (!pFile->Open(m_sConfigFile, O_RDWR)) {
        sError = "Can not open config file";
        CUtils::PrintStatus(false, sError);
        delete pFile;
        return false;
    }

    if (!pFile->TryExLock()) {
        sError = "ZNC is already running on this config.";
        CUtils::PrintStatus(false, sError);
        delete pFile;
        return false;
    }

    // (re)open the config file
    delete m_pLockFile;
    m_pLockFile = pFile;
    CFile& File = *pFile;

    if (!config.Parse(File, sError)) {
        CUtils::PrintStatus(false, sError);
        return false;
    }
    CUtils::PrintStatus(true);

    // check if config is from old ZNC version and
    // create a backup file if necessary
    CString sSavedVersion;
    config.FindStringEntry("version", sSavedVersion);
    if (sSavedVersion.empty()) {
        CUtils::PrintError(
            "Config does not contain a version identifier. It may be be too "
            "old or corrupt.");
        return false;
    }

    tuple<unsigned int, unsigned int> tSavedVersion =
        make_tuple(sSavedVersion.Token(0, false, ".").ToUInt(),
                   sSavedVersion.Token(1, false, ".").ToUInt());
    tuple<unsigned int, unsigned int> tCurrentVersion =
        make_tuple(VERSION_MAJOR, VERSION_MINOR);
    if (tSavedVersion < tCurrentVersion) {
        CUtils::PrintMessage("Found old config from ZNC " + sSavedVersion +
                             ". Saving a backup of it.");
        BackupConfigOnce("pre-" + CString(VERSION_STR));
    } else if (tSavedVersion > tCurrentVersion) {
        CUtils::PrintError("Config was saved from ZNC " + sSavedVersion +
                           ". It may or may not work with current ZNC " +
                           GetVersion());
    }

    return true;
}

bool CZNC::RehashConfig(CString& sError) {
    ALLMODULECALL(OnPreRehash(), NOTHING);

    CConfig config;
    if (!ReadConfig(config, sError)) return false;

    if (!LoadGlobal(config, sError)) return false;

    // do not reload users - it's dangerous!

    ALLMODULECALL(OnPostRehash(), NOTHING);
    return true;
}

bool CZNC::LoadGlobal(CConfig& config, CString& sError) {
    sError.clear();

    MCString msModules;  // Modules are queued for later loading

    VCString vsList;
    config.FindStringVector("loadmodule", vsList);
    for (const CString& sModLine : vsList) {
        CString sModName = sModLine.Token(0);
        CString sArgs = sModLine.Token(1, true);

        // compatibility for pre-1.0 configs
        CString sSavedVersion;
        config.FindStringEntry("version", sSavedVersion);
        tuple<unsigned int, unsigned int> tSavedVersion =
            make_tuple(sSavedVersion.Token(0, false, ".").ToUInt(),
                       sSavedVersion.Token(1, false, ".").ToUInt());
        if (sModName == "saslauth" && tSavedVersion < make_tuple(0, 207)) {
            CUtils::PrintMessage(
                "saslauth module was renamed to cyrusauth. Loading cyrusauth "
                "instead.");
            sModName = "cyrusauth";
        }
        // end-compatibility for pre-1.0 configs

        if (msModules.find(sModName) != msModules.end()) {
            sError = "Module [" + sModName + "] already loaded";
            CUtils::PrintError(sError);
            return false;
        }
        CString sModRet;
        CModule* pOldMod;

        pOldMod = GetModules().FindModule(sModName);
        if (!pOldMod) {
            CUtils::PrintAction("Loading global module [" + sModName + "]");

            bool bModRet =
                GetModules().LoadModule(sModName, sArgs, CModInfo::GlobalModule,
                                        nullptr, nullptr, sModRet);

            CUtils::PrintStatus(bModRet, bModRet ? "" : sModRet);
            if (!bModRet) {
                sError = sModRet;
                return false;
            }
        } else if (pOldMod->GetArgs() != sArgs) {
            CUtils::PrintAction("Reloading global module [" + sModName + "]");

            bool bModRet = GetModules().ReloadModule(sModName, sArgs, nullptr,
                                                     nullptr, sModRet);

            CUtils::PrintStatus(bModRet, sModRet);
            if (!bModRet) {
                sError = sModRet;
                return false;
            }
        } else
            CUtils::PrintMessage("Module [" + sModName + "] already loaded.");

        msModules[sModName] = sArgs;
    }

    m_vsMotd.clear();
    config.FindStringVector("motd", vsList);
    for (const CString& sMotd : vsList) {
        AddMotd(sMotd);
    }

    if (config.FindStringVector("bindhost", vsList)) {
        CUtils::PrintStatus(false,
                            "WARNING: the global BindHost list is deprecated. "
                            "Ignoring the following lines:");
        for (const CString& sHost : vsList) {
            CUtils::PrintStatus(false, "BindHost = " + sHost);
        }
    }
    if (config.FindStringVector("vhost", vsList)) {
        CUtils::PrintStatus(false,
                            "WARNING: the global vHost list is deprecated. "
                            "Ignoring the following lines:");
        for (const CString& sHost : vsList) {
            CUtils::PrintStatus(false, "vHost = " + sHost);
        }
    }

    m_vsTrustedProxies.clear();
    config.FindStringVector("trustedproxy", vsList);
    for (const CString& sProxy : vsList) {
        AddTrustedProxy(sProxy);
    }

    CString sVal;
    if (config.FindStringEntry("pidfile", sVal)) m_sPidFile = sVal;
    if (config.FindStringEntry("statusprefix", sVal)) m_sStatusPrefix = sVal;
    if (config.FindStringEntry("sslcertfile", sVal)) m_sSSLCertFile = sVal;
    if (config.FindStringEntry("sslkeyfile", sVal)) m_sSSLKeyFile = sVal;
    if (config.FindStringEntry("ssldhparamfile", sVal))
        m_sSSLDHParamFile = sVal;
    if (config.FindStringEntry("sslciphers", sVal)) m_sSSLCiphers = sVal;
    if (config.FindStringEntry("skin", sVal)) SetSkinName(sVal);
    if (config.FindStringEntry("connectdelay", sVal))
        SetConnectDelay(sVal.ToUInt());
    if (config.FindStringEntry("serverthrottle", sVal))
        m_sConnectThrottle.SetTTL(sVal.ToUInt() * 1000);
    if (config.FindStringEntry("anoniplimit", sVal))
        m_uiAnonIPLimit = sVal.ToUInt();
    if (config.FindStringEntry("maxbuffersize", sVal))
        m_uiMaxBufferSize = sVal.ToUInt();
    if (config.FindStringEntry("protectwebsessions", sVal))
        m_bProtectWebSessions = sVal.ToBool();
    if (config.FindStringEntry("hideversion", sVal))
        m_bHideVersion = sVal.ToBool();
    if (config.FindStringEntry("authonlyviamodule", sVal))
        m_bAuthOnlyViaModule = sVal.ToBool();
    if (config.FindStringEntry("sslprotocols", sVal)) {
        if (!SetSSLProtocols(sVal)) {
            VCString vsProtocols = GetAvailableSSLProtocols();
            CUtils::PrintError("Invalid SSLProtocols value [" + sVal + "]");
            CUtils::PrintError(
                "The syntax is [SSLProtocols = [+|-]<protocol> ...]");
            CUtils::PrintError(
                "Available protocols are [" +
                CString(", ").Join(vsProtocols.begin(), vsProtocols.end()) +
                "]");
            return false;
        }
    }
    if (config.FindStringEntry("configwritedelay", sVal))
        m_uiConfigWriteDelay = sVal.ToUInt();

    UnloadRemovedModules(msModules);

    if (!LoadListeners(config, sError)) return false;

    return true;
}

bool CZNC::LoadUsers(CConfig& config, CString& sError) {
    sError.clear();

    m_msUsers.clear();

    CConfig::SubConfig subConf;
    config.FindSubConfig("user", subConf);

    for (const auto& subIt : subConf) {
        const CString& sUserName = subIt.first;
        CConfig* pSubConf = subIt.second.m_pSubConfig;

        CUtils::PrintMessage("Loading user [" + sUserName + "]");

        std::unique_ptr<CUser> pUser(new CUser(sUserName));

        if (!m_sStatusPrefix.empty()) {
            if (!pUser->SetStatusPrefix(m_sStatusPrefix)) {
                sError = "Invalid StatusPrefix [" + m_sStatusPrefix +
                         "] Must be 1-5 chars, no spaces.";
                CUtils::PrintError(sError);
                return false;
            }
        }

        if (!pUser->ParseConfig(pSubConf, sError)) {
            CUtils::PrintError(sError);
            return false;
        }

        if (!pSubConf->empty()) {
            sError = "Unhandled lines in config for User [" + sUserName + "]!";
            CUtils::PrintError(sError);
            DumpConfig(pSubConf);
            return false;
        }

        CString sErr;
        if (!AddUser(pUser.release(), sErr, true)) {
            sError = "Invalid user [" + sUserName + "] " + sErr;
        }

        if (!sError.empty()) {
            CUtils::PrintError(sError);
            pUser->SetBeingDeleted(true);
            return false;
        }
    }

    if (m_msUsers.empty()) {
        sError = "You must define at least one user in your config.";
        CUtils::PrintError(sError);
        return false;
    }

    return true;
}

bool CZNC::LoadListeners(CConfig& config, CString& sError) {
    sError.clear();

    // Delete all listeners
    while (!m_vpListeners.empty()) {
        delete m_vpListeners[0];
        m_vpListeners.erase(m_vpListeners.begin());
    }

    // compatibility for pre-1.0 configs
    const char* szListenerEntries[] = {"listen",   "listen6",   "listen4",
                                       "listener", "listener6", "listener4"};

    VCString vsList;
    config.FindStringVector("loadmodule", vsList);

    // This has to be after SSLCertFile is handled since it uses that value
    for (const char* szEntry : szListenerEntries) {
        config.FindStringVector(szEntry, vsList);
        for (const CString& sListener : vsList) {
            if (!AddListener(szEntry + CString(" ") + sListener, sError))
                return false;
        }
    }
    // end-compatibility for pre-1.0 configs

    CConfig::SubConfig subConf;
    config.FindSubConfig("listener", subConf);

    for (const auto& subIt : subConf) {
        CConfig* pSubConf = subIt.second.m_pSubConfig;
        if (!AddListener(pSubConf, sError)) return false;
        if (!pSubConf->empty()) {
            sError = "Unhandled lines in Listener config!";
            CUtils::PrintError(sError);

            CZNC::DumpConfig(pSubConf);
            return false;
        }
    }

    if (m_vpListeners.empty()) {
        sError = "You must supply at least one Listener in your config.";
        CUtils::PrintError(sError);
        return false;
    }

    return true;
}

void CZNC::UnloadRemovedModules(const MCString& msModules) {
    // unload modules which are no longer in the config

    set<CString> ssUnload;
    for (CModule* pCurMod : GetModules()) {
        if (msModules.find(pCurMod->GetModName()) == msModules.end())
            ssUnload.insert(pCurMod->GetModName());
    }

    for (const CString& sMod : ssUnload) {
        if (GetModules().UnloadModule(sMod))
            CUtils::PrintMessage("Unloaded global module [" + sMod + "]");
        else
            CUtils::PrintMessage("Could not unload [" + sMod + "]");
    }
}

void CZNC::DumpConfig(const CConfig* pConfig) {
    CConfig::EntryMapIterator eit = pConfig->BeginEntries();
    for (; eit != pConfig->EndEntries(); ++eit) {
        const CString& sKey = eit->first;
        const VCString& vsList = eit->second;
        VCString::const_iterator it = vsList.begin();
        for (; it != vsList.end(); ++it) {
            CUtils::PrintError(sKey + " = " + *it);
        }
    }

    CConfig::SubConfigMapIterator sit = pConfig->BeginSubConfigs();
    for (; sit != pConfig->EndSubConfigs(); ++sit) {
        const CString& sKey = sit->first;
        const CConfig::SubConfig& sSub = sit->second;
        CConfig::SubConfig::const_iterator it = sSub.begin();

        for (; it != sSub.end(); ++it) {
            CUtils::PrintError("SubConfig [" + sKey + " " + it->first + "]:");
            DumpConfig(it->second.m_pSubConfig);
        }
    }
}

void CZNC::ClearTrustedProxies() { m_vsTrustedProxies.clear(); }

bool CZNC::AddTrustedProxy(const CString& sHost) {
    if (sHost.empty()) {
        return false;
    }

    for (const CString& sTrustedProxy : m_vsTrustedProxies) {
        if (sTrustedProxy.Equals(sHost)) {
            return false;
        }
    }

    m_vsTrustedProxies.push_back(sHost);
    return true;
}

bool CZNC::RemTrustedProxy(const CString& sHost) {
    VCString::iterator it;
    for (it = m_vsTrustedProxies.begin(); it != m_vsTrustedProxies.end();
         ++it) {
        if (sHost.Equals(*it)) {
            m_vsTrustedProxies.erase(it);
            return true;
        }
    }

    return false;
}

void CZNC::Broadcast(const CString& sMessage, bool bAdminOnly, CUser* pSkipUser,
                     CClient* pSkipClient) {
    for (const auto& it : m_msUsers) {
        if (bAdminOnly && !it.second->IsAdmin()) continue;

        if (it.second != pSkipUser) {
            // TODO: translate message to user's language
            CString sMsg = sMessage;

            bool bContinue = false;
            USERMODULECALL(OnBroadcast(sMsg), it.second, nullptr, &bContinue);
            if (bContinue) continue;

            it.second->PutStatusNotice("*** " + sMsg, nullptr, pSkipClient);
        }
    }
}

CModule* CZNC::FindModule(const CString& sModName, const CString& sUsername) {
    if (sUsername.empty()) {
        return CZNC::Get().GetModules().FindModule(sModName);
    }

    CUser* pUser = FindUser(sUsername);

    return (!pUser) ? nullptr : pUser->GetModules().FindModule(sModName);
}

CModule* CZNC::FindModule(const CString& sModName, CUser* pUser) {
    if (pUser) {
        return pUser->GetModules().FindModule(sModName);
    }

    return CZNC::Get().GetModules().FindModule(sModName);
}

bool CZNC::UpdateModule(const CString& sModule) {
    CModule* pModule;

    map<CUser*, CString> musLoaded;
    map<CIRCNetwork*, CString> mnsLoaded;

    // Unload the module for every user and network
    for (const auto& it : m_msUsers) {
        CUser* pUser = it.second;

        pModule = pUser->GetModules().FindModule(sModule);
        if (pModule) {
            musLoaded[pUser] = pModule->GetArgs();
            pUser->GetModules().UnloadModule(sModule);
        }

        // See if the user has this module loaded to a network
        vector<CIRCNetwork*> vNetworks = pUser->GetNetworks();
        for (CIRCNetwork* pNetwork : vNetworks) {
            pModule = pNetwork->GetModules().FindModule(sModule);
            if (pModule) {
                mnsLoaded[pNetwork] = pModule->GetArgs();
                pNetwork->GetModules().UnloadModule(sModule);
            }
        }
    }

    // Unload the global module
    bool bGlobal = false;
    CString sGlobalArgs;

    pModule = GetModules().FindModule(sModule);
    if (pModule) {
        bGlobal = true;
        sGlobalArgs = pModule->GetArgs();
        GetModules().UnloadModule(sModule);
    }

    // Lets reload everything
    bool bError = false;
    CString sErr;

    // Reload the global module
    if (bGlobal) {
        if (!GetModules().LoadModule(sModule, sGlobalArgs,
                                     CModInfo::GlobalModule, nullptr, nullptr,
                                     sErr)) {
            DEBUG("Failed to reload [" << sModule << "] globally [" << sErr
                                       << "]");
            bError = true;
        }
    }

    // Reload the module for all users
    for (const auto& it : musLoaded) {
        CUser* pUser = it.first;
        const CString& sArgs = it.second;

        if (!pUser->GetModules().LoadModule(
                sModule, sArgs, CModInfo::UserModule, pUser, nullptr, sErr)) {
            DEBUG("Failed to reload [" << sModule << "] for ["
                                       << pUser->GetUserName() << "] [" << sErr
                                       << "]");
            bError = true;
        }
    }

    // Reload the module for all networks
    for (const auto& it : mnsLoaded) {
        CIRCNetwork* pNetwork = it.first;
        const CString& sArgs = it.second;

        if (!pNetwork->GetModules().LoadModule(
                sModule, sArgs, CModInfo::NetworkModule, pNetwork->GetUser(),
                pNetwork, sErr)) {
            DEBUG("Failed to reload ["
                  << sModule << "] for [" << pNetwork->GetUser()->GetUserName()
                  << "/" << pNetwork->GetName() << "] [" << sErr << "]");
            bError = true;
        }
    }

    return !bError;
}

CUser* CZNC::FindUser(const CString& sUsername) {
    map<CString, CUser*>::iterator it = m_msUsers.find(sUsername);

    if (it != m_msUsers.end()) {
        return it->second;
    }

    return nullptr;
}

bool CZNC::DeleteUser(const CString& sUsername) {
    CUser* pUser = FindUser(sUsername);

    if (!pUser) {
        return false;
    }

    m_msDelUsers[pUser->GetUserName()] = pUser;
    return true;
}

bool CZNC::AddUser(CUser* pUser, CString& sErrorRet, bool bStartup) {
    if (FindUser(pUser->GetUserName()) != nullptr) {
        sErrorRet = t_s("User already exists");
        DEBUG("User [" << pUser->GetUserName() << "] - already exists");
        return false;
    }
    if (!pUser->IsValid(sErrorRet)) {
        DEBUG("Invalid user [" << pUser->GetUserName() << "] - [" << sErrorRet
                               << "]");
        return false;
    }
    bool bFailed = false;

    // do not call OnAddUser hook during ZNC startup
    if (!bStartup) {
        GLOBALMODULECALL(OnAddUser(*pUser, sErrorRet), &bFailed);
    }

    if (bFailed) {
        DEBUG("AddUser [" << pUser->GetUserName() << "] aborted by a module ["
                          << sErrorRet << "]");
        return false;
    }
    m_msUsers[pUser->GetUserName()] = pUser;
    return true;
}

CListener* CZNC::FindListener(u_short uPort, const CString& sBindHost,
                              EAddrType eAddr) {
    for (CListener* pListener : m_vpListeners) {
        if (pListener->GetPort() != uPort) continue;
        if (pListener->GetBindHost() != sBindHost) continue;
        if (pListener->GetAddrType() != eAddr) continue;
        return pListener;
    }
    return nullptr;
}

bool CZNC::AddListener(const CString& sLine, CString& sError) {
    CString sName = sLine.Token(0);
    CString sValue = sLine.Token(1, true);

    EAddrType eAddr = ADDR_ALL;
    if (sName.Equals("Listen4") || sName.Equals("Listen") ||
        sName.Equals("Listener4")) {
        eAddr = ADDR_IPV4ONLY;
    }
    if (sName.Equals("Listener6")) {
        eAddr = ADDR_IPV6ONLY;
    }

    CListener::EAcceptType eAccept = CListener::ACCEPT_ALL;
    if (sValue.TrimPrefix("irc_only "))
        eAccept = CListener::ACCEPT_IRC;
    else if (sValue.TrimPrefix("web_only "))
        eAccept = CListener::ACCEPT_HTTP;

    bool bSSL = false;
    CString sPort;
    CString sBindHost;

    if (ADDR_IPV4ONLY == eAddr) {
        sValue.Replace(":", " ");
    }

    if (sValue.Contains(" ")) {
        sBindHost = sValue.Token(0, false, " ");
        sPort = sValue.Token(1, true, " ");
    } else {
        sPort = sValue;
    }

    if (sPort.TrimPrefix("+")) {
        bSSL = true;
    }

    // No support for URIPrefix for old-style configs.
    CString sURIPrefix;
    unsigned short uPort = sPort.ToUShort();
    return AddListener(uPort, sBindHost, sURIPrefix, bSSL, eAddr, eAccept,
                       sError);
}

bool CZNC::AddListener(unsigned short uPort, const CString& sBindHost,
                       const CString& sURIPrefixRaw, bool bSSL, EAddrType eAddr,
                       CListener::EAcceptType eAccept, CString& sError) {
    CString sHostComment;

    if (!sBindHost.empty()) {
        sHostComment = " on host [" + sBindHost + "]";
    }

    CString sIPV6Comment;

    switch (eAddr) {
        case ADDR_ALL:
            sIPV6Comment = "";
            break;
        case ADDR_IPV4ONLY:
            sIPV6Comment = " using ipv4";
            break;
        case ADDR_IPV6ONLY:
            sIPV6Comment = " using ipv6";
    }

    CUtils::PrintAction("Binding to port [" + CString((bSSL) ? "+" : "") +
                        CString(uPort) + "]" + sHostComment + sIPV6Comment);

#ifndef HAVE_IPV6
    if (ADDR_IPV6ONLY == eAddr) {
        sError = t_s("IPv6 is not enabled");
        CUtils::PrintStatus(false, sError);
        return false;
    }
#endif

#ifndef HAVE_LIBSSL
    if (bSSL) {
        sError = t_s("SSL is not enabled");
        CUtils::PrintStatus(false, sError);
        return false;
    }
#else
    CString sPemFile = GetPemLocation();

    if (bSSL && !CFile::Exists(sPemFile)) {
        sError = t_f("Unable to locate pem file: {1}")(sPemFile);
        CUtils::PrintStatus(false, sError);

        // If stdin is e.g. /dev/null and we call GetBoolInput(),
        // we are stuck in an endless loop!
        if (isatty(0) &&
            CUtils::GetBoolInput("Would you like to create a new pem file?",
                                 true)) {
            sError.clear();
            WritePemFile();
        } else {
            return false;
        }

        CUtils::PrintAction("Binding to port [+" + CString(uPort) + "]" +
                            sHostComment + sIPV6Comment);
    }
#endif
    if (!uPort) {
        sError = t_s("Invalid port");
        CUtils::PrintStatus(false, sError);
        return false;
    }

    // URIPrefix must start with a slash and end without one.
    CString sURIPrefix = CString(sURIPrefixRaw);
    if (!sURIPrefix.empty()) {
        if (!sURIPrefix.StartsWith("/")) {
            sURIPrefix = "/" + sURIPrefix;
        }
        if (sURIPrefix.EndsWith("/")) {
            sURIPrefix.TrimRight("/");
        }
    }

    CListener* pListener =
        new CListener(uPort, sBindHost, sURIPrefix, bSSL, eAddr, eAccept);

    if (!pListener->Listen()) {
        sError = FormatBindError();
        CUtils::PrintStatus(false, sError);
        delete pListener;
        return false;
    }

    m_vpListeners.push_back(pListener);
    CUtils::PrintStatus(true);

    return true;
}

bool CZNC::AddListener(CConfig* pConfig, CString& sError) {
    CString sBindHost;
    CString sURIPrefix;
    bool bSSL;
    bool b4;
#ifdef HAVE_IPV6
    bool b6 = true;
#else
    bool b6 = false;
#endif
    bool bIRC;
    bool bWeb;
    unsigned short uPort;
    if (!pConfig->FindUShortEntry("port", uPort)) {
        sError = "No port given";
        CUtils::PrintError(sError);
        return false;
    }
    pConfig->FindStringEntry("host", sBindHost);
    pConfig->FindBoolEntry("ssl", bSSL, false);
    pConfig->FindBoolEntry("ipv4", b4, true);
    pConfig->FindBoolEntry("ipv6", b6, b6);
    pConfig->FindBoolEntry("allowirc", bIRC, true);
    pConfig->FindBoolEntry("allowweb", bWeb, true);
    pConfig->FindStringEntry("uriprefix", sURIPrefix);

    EAddrType eAddr;
    if (b4 && b6) {
        eAddr = ADDR_ALL;
    } else if (b4 && !b6) {
        eAddr = ADDR_IPV4ONLY;
    } else if (!b4 && b6) {
        eAddr = ADDR_IPV6ONLY;
    } else {
        sError = "No address family given";
        CUtils::PrintError(sError);
        return false;
    }

    CListener::EAcceptType eAccept;
    if (bIRC && bWeb) {
        eAccept = CListener::ACCEPT_ALL;
    } else if (bIRC && !bWeb) {
        eAccept = CListener::ACCEPT_IRC;
    } else if (!bIRC && bWeb) {
        eAccept = CListener::ACCEPT_HTTP;
    } else {
        sError = "Either Web or IRC or both should be selected";
        CUtils::PrintError(sError);
        return false;
    }

    return AddListener(uPort, sBindHost, sURIPrefix, bSSL, eAddr, eAccept,
                       sError);
}

bool CZNC::AddListener(CListener* pListener) {
    if (!pListener->GetRealListener()) {
        // Listener doesn't actually listen
        delete pListener;
        return false;
    }

    // We don't check if there is an identical listener already listening
    // since one can't listen on e.g. the same port multiple times

    m_vpListeners.push_back(pListener);
    return true;
}

bool CZNC::DelListener(CListener* pListener) {
    auto it = std::find(m_vpListeners.begin(), m_vpListeners.end(), pListener);
    if (it != m_vpListeners.end()) {
        m_vpListeners.erase(it);
        delete pListener;
        return true;
    }

    return false;
}

CString CZNC::FormatBindError() {
    CString sError = (errno == 0 ? t_s(("unknown error, check the host name"))
                                 : CString(strerror(errno)));
    return t_f("Unable to bind: {1}")(sError);
}

static CZNC* s_pZNC = nullptr;

void CZNC::CreateInstance() {
    if (s_pZNC) abort();

    s_pZNC = new CZNC();
}

CZNC& CZNC::Get() { return *s_pZNC; }

void CZNC::DestroyInstance() {
    delete s_pZNC;
    s_pZNC = nullptr;
}

CZNC::TrafficStatsMap CZNC::GetTrafficStats(TrafficStatsPair& Users,
                                            TrafficStatsPair& ZNC,
                                            TrafficStatsPair& Total) {
    TrafficStatsMap ret;
    unsigned long long uiUsers_in, uiUsers_out, uiZNC_in, uiZNC_out;
    const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

    uiUsers_in = uiUsers_out = 0;
    uiZNC_in = BytesRead();
    uiZNC_out = BytesWritten();

    for (const auto& it : msUsers) {
        ret[it.first] =
            TrafficStatsPair(it.second->BytesRead(), it.second->BytesWritten());
        uiUsers_in += it.second->BytesRead();
        uiUsers_out += it.second->BytesWritten();
    }

    for (Csock* pSock : m_Manager) {
        CUser* pUser = nullptr;
        if (pSock->GetSockName().StartsWith("IRC::")) {
            pUser = ((CIRCSock*)pSock)->GetNetwork()->GetUser();
        } else if (pSock->GetSockName().StartsWith("USR::")) {
            pUser = ((CClient*)pSock)->GetUser();
        }

        if (pUser) {
            ret[pUser->GetUserName()].first += pSock->GetBytesRead();
            ret[pUser->GetUserName()].second += pSock->GetBytesWritten();
            uiUsers_in += pSock->GetBytesRead();
            uiUsers_out += pSock->GetBytesWritten();
        } else {
            uiZNC_in += pSock->GetBytesRead();
            uiZNC_out += pSock->GetBytesWritten();
        }
    }

    Users = TrafficStatsPair(uiUsers_in, uiUsers_out);
    ZNC = TrafficStatsPair(uiZNC_in, uiZNC_out);
    Total = TrafficStatsPair(uiUsers_in + uiZNC_in, uiUsers_out + uiZNC_out);

    return ret;
}

CZNC::TrafficStatsMap CZNC::GetNetworkTrafficStats(const CString& sUsername,
                                                   TrafficStatsPair& Total) {
    TrafficStatsMap Networks;

    CUser* pUser = FindUser(sUsername);
    if (pUser) {
        for (const CIRCNetwork* pNetwork : pUser->GetNetworks()) {
            Networks[pNetwork->GetName()].first = pNetwork->BytesRead();
            Networks[pNetwork->GetName()].second = pNetwork->BytesWritten();
            Total.first += pNetwork->BytesRead();
            Total.second += pNetwork->BytesWritten();
        }

        for (Csock* pSock : m_Manager) {
            CIRCNetwork* pNetwork = nullptr;
            if (pSock->GetSockName().StartsWith("IRC::")) {
                pNetwork = ((CIRCSock*)pSock)->GetNetwork();
            } else if (pSock->GetSockName().StartsWith("USR::")) {
                pNetwork = ((CClient*)pSock)->GetNetwork();
            }

            if (pNetwork && pNetwork->GetUser() == pUser) {
                Networks[pNetwork->GetName()].first = pSock->GetBytesRead();
                Networks[pNetwork->GetName()].second = pSock->GetBytesWritten();
                Total.first += pSock->GetBytesRead();
                Total.second += pSock->GetBytesWritten();
            }
        }
    }

    return Networks;
}

void CZNC::AuthUser(std::shared_ptr<CAuthBase> AuthClass) {
    // TODO unless the auth module calls it, CUser::IsHostAllowed() is not
    // honoured
    bool bReturn = false;
    GLOBALMODULECALL(OnLoginAttempt(AuthClass), &bReturn);
    if (bReturn) return;

    CUser* pUser = FindUser(AuthClass->GetUsername());

    if (!pUser || !pUser->CheckPass(AuthClass->GetPassword())) {
        AuthClass->RefuseLogin("Invalid Password");
        return;
    }

    CString sHost = AuthClass->GetRemoteIP();

    if (!pUser->IsHostAllowed(sHost)) {
        AuthClass->RefuseLogin("Your host [" + sHost + "] is not allowed");
        return;
    }

    AuthClass->AcceptLogin(*pUser);
}

class CConnectQueueTimer : public CCron {
  public:
    CConnectQueueTimer(int iSecs) : CCron() {
        SetName("Connect users");
        Start(iSecs);
        // Don't wait iSecs seconds for first timer run
        m_bRunOnNextCall = true;
    }
    ~CConnectQueueTimer() override {
        // This is only needed when ZNC shuts down:
        // CZNC::~CZNC() sets its CConnectQueueTimer pointer to nullptr and
        // calls the manager's Cleanup() which destroys all sockets and
        // timers. If something calls CZNC::EnableConnectQueue() here
        // (e.g. because a CIRCSock is destroyed), the socket manager
        // deletes that timer almost immediately, but CZNC now got a
        // dangling pointer to this timer which can crash later on.
        //
        // Unlikely but possible ;)
        CZNC::Get().LeakConnectQueueTimer(this);
    }

  protected:
    void RunJob() override {
        list<CIRCNetwork*> ConnectionQueue;
        list<CIRCNetwork*>& RealConnectionQueue =
            CZNC::Get().GetConnectionQueue();

        // Problem: If a network can't connect right now because e.g. it
        // is throttled, it will re-insert itself into the connection
        // queue. However, we must only give each network a single
        // chance during this timer run.
        //
        // Solution: We move the connection queue to our local list at
        // the beginning and work from that.
        ConnectionQueue.swap(RealConnectionQueue);

        while (!ConnectionQueue.empty()) {
            CIRCNetwork* pNetwork = ConnectionQueue.front();
            ConnectionQueue.pop_front();

            if (pNetwork->Connect()) {
                break;
            }
        }

        /* Now re-insert anything that is left in our local list into
         * the real connection queue.
         */
        RealConnectionQueue.splice(RealConnectionQueue.begin(),
                                   ConnectionQueue);

        if (RealConnectionQueue.empty()) {
            DEBUG("ConnectQueueTimer done");
            CZNC::Get().DisableConnectQueue();
        }
    }
};

void CZNC::SetConnectDelay(unsigned int i) {
    if (i < 1) {
        // Don't hammer server with our failed connects
        i = 1;
    }
    if (m_uiConnectDelay != i && m_pConnectQueueTimer != nullptr) {
        m_pConnectQueueTimer->Start(i);
    }
    m_uiConnectDelay = i;
}

VCString CZNC::GetAvailableSSLProtocols() {
    // NOTE: keep in sync with SetSSLProtocols()
    return {"SSLv2", "SSLv3", "TLSv1", "TLSV1.1", "TLSv1.2"};
}

bool CZNC::SetSSLProtocols(const CString& sProtocols) {
    VCString vsProtocols;
    sProtocols.Split(" ", vsProtocols, false, "", "", true, true);

    unsigned int uDisabledProtocols = Csock::EDP_SSL;
    for (CString& sProtocol : vsProtocols) {
        unsigned int uFlag = 0;
        bool bEnable = sProtocol.TrimPrefix("+");
        bool bDisable = sProtocol.TrimPrefix("-");

        // NOTE: keep in sync with GetAvailableSSLProtocols()
        if (sProtocol.Equals("All")) {
            uFlag = ~0;
        } else if (sProtocol.Equals("SSLv2")) {
            uFlag = Csock::EDP_SSLv2;
        } else if (sProtocol.Equals("SSLv3")) {
            uFlag = Csock::EDP_SSLv3;
        } else if (sProtocol.Equals("TLSv1")) {
            uFlag = Csock::EDP_TLSv1;
        } else if (sProtocol.Equals("TLSv1.1")) {
            uFlag = Csock::EDP_TLSv1_1;
        } else if (sProtocol.Equals("TLSv1.2")) {
            uFlag = Csock::EDP_TLSv1_2;
        } else {
            return false;
        }

        if (bEnable) {
            uDisabledProtocols &= ~uFlag;
        } else if (bDisable) {
            uDisabledProtocols |= uFlag;
        } else {
            uDisabledProtocols = ~uFlag;
        }
    }

    m_sSSLProtocols = sProtocols;
    m_uDisabledSSLProtocols = uDisabledProtocols;
    return true;
}

void CZNC::EnableConnectQueue() {
    if (!m_pConnectQueueTimer && !m_uiConnectPaused &&
        !m_lpConnectQueue.empty()) {
        m_pConnectQueueTimer = new CConnectQueueTimer(m_uiConnectDelay);
        GetManager().AddCron(m_pConnectQueueTimer);
    }
}

void CZNC::DisableConnectQueue() {
    if (m_pConnectQueueTimer) {
        // This will kill the cron
        m_pConnectQueueTimer->Stop();
        m_pConnectQueueTimer = nullptr;
    }
}

void CZNC::PauseConnectQueue() {
    DEBUG("Connection queue paused");
    m_uiConnectPaused++;

    if (m_pConnectQueueTimer) {
        m_pConnectQueueTimer->Pause();
    }
}

void CZNC::ResumeConnectQueue() {
    DEBUG("Connection queue resumed");
    m_uiConnectPaused--;

    EnableConnectQueue();
    if (m_pConnectQueueTimer) {
        m_pConnectQueueTimer->UnPause();
    }
}

void CZNC::ForceEncoding() {
    m_uiForceEncoding++;
#ifdef HAVE_ICU
    for (Csock* pSock : GetManager()) {
        if (pSock->GetEncoding().empty()) {
            pSock->SetEncoding("UTF-8");
        }
    }
#endif
}
void CZNC::UnforceEncoding() { m_uiForceEncoding--; }
bool CZNC::IsForcingEncoding() const { return m_uiForceEncoding; }
CString CZNC::FixupEncoding(const CString& sEncoding) const {
    if (sEncoding.empty() && m_uiForceEncoding) {
        return "UTF-8";
    }
    return sEncoding;
}

void CZNC::AddNetworkToQueue(CIRCNetwork* pNetwork) {
    // Make sure we are not already in the queue
    if (std::find(m_lpConnectQueue.begin(), m_lpConnectQueue.end(), pNetwork) !=
        m_lpConnectQueue.end()) {
        return;
    }

    m_lpConnectQueue.push_back(pNetwork);
    EnableConnectQueue();
}

void CZNC::LeakConnectQueueTimer(CConnectQueueTimer* pTimer) {
    if (m_pConnectQueueTimer == pTimer) m_pConnectQueueTimer = nullptr;
}

bool CZNC::WaitForChildLock() { return m_pLockFile && m_pLockFile->ExLock(); }

void CZNC::DisableConfigTimer() {
    if (m_pConfigTimer) {
        m_pConfigTimer->Stop();
        m_pConfigTimer = nullptr;
    }
}
