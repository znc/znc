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

#include <znc/User.h>
#include <znc/Config.h>
#include <znc/FileUtils.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Chan.h>
#include <znc/Query.h>
#include <math.h>
#include <time.h>
#include <algorithm>

using std::vector;
using std::set;

class CUserTimer : public CCron {
  public:
    CUserTimer(CUser* pUser) : CCron(), m_pUser(pUser) {
        SetName("CUserTimer::" + m_pUser->GetUserName());
        Start(m_pUser->GetPingSlack());
    }
    ~CUserTimer() override {}

    CUserTimer(const CUserTimer&) = delete;
    CUserTimer& operator=(const CUserTimer&) = delete;

  private:
  protected:
    void RunJob() override {
        const vector<CClient*>& vUserClients = m_pUser->GetUserClients();

        for (CClient* pUserClient : vUserClients) {
            if (pUserClient->GetTimeSinceLastDataTransaction() >=
                m_pUser->GetPingFrequency()) {
                pUserClient->PutClient("PING :ZNC");
            }
        }

        // Restart timer for the case if the period had changed. Usually this is
        // noop
        Start(m_pUser->GetPingSlack());
    }

    CUser* m_pUser;
};

CUser::CUser(const CString& sUserName)
    : m_sUserName(sUserName),
      m_sCleanUserName(MakeCleanUserName(sUserName)),
      m_sNick(m_sCleanUserName),
      m_sAltNick(""),
      m_sIdent(m_sCleanUserName),
      m_sRealName(""),
      m_sBindHost(""),
      m_sDCCBindHost(""),
      m_sPass(""),
      m_sPassSalt(""),
      m_sStatusPrefix("*"),
      m_sDefaultChanModes(""),
      m_sClientEncoding(""),
      m_sQuitMsg(""),
      m_mssCTCPReplies(),
      m_sTimestampFormat("[%H:%M:%S]"),
      m_sTimezone(""),
      m_eHashType(HASH_NONE),
      m_sUserPath(CZNC::Get().GetUserPath() + "/" + sUserName),
      m_bMultiClients(true),
      m_bDenyLoadMod(false),
      m_bAdmin(false),
      m_bDenySetBindHost(false),
      m_bAutoClearChanBuffer(true),
      m_bAutoClearQueryBuffer(true),
      m_bBeingDeleted(false),
      m_bAppendTimestamp(false),
      m_bPrependTimestamp(true),
      m_bAuthOnlyViaModule(false),
      m_pUserTimer(nullptr),
      m_vIRCNetworks(),
      m_vClients(),
      m_ssAllowedHosts(),
      m_uChanBufferSize(50),
      m_uQueryBufferSize(50),
      m_uBytesRead(0),
      m_uBytesWritten(0),
      m_uMaxJoinTries(10),
      m_uMaxNetworks(1),
      m_uMaxQueryBuffers(50),
      m_uMaxJoins(0),
      m_uNoTrafficTimeout(180),
      m_sSkinName(""),
      m_pModules(new CModules) {
    m_pUserTimer = new CUserTimer(this);
    CZNC::Get().GetManager().AddCron(m_pUserTimer);
}

CUser::~CUser() {
    // Delete networks
    while (!m_vIRCNetworks.empty()) {
        delete *m_vIRCNetworks.begin();
    }

    // Delete clients
    while (!m_vClients.empty()) {
        CZNC::Get().GetManager().DelSockByAddr(m_vClients[0]);
    }
    m_vClients.clear();

    // Delete modules (unloads all modules!)
    delete m_pModules;
    m_pModules = nullptr;

    CZNC::Get().GetManager().DelCronByAddr(m_pUserTimer);

    CZNC::Get().AddBytesRead(m_uBytesRead);
    CZNC::Get().AddBytesWritten(m_uBytesWritten);
}

template <class T>
struct TOption {
    const char* name;
    void (CUser::*pSetter)(T);
};

bool CUser::ParseConfig(CConfig* pConfig, CString& sError) {
    TOption<const CString&> StringOptions[] = {
        {"nick", &CUser::SetNick},
        {"quitmsg", &CUser::SetQuitMsg},
        {"altnick", &CUser::SetAltNick},
        {"ident", &CUser::SetIdent},
        {"realname", &CUser::SetRealName},
        {"chanmodes", &CUser::SetDefaultChanModes},
        {"bindhost", &CUser::SetBindHost},
        {"vhost", &CUser::SetBindHost},
        {"dccbindhost", &CUser::SetDCCBindHost},
        {"dccvhost", &CUser::SetDCCBindHost},
        {"timestampformat", &CUser::SetTimestampFormat},
        {"skin", &CUser::SetSkinName},
        {"clientencoding", &CUser::SetClientEncoding},
    };
    TOption<unsigned int> UIntOptions[] = {
        {"jointries", &CUser::SetJoinTries},
        {"maxnetworks", &CUser::SetMaxNetworks},
        {"maxquerybuffers", &CUser::SetMaxQueryBuffers},
        {"maxjoins", &CUser::SetMaxJoins},
        {"notraffictimeout", &CUser::SetNoTrafficTimeout},
    };
    TOption<bool> BoolOptions[] = {
        {"keepbuffer",
         &CUser::SetKeepBuffer},  // XXX compatibility crap from pre-0.207
        {"autoclearchanbuffer", &CUser::SetAutoClearChanBuffer},
        {"autoclearquerybuffer", &CUser::SetAutoClearQueryBuffer},
        {"multiclients", &CUser::SetMultiClients},
        {"denyloadmod", &CUser::SetDenyLoadMod},
        {"admin", &CUser::SetAdmin},
        {"denysetbindhost", &CUser::SetDenySetBindHost},
        {"denysetvhost", &CUser::SetDenySetBindHost},
        {"appendtimestamp", &CUser::SetTimestampAppend},
        {"prependtimestamp", &CUser::SetTimestampPrepend},
        {"authonlyviamodule", &CUser::SetAuthOnlyViaModule},
    };

    for (const auto& Option : StringOptions) {
        CString sValue;
        if (pConfig->FindStringEntry(Option.name, sValue))
            (this->*Option.pSetter)(sValue);
    }
    for (const auto& Option : UIntOptions) {
        CString sValue;
        if (pConfig->FindStringEntry(Option.name, sValue))
            (this->*Option.pSetter)(sValue.ToUInt());
    }
    for (const auto& Option : BoolOptions) {
        CString sValue;
        if (pConfig->FindStringEntry(Option.name, sValue))
            (this->*Option.pSetter)(sValue.ToBool());
    }

    VCString vsList;
    pConfig->FindStringVector("allow", vsList);
    for (const CString& sHost : vsList) {
        AddAllowedHost(sHost);
    }
    pConfig->FindStringVector("ctcpreply", vsList);
    for (const CString& sReply : vsList) {
        AddCTCPReply(sReply.Token(0), sReply.Token(1, true));
    }

    CString sValue;

    CString sDCCLookupValue;
    pConfig->FindStringEntry("dcclookupmethod", sDCCLookupValue);
    if (pConfig->FindStringEntry("bouncedccs", sValue)) {
        if (sValue.ToBool()) {
            CUtils::PrintAction("Loading Module [bouncedcc]");
            CString sModRet;
            bool bModRet = GetModules().LoadModule(
                "bouncedcc", "", CModInfo::UserModule, this, nullptr, sModRet);

            CUtils::PrintStatus(bModRet, sModRet);
            if (!bModRet) {
                sError = sModRet;
                return false;
            }

            if (sDCCLookupValue.Equals("Client")) {
                GetModules().FindModule("bouncedcc")->SetNV("UseClientIP", "1");
            }
        }
    }
    if (pConfig->FindStringEntry("buffer", sValue))
        SetBufferCount(sValue.ToUInt(), true);
    if (pConfig->FindStringEntry("chanbuffersize", sValue))
        SetChanBufferSize(sValue.ToUInt(), true);
    if (pConfig->FindStringEntry("querybuffersize", sValue))
        SetQueryBufferSize(sValue.ToUInt(), true);
    if (pConfig->FindStringEntry("awaysuffix", sValue)) {
        CUtils::PrintMessage(
            "WARNING: AwaySuffix has been deprecated, instead try -> "
            "LoadModule = awaynick %nick%_" +
            sValue);
    }
    if (pConfig->FindStringEntry("autocycle", sValue)) {
        if (sValue.Equals("true"))
            CUtils::PrintError(
                "WARNING: AutoCycle has been removed, instead try -> "
                "LoadModule = autocycle");
    }
    if (pConfig->FindStringEntry("keepnick", sValue)) {
        if (sValue.Equals("true"))
            CUtils::PrintError(
                "WARNING: KeepNick has been deprecated, instead try -> "
                "LoadModule = keepnick");
    }
    if (pConfig->FindStringEntry("statusprefix", sValue)) {
        if (!SetStatusPrefix(sValue)) {
            sError = "Invalid StatusPrefix [" + sValue +
                     "] Must be 1-5 chars, no spaces.";
            CUtils::PrintError(sError);
            return false;
        }
    }
    if (pConfig->FindStringEntry("timezone", sValue)) {
        SetTimezone(sValue);
    }
    if (pConfig->FindStringEntry("timezoneoffset", sValue)) {
        if (fabs(sValue.ToDouble()) > 0.1) {
            CUtils::PrintError(
                "WARNING: TimezoneOffset has been deprecated, now you can set "
                "your timezone by name");
        }
    }
    if (pConfig->FindStringEntry("timestamp", sValue)) {
        if (!sValue.Trim_n().Equals("true")) {
            if (sValue.Trim_n().Equals("append")) {
                SetTimestampAppend(true);
                SetTimestampPrepend(false);
            } else if (sValue.Trim_n().Equals("prepend")) {
                SetTimestampAppend(false);
                SetTimestampPrepend(true);
            } else if (sValue.Trim_n().Equals("false")) {
                SetTimestampAppend(false);
                SetTimestampPrepend(false);
            } else {
                SetTimestampFormat(sValue);
            }
        }
    }
    if (pConfig->FindStringEntry("language", sValue)) {
        SetLanguage(sValue);
    }
    pConfig->FindStringEntry("pass", sValue);
    // There are different formats for this available:
    // Pass = <plain text>
    // Pass = <md5 hash> -
    // Pass = plain#<plain text>
    // Pass = <hash name>#<hash>
    // Pass = <hash name>#<salted hash>#<salt>#
    // 'Salted hash' means hash of 'password' + 'salt'
    // Possible hashes are md5 and sha256
    if (sValue.TrimSuffix("-")) {
        SetPass(sValue.Trim_n(), CUser::HASH_MD5);
    } else {
        CString sMethod = sValue.Token(0, false, "#");
        CString sPass = sValue.Token(1, true, "#");
        if (sMethod == "md5" || sMethod == "sha256") {
            CUser::eHashType type = CUser::HASH_MD5;
            if (sMethod == "sha256") type = CUser::HASH_SHA256;

            CString sSalt = sPass.Token(1, false, "#");
            sPass = sPass.Token(0, false, "#");
            SetPass(sPass, type, sSalt);
        } else if (sMethod == "plain") {
            SetPass(sPass, CUser::HASH_NONE);
        } else {
            SetPass(sValue, CUser::HASH_NONE);
        }
    }
    CConfig::SubConfig subConf;
    CConfig::SubConfig::const_iterator subIt;
    pConfig->FindSubConfig("pass", subConf);
    if (!sValue.empty() && !subConf.empty()) {
        sError = "Password defined more than once";
        CUtils::PrintError(sError);
        return false;
    }
    subIt = subConf.begin();
    if (subIt != subConf.end()) {
        CConfig* pSubConf = subIt->second.m_pSubConfig;
        CString sHash;
        CString sMethod;
        CString sSalt;
        CUser::eHashType method;
        pSubConf->FindStringEntry("hash", sHash);
        pSubConf->FindStringEntry("method", sMethod);
        pSubConf->FindStringEntry("salt", sSalt);
        if (sMethod.empty() || sMethod.Equals("plain"))
            method = CUser::HASH_NONE;
        else if (sMethod.Equals("md5"))
            method = CUser::HASH_MD5;
        else if (sMethod.Equals("sha256"))
            method = CUser::HASH_SHA256;
        else {
            sError = "Invalid hash method";
            CUtils::PrintError(sError);
            return false;
        }

        SetPass(sHash, method, sSalt);
        if (!pSubConf->empty()) {
            sError = "Unhandled lines in config!";
            CUtils::PrintError(sError);

            CZNC::DumpConfig(pSubConf);
            return false;
        }
        ++subIt;
    }
    if (subIt != subConf.end()) {
        sError = "Password defined more than once";
        CUtils::PrintError(sError);
        return false;
    }

    pConfig->FindSubConfig("network", subConf);
    for (subIt = subConf.begin(); subIt != subConf.end(); ++subIt) {
        const CString& sNetworkName = subIt->first;

        CUtils::PrintMessage("Loading network [" + sNetworkName + "]");

        CIRCNetwork* pNetwork = FindNetwork(sNetworkName);

        if (!pNetwork) {
            pNetwork = new CIRCNetwork(this, sNetworkName);
        }

        if (!pNetwork->ParseConfig(subIt->second.m_pSubConfig, sError)) {
            return false;
        }
    }

    if (pConfig->FindStringVector("server", vsList, false) ||
        pConfig->FindStringVector("chan", vsList, false) ||
        pConfig->FindSubConfig("chan", subConf, false)) {
        CIRCNetwork* pNetwork = FindNetwork("default");
        if (!pNetwork) {
            CString sErrorDummy;
            pNetwork = AddNetwork("default", sErrorDummy);
        }

        if (pNetwork) {
            CUtils::PrintMessage(
                "NOTICE: Found deprecated config, upgrading to a network");

            if (!pNetwork->ParseConfig(pConfig, sError, true)) {
                return false;
            }
        }
    }

    pConfig->FindStringVector("loadmodule", vsList);
    for (const CString& sMod : vsList) {
        CString sModName = sMod.Token(0);
        CString sNotice = "Loading user module [" + sModName + "]";

        // XXX Legacy crap, added in ZNC 0.089
        if (sModName == "discon_kick") {
            sNotice =
                "NOTICE: [discon_kick] was renamed, loading [disconkick] "
                "instead";
            sModName = "disconkick";
        }

        // XXX Legacy crap, added in ZNC 0.099
        if (sModName == "fixfreenode") {
            sNotice =
                "NOTICE: [fixfreenode] doesn't do anything useful anymore, "
                "ignoring it";
            CUtils::PrintMessage(sNotice);
            continue;
        }

        // XXX Legacy crap, added in ZNC 0.207
        if (sModName == "admin") {
            sNotice =
                "NOTICE: [admin] module was renamed, loading [controlpanel] "
                "instead";
            sModName = "controlpanel";
        }

        // XXX Legacy crap, should have been added ZNC 0.207, but added only in
        // 1.1 :(
        if (sModName == "away") {
            sNotice = "NOTICE: [away] was renamed, loading [awaystore] instead";
            sModName = "awaystore";
        }

        // XXX Legacy crap, added in 1.1; fakeonline module was dropped in 1.0
        // and returned in 1.1
        if (sModName == "fakeonline") {
            sNotice =
                "NOTICE: [fakeonline] was renamed, loading [modules_online] "
                "instead";
            sModName = "modules_online";
        }

        // XXX Legacy crap, added in 1.3
        if (sModName == "charset") {
            CUtils::PrintAction(
                "NOTICE: Charset support was moved to core, importing old "
                "charset module settings");
            size_t uIndex = 1;
            if (sMod.Token(uIndex).Equals("-force")) {
                uIndex++;
            }
            VCString vsClient, vsServer;
            sMod.Token(uIndex).Split(",", vsClient);
            sMod.Token(uIndex + 1).Split(",", vsServer);
            if (vsClient.empty() || vsServer.empty()) {
                CUtils::PrintStatus(
                    false, "charset module was loaded with wrong parameters.");
                continue;
            }
            SetClientEncoding(vsClient[0]);
            for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
                pNetwork->SetEncoding(vsServer[0]);
            }
            CUtils::PrintStatus(true, "Using [" + vsClient[0] +
                                          "] for clients, and [" + vsServer[0] +
                                          "] for servers");
            continue;
        }

        CString sModRet;
        CString sArgs = sMod.Token(1, true);

        bool bModRet = LoadModule(sModName, sArgs, sNotice, sModRet);

        CUtils::PrintStatus(bModRet, sModRet);
        if (!bModRet) {
            // XXX The awaynick module was retired in 1.6 (still available as
            // external module)
            if (sModName == "awaynick") {
                // load simple_away instead, unless it's already on the list
                if (std::find(vsList.begin(), vsList.end(), "simple_away") ==
                    vsList.end()) {
                    sNotice = "Loading [simple_away] module instead";
                    sModName = "simple_away";
                    // not a fatal error if simple_away is not available
                    LoadModule(sModName, sArgs, sNotice, sModRet);
                }
            } else {
                sError = sModRet;
                return false;
            }
        }
        continue;
    }

    // Move ircconnectenabled to the networks
    if (pConfig->FindStringEntry("ircconnectenabled", sValue)) {
        for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
            pNetwork->SetIRCConnectEnabled(sValue.ToBool());
        }
    }

    return true;
}

CIRCNetwork* CUser::AddNetwork(const CString& sNetwork, CString& sErrorRet) {
    if (!CIRCNetwork::IsValidNetwork(sNetwork)) {
        sErrorRet =
            t_s("Invalid network name. It should be alphanumeric. Not to be "
                "confused with server name");
        return nullptr;
    } else if (FindNetwork(sNetwork)) {
        sErrorRet = t_f("Network {1} already exists")(sNetwork);
        return nullptr;
    }

    CIRCNetwork* pNetwork = new CIRCNetwork(this, sNetwork);

    bool bCancel = false;
    USERMODULECALL(OnAddNetwork(*pNetwork, sErrorRet), this, nullptr, &bCancel);
    if (bCancel) {
        RemoveNetwork(pNetwork);
        delete pNetwork;
        return nullptr;
    }

    return pNetwork;
}

bool CUser::AddNetwork(CIRCNetwork* pNetwork) {
    if (FindNetwork(pNetwork->GetName())) {
        return false;
    }

    m_vIRCNetworks.push_back(pNetwork);

    return true;
}

void CUser::RemoveNetwork(CIRCNetwork* pNetwork) {
    auto it = std::find(m_vIRCNetworks.begin(), m_vIRCNetworks.end(), pNetwork);
    if (it != m_vIRCNetworks.end()) {
        m_vIRCNetworks.erase(it);
    }
}

bool CUser::DeleteNetwork(const CString& sNetwork) {
    CIRCNetwork* pNetwork = FindNetwork(sNetwork);

    if (pNetwork) {
        bool bCancel = false;
        USERMODULECALL(OnDeleteNetwork(*pNetwork), this, nullptr, &bCancel);
        if (!bCancel) {
            delete pNetwork;
            return true;
        }
    }

    return false;
}

CIRCNetwork* CUser::FindNetwork(const CString& sNetwork) const {
    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        if (pNetwork->GetName().Equals(sNetwork)) {
            return pNetwork;
        }
    }

    return nullptr;
}

const vector<CIRCNetwork*>& CUser::GetNetworks() const {
    return m_vIRCNetworks;
}

CString CUser::ExpandString(const CString& sStr) const {
    CString sRet;
    return ExpandString(sStr, sRet);
}

CString& CUser::ExpandString(const CString& sStr, CString& sRet) const {
    CString sTime = CUtils::CTime(time(nullptr), m_sTimezone);

    sRet = sStr;
    sRet.Replace("%altnick%", GetAltNick());
    sRet.Replace("%bindhost%", GetBindHost());
    sRet.Replace("%defnick%", GetNick());
    sRet.Replace("%ident%", GetIdent());
    sRet.Replace("%nick%", GetNick());
    sRet.Replace("%realname%", GetRealName());
    sRet.Replace("%time%", sTime);
    sRet.Replace("%uptime%", CZNC::Get().GetUptime());
    sRet.Replace("%user%", GetUserName());
    sRet.Replace("%version%", CZNC::GetVersion());
    sRet.Replace("%vhost%", GetBindHost());
    sRet.Replace("%znc%", CZNC::GetTag(false));

    // Allows for escaping ExpandString if necessary, or to prevent
    // defaults from kicking in if you don't want them.
    sRet.Replace("%empty%", "");
    // The following lines do not exist. You must be on DrUgS!
    sRet.Replace("%irc%", "All your IRC are belong to ZNC");
    // Chosen by fair zocchihedron dice roll by SilverLeo
    sRet.Replace("%rand%", "42");

    return sRet;
}

CString CUser::AddTimestamp(const CString& sStr) const {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return AddTimestamp(tv, sStr);
}

CString CUser::AddTimestamp(time_t tm, const CString& sStr) const {
    timeval tv;
    tv.tv_sec = tm;
    tv.tv_usec = 0;
    return AddTimestamp(tv, sStr);
}

CString CUser::AddTimestamp(timeval tv, const CString& sStr) const {
    CString sRet = sStr;

    if (!GetTimestampFormat().empty() &&
        (m_bAppendTimestamp || m_bPrependTimestamp)) {
        CString sTimestamp =
            CUtils::FormatTime(tv, GetTimestampFormat(), m_sTimezone);
        if (sTimestamp.empty()) {
            return sRet;
        }

        if (m_bPrependTimestamp) {
            sRet = sTimestamp;
            sRet += " " + sStr;
        }
        if (m_bAppendTimestamp) {
            // From http://www.mirc.com/colors.html
            // The Control+O key combination in mIRC inserts ascii character 15,
            // which turns off all previous attributes, including color, bold,
            // underline, and italics.
            //
            // \x02 bold
            // \x03 mIRC-compatible color
            // \x04 RRGGBB color
            // \x0F normal/reset (turn off bold, colors, etc.)
            // \x12 reverse (weechat)
            // \x16 reverse (mirc, kvirc)
            // \x1D italic
            // \x1F underline
            // Also see http://www.visualirc.net/tech-attrs.php
            //
            // Keep in sync with CIRCSocket::IcuExt__UCallback
            if (CString::npos !=
                sRet.find_first_of("\x02\x03\x04\x0F\x12\x16\x1D\x1F")) {
                sRet += "\x0F";
            }

            sRet += " " + sTimestamp;
        }
    }

    return sRet;
}

void CUser::BounceAllClients() {
    for (CClient* pClient : m_vClients) {
        pClient->BouncedOff();
    }

    m_vClients.clear();
}

void CUser::UserConnected(CClient* pClient) {
    if (!MultiClients()) {
        BounceAllClients();
    }

    pClient->PutClient(":irc.znc.in 001 " + pClient->GetNick() + " :" +
                       t_s("Welcome to ZNC"));

    m_vClients.push_back(pClient);
}

void CUser::UserDisconnected(CClient* pClient) {
    auto it = std::find(m_vClients.begin(), m_vClients.end(), pClient);
    if (it != m_vClients.end()) {
        m_vClients.erase(it);
    }
}

void CUser::CloneNetworks(const CUser& User) {
    const vector<CIRCNetwork*>& vNetworks = User.GetNetworks();
    for (CIRCNetwork* pUserNetwork : vNetworks) {
        CIRCNetwork* pNetwork = FindNetwork(pUserNetwork->GetName());

        if (pNetwork) {
            pNetwork->Clone(*pUserNetwork);
        } else {
            new CIRCNetwork(this, *pUserNetwork);
        }
    }

    set<CString> ssDeleteNetworks;
    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        if (!(User.FindNetwork(pNetwork->GetName()))) {
            ssDeleteNetworks.insert(pNetwork->GetName());
        }
    }

    for (const CString& sNetwork : ssDeleteNetworks) {
        // The following will move all the clients to the user.
        // So the clients are not disconnected. The client could
        // have requested the rehash. Then when we do
        // client->PutStatus("Rehashing succeeded!") we would
        // crash if there was no client anymore.
        const vector<CClient*>& vClients = FindNetwork(sNetwork)->GetClients();

        while (vClients.begin() != vClients.end()) {
            CClient* pClient = vClients.front();
            // This line will remove pClient from vClients,
            // because it's a reference to the internal Network's vector.
            pClient->SetNetwork(nullptr);
        }

        DeleteNetwork(sNetwork);
    }
}

bool CUser::Clone(const CUser& User, CString& sErrorRet, bool bCloneNetworks) {
    sErrorRet.clear();

    if (!User.IsValid(sErrorRet, true)) {
        return false;
    }

    // user names can only specified for the constructor, changing it later
    // on breaks too much stuff (e.g. lots of paths depend on the user name)
    if (GetUserName() != User.GetUserName()) {
        DEBUG("Ignoring username in CUser::Clone(), old username ["
              << GetUserName() << "]; New username [" << User.GetUserName()
              << "]");
    }

    if (!User.GetPass().empty()) {
        SetPass(User.GetPass(), User.GetPassHashType(), User.GetPassSalt());
    }

    SetNick(User.GetNick(false));
    SetAltNick(User.GetAltNick(false));
    SetIdent(User.GetIdent(false));
    SetRealName(User.GetRealName());
    SetStatusPrefix(User.GetStatusPrefix());
    SetBindHost(User.GetBindHost());
    SetDCCBindHost(User.GetDCCBindHost());
    SetQuitMsg(User.GetQuitMsg());
    SetSkinName(User.GetSkinName());
    SetDefaultChanModes(User.GetDefaultChanModes());
    SetChanBufferSize(User.GetChanBufferSize(), true);
    SetQueryBufferSize(User.GetQueryBufferSize(), true);
    SetJoinTries(User.JoinTries());
    SetMaxNetworks(User.MaxNetworks());
    SetMaxQueryBuffers(User.MaxQueryBuffers());
    SetMaxJoins(User.MaxJoins());
    SetNoTrafficTimeout(User.GetNoTrafficTimeout());
    SetClientEncoding(User.GetClientEncoding());
    SetLanguage(User.GetLanguage());

    // Allowed Hosts
    m_ssAllowedHosts.clear();
    const set<CString>& ssHosts = User.GetAllowedHosts();
    for (const CString& sHost : ssHosts) {
        AddAllowedHost(sHost);
    }

    for (CClient* pSock : m_vClients) {
        if (!IsHostAllowed(pSock->GetRemoteIP())) {
            pSock->PutStatusNotice(
                t_s("You are being disconnected because your IP is no longer "
                    "allowed to connect to this user"));
            pSock->Close();
        }
    }

    // !Allowed Hosts

    // Networks
    if (bCloneNetworks) {
        CloneNetworks(User);
    }
    // !Networks

    // CTCP Replies
    m_mssCTCPReplies.clear();
    const MCString& msReplies = User.GetCTCPReplies();
    for (const auto& it : msReplies) {
        AddCTCPReply(it.first, it.second);
    }
    // !CTCP Replies

    // Flags
    SetAutoClearChanBuffer(User.AutoClearChanBuffer());
    SetAutoClearQueryBuffer(User.AutoClearQueryBuffer());
    SetMultiClients(User.MultiClients());
    SetDenyLoadMod(User.DenyLoadMod());
    SetAdmin(User.IsAdmin());
    SetDenySetBindHost(User.DenySetBindHost());
    SetAuthOnlyViaModule(User.AuthOnlyViaModule());
    SetTimestampAppend(User.GetTimestampAppend());
    SetTimestampPrepend(User.GetTimestampPrepend());
    SetTimestampFormat(User.GetTimestampFormat());
    SetTimezone(User.GetTimezone());
    // !Flags

    // Modules
    set<CString> ssUnloadMods;
    CModules& vCurMods = GetModules();
    const CModules& vNewMods = User.GetModules();

    for (CModule* pNewMod : vNewMods) {
        CString sModRet;
        CModule* pCurMod = vCurMods.FindModule(pNewMod->GetModName());

        if (!pCurMod) {
            vCurMods.LoadModule(pNewMod->GetModName(), pNewMod->GetArgs(),
                                CModInfo::UserModule, this, nullptr, sModRet);
        } else if (pNewMod->GetArgs() != pCurMod->GetArgs()) {
            vCurMods.ReloadModule(pNewMod->GetModName(), pNewMod->GetArgs(),
                                  this, nullptr, sModRet);
        }
    }

    for (CModule* pCurMod : vCurMods) {
        CModule* pNewMod = vNewMods.FindModule(pCurMod->GetModName());

        if (!pNewMod) {
            ssUnloadMods.insert(pCurMod->GetModName());
        }
    }

    for (const CString& sMod : ssUnloadMods) {
        vCurMods.UnloadModule(sMod);
    }
    // !Modules

    return true;
}

const set<CString>& CUser::GetAllowedHosts() const { return m_ssAllowedHosts; }
bool CUser::AddAllowedHost(const CString& sHostMask) {
    if (sHostMask.empty() ||
        m_ssAllowedHosts.find(sHostMask) != m_ssAllowedHosts.end()) {
        return false;
    }

    m_ssAllowedHosts.insert(sHostMask);
    return true;
}
bool CUser::RemAllowedHost(const CString& sHostMask) {
    return m_ssAllowedHosts.erase(sHostMask) > 0;
}
void CUser::ClearAllowedHosts() { m_ssAllowedHosts.clear(); }

bool CUser::IsHostAllowed(const CString& sHost) const {
    if (m_ssAllowedHosts.empty()) {
        return true;
    }

    for (const CString& sAllowedHost : m_ssAllowedHosts) {
        if (CUtils::CheckCIDR(sHost, sAllowedHost)) {
            return true;
        }
    }

    return false;
}

const CString& CUser::GetTimestampFormat() const { return m_sTimestampFormat; }
bool CUser::GetTimestampAppend() const { return m_bAppendTimestamp; }
bool CUser::GetTimestampPrepend() const { return m_bPrependTimestamp; }

bool CUser::IsValidUserName(const CString& sUserName) {
    // /^[a-zA-Z][a-zA-Z@._\-]*$/
    const char* p = sUserName.c_str();

    if (sUserName.empty()) {
        return false;
    }

    if ((*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z')) {
        return false;
    }

    while (*p) {
        if (*p != '@' && *p != '.' && *p != '-' && *p != '_' && !isalnum(*p)) {
            return false;
        }

        p++;
    }

    return true;
}

bool CUser::IsValid(CString& sErrMsg, bool bSkipPass) const {
    sErrMsg.clear();

    if (!bSkipPass && m_sPass.empty()) {
        sErrMsg = t_s("Password is empty");
        return false;
    }

    if (m_sUserName.empty()) {
        sErrMsg = t_s("Username is empty");
        return false;
    }

    if (!CUser::IsValidUserName(m_sUserName)) {
        sErrMsg = t_s("Username is invalid");
        return false;
    }

    return true;
}

CConfig CUser::ToConfig() const {
    CConfig config;
    CConfig passConfig;

    CString sHash;
    switch (m_eHashType) {
        case HASH_NONE:
            sHash = "Plain";
            break;
        case HASH_MD5:
            sHash = "MD5";
            break;
        case HASH_SHA256:
            sHash = "SHA256";
            break;
    }
    passConfig.AddKeyValuePair("Salt", m_sPassSalt);
    passConfig.AddKeyValuePair("Method", sHash);
    passConfig.AddKeyValuePair("Hash", GetPass());
    config.AddSubConfig("Pass", "password", passConfig);

    config.AddKeyValuePair("Nick", GetNick());
    config.AddKeyValuePair("AltNick", GetAltNick());
    config.AddKeyValuePair("Ident", GetIdent());
    config.AddKeyValuePair("RealName", GetRealName());
    config.AddKeyValuePair("BindHost", GetBindHost());
    config.AddKeyValuePair("DCCBindHost", GetDCCBindHost());
    config.AddKeyValuePair("QuitMsg", GetQuitMsg());
    if (CZNC::Get().GetStatusPrefix() != GetStatusPrefix())
        config.AddKeyValuePair("StatusPrefix", GetStatusPrefix());
    config.AddKeyValuePair("Skin", GetSkinName());
    config.AddKeyValuePair("ChanModes", GetDefaultChanModes());
    config.AddKeyValuePair("ChanBufferSize", CString(GetChanBufferSize()));
    config.AddKeyValuePair("QueryBufferSize", CString(GetQueryBufferSize()));
    config.AddKeyValuePair("AutoClearChanBuffer",
                           CString(AutoClearChanBuffer()));
    config.AddKeyValuePair("AutoClearQueryBuffer",
                           CString(AutoClearQueryBuffer()));
    config.AddKeyValuePair("MultiClients", CString(MultiClients()));
    config.AddKeyValuePair("DenyLoadMod", CString(DenyLoadMod()));
    config.AddKeyValuePair("Admin", CString(IsAdmin()));
    config.AddKeyValuePair("DenySetBindHost", CString(DenySetBindHost()));
    config.AddKeyValuePair("TimestampFormat", GetTimestampFormat());
    config.AddKeyValuePair("AppendTimestamp", CString(GetTimestampAppend()));
    config.AddKeyValuePair("PrependTimestamp", CString(GetTimestampPrepend()));
    config.AddKeyValuePair("AuthOnlyViaModule", CString(AuthOnlyViaModule()));
    config.AddKeyValuePair("Timezone", m_sTimezone);
    config.AddKeyValuePair("JoinTries", CString(m_uMaxJoinTries));
    config.AddKeyValuePair("MaxNetworks", CString(m_uMaxNetworks));
    config.AddKeyValuePair("MaxQueryBuffers", CString(m_uMaxQueryBuffers));
    config.AddKeyValuePair("MaxJoins", CString(m_uMaxJoins));
    config.AddKeyValuePair("ClientEncoding", GetClientEncoding());
    config.AddKeyValuePair("Language", GetLanguage());
    config.AddKeyValuePair("NoTrafficTimeout", CString(GetNoTrafficTimeout()));

    // Allow Hosts
    if (!m_ssAllowedHosts.empty()) {
        for (const CString& sHost : m_ssAllowedHosts) {
            config.AddKeyValuePair("Allow", sHost);
        }
    }

    // CTCP Replies
    if (!m_mssCTCPReplies.empty()) {
        for (const auto& itb : m_mssCTCPReplies) {
            config.AddKeyValuePair("CTCPReply",
                                   itb.first.AsUpper() + " " + itb.second);
        }
    }

    // Modules
    const CModules& Mods = GetModules();

    if (!Mods.empty()) {
        for (CModule* pMod : Mods) {
            CString sArgs = pMod->GetArgs();

            if (!sArgs.empty()) {
                sArgs = " " + sArgs;
            }

            config.AddKeyValuePair("LoadModule", pMod->GetModName() + sArgs);
        }
    }

    // Networks
    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        config.AddSubConfig("Network", pNetwork->GetName(),
                            pNetwork->ToConfig());
    }

    return config;
}

bool CUser::CheckPass(const CString& sPass) const {
    if(AuthOnlyViaModule() || CZNC::Get().GetAuthOnlyViaModule()) {
        return false;
    }

    switch (m_eHashType) {
        case HASH_MD5:
            return m_sPass.Equals(CUtils::SaltedMD5Hash(sPass, m_sPassSalt));
        case HASH_SHA256:
            return m_sPass.Equals(CUtils::SaltedSHA256Hash(sPass, m_sPassSalt));
        case HASH_NONE:
        default:
            return (sPass == m_sPass);
    }
}

/*CClient* CUser::GetClient() {
    // Todo: optimize this by saving a pointer to the sock
    CSockManager& Manager = CZNC::Get().GetManager();
    CString sSockName = "USR::" + m_sUserName;

    for (unsigned int a = 0; a < Manager.size(); a++) {
        Csock* pSock = Manager[a];
        if (pSock->GetSockName().Equals(sSockName)) {
            if (!pSock->IsClosed()) {
                return (CClient*) pSock;
            }
        }
    }

    return (CClient*) CZNC::Get().GetManager().FindSockByName(sSockName);
}*/

CString CUser::GetLocalDCCIP() const {
    if (!GetDCCBindHost().empty()) return GetDCCBindHost();

    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        CIRCSock* pIRCSock = pNetwork->GetIRCSock();
        if (pIRCSock) {
            return pIRCSock->GetLocalIP();
        }
    }

    if (!GetAllClients().empty()) {
        return GetAllClients()[0]->GetLocalIP();
    }

    return "";
}

bool CUser::PutUser(const CString& sLine, CClient* pClient,
                    CClient* pSkipClient) {
    for (CClient* pEachClient : m_vClients) {
        if ((!pClient || pClient == pEachClient) &&
            pSkipClient != pEachClient) {
            pEachClient->PutClient(sLine);

            if (pClient) {
                return true;
            }
        }
    }

    return (pClient == nullptr);
}

bool CUser::PutAllUser(const CString& sLine, CClient* pClient,
                       CClient* pSkipClient) {
    PutUser(sLine, pClient, pSkipClient);

    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        if (pNetwork->PutUser(sLine, pClient, pSkipClient)) {
            return true;
        }
    }

    return (pClient == nullptr);
}

bool CUser::PutStatus(const CString& sLine, CClient* pClient,
                      CClient* pSkipClient) {
    vector<CClient*> vClients = GetAllClients();
    for (CClient* pEachClient : vClients) {
        if ((!pClient || pClient == pEachClient) &&
            pSkipClient != pEachClient) {
            pEachClient->PutStatus(sLine);

            if (pClient) {
                return true;
            }
        }
    }

    return (pClient == nullptr);
}

bool CUser::PutStatusNotice(const CString& sLine, CClient* pClient,
                            CClient* pSkipClient) {
    vector<CClient*> vClients = GetAllClients();
    for (CClient* pEachClient : vClients) {
        if ((!pClient || pClient == pEachClient) &&
            pSkipClient != pEachClient) {
            pEachClient->PutStatusNotice(sLine);

            if (pClient) {
                return true;
            }
        }
    }

    return (pClient == nullptr);
}

bool CUser::PutModule(const CString& sModule, const CString& sLine,
                      CClient* pClient, CClient* pSkipClient) {
    for (CClient* pEachClient : m_vClients) {
        if ((!pClient || pClient == pEachClient) &&
            pSkipClient != pEachClient) {
            pEachClient->PutModule(sModule, sLine);

            if (pClient) {
                return true;
            }
        }
    }

    return (pClient == nullptr);
}

bool CUser::PutModNotice(const CString& sModule, const CString& sLine,
                         CClient* pClient, CClient* pSkipClient) {
    for (CClient* pEachClient : m_vClients) {
        if ((!pClient || pClient == pEachClient) &&
            pSkipClient != pEachClient) {
            pEachClient->PutModNotice(sModule, sLine);

            if (pClient) {
                return true;
            }
        }
    }

    return (pClient == nullptr);
}

CString CUser::MakeCleanUserName(const CString& sUserName) {
    return sUserName.Token(0, false, "@").Replace_n(".", "");
}

bool CUser::IsUserAttached() const {
    if (!m_vClients.empty()) {
        return true;
    }

    for (const CIRCNetwork* pNetwork : m_vIRCNetworks) {
        if (pNetwork->IsUserAttached()) {
            return true;
        }
    }

    return false;
}

bool CUser::LoadModule(const CString& sModName, const CString& sArgs,
                       const CString& sNotice, CString& sError) {
    bool bModRet = true;
    CString sModRet;

    CModInfo ModInfo;
    if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sModName, sModRet)) {
        sError = t_f("Unable to find modinfo {1}: {2}")(sModName, sModRet);
        return false;
    }

    CUtils::PrintAction(sNotice);

    if (!ModInfo.SupportsType(CModInfo::UserModule) &&
        ModInfo.SupportsType(CModInfo::NetworkModule)) {
        CUtils::PrintMessage(
            "NOTICE: Module [" + sModName +
            "] is a network module, loading module for all networks in user.");

        // Do they have old NV?
        CFile fNVFile =
            CFile(GetUserPath() + "/moddata/" + sModName + "/.registry");

        for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
            // Check whether the network already has this module loaded (#954)
            if (pNetwork->GetModules().FindModule(sModName)) {
                continue;
            }

            if (fNVFile.Exists()) {
                CString sNetworkModPath =
                    pNetwork->GetNetworkPath() + "/moddata/" + sModName;
                if (!CFile::Exists(sNetworkModPath)) {
                    CDir::MakeDir(sNetworkModPath);
                }

                fNVFile.Copy(sNetworkModPath + "/.registry");
            }

            bModRet = pNetwork->GetModules().LoadModule(
                sModName, sArgs, CModInfo::NetworkModule, this, pNetwork,
                sModRet);
            if (!bModRet) {
                break;
            }
        }
    } else {
        bModRet = GetModules().LoadModule(sModName, sArgs, CModInfo::UserModule,
                                          this, nullptr, sModRet);
    }

    if (!bModRet) {
        sError = sModRet;
    }
    return bModRet;
}

// Setters
void CUser::SetNick(const CString& s) { m_sNick = s; }
void CUser::SetAltNick(const CString& s) { m_sAltNick = s; }
void CUser::SetIdent(const CString& s) { m_sIdent = s; }
void CUser::SetRealName(const CString& s) { m_sRealName = s; }
void CUser::SetBindHost(const CString& s) { m_sBindHost = s; }
void CUser::SetDCCBindHost(const CString& s) { m_sDCCBindHost = s; }
void CUser::SetPass(const CString& s, eHashType eHash, const CString& sSalt) {
    m_sPass = s;
    m_eHashType = eHash;
    m_sPassSalt = sSalt;
}
void CUser::SetMultiClients(bool b) { m_bMultiClients = b; }
void CUser::SetDenyLoadMod(bool b) { m_bDenyLoadMod = b; }
void CUser::SetAdmin(bool b) { m_bAdmin = b; }
void CUser::SetDenySetBindHost(bool b) { m_bDenySetBindHost = b; }
void CUser::SetDefaultChanModes(const CString& s) { m_sDefaultChanModes = s; }
void CUser::SetClientEncoding(const CString& s) {
    m_sClientEncoding = s;
    for (CClient* pClient : GetAllClients()) {
        pClient->SetEncoding(s);
    }
}
void CUser::SetQuitMsg(const CString& s) { m_sQuitMsg = s; }
void CUser::SetAutoClearChanBuffer(bool b) {
    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        for (CChan* pChan : pNetwork->GetChans()) {
            pChan->InheritAutoClearChanBuffer(b);
        }
    }
    m_bAutoClearChanBuffer = b;
}
void CUser::SetAutoClearQueryBuffer(bool b) { m_bAutoClearQueryBuffer = b; }

bool CUser::SetBufferCount(unsigned int u, bool bForce) {
    return SetChanBufferSize(u, bForce);
}

bool CUser::SetChanBufferSize(unsigned int u, bool bForce) {
    if (!bForce && u > CZNC::Get().GetMaxBufferSize()) return false;
    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        for (CChan* pChan : pNetwork->GetChans()) {
            pChan->InheritBufferCount(u, bForce);
        }
    }
    m_uChanBufferSize = u;
    return true;
}

bool CUser::SetQueryBufferSize(unsigned int u, bool bForce) {
    if (!bForce && u > CZNC::Get().GetMaxBufferSize()) return false;
    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        for (CQuery* pQuery : pNetwork->GetQueries()) {
            pQuery->SetBufferCount(u, bForce);
        }
    }
    m_uQueryBufferSize = u;
    return true;
}

bool CUser::AddCTCPReply(const CString& sCTCP, const CString& sReply) {
    // Reject CTCP requests containing spaces
    if (sCTCP.find_first_of(' ') != CString::npos) {
        return false;
    }
    // Reject empty CTCP requests
    if (sCTCP.empty()) {
        return false;
    }
    m_mssCTCPReplies[sCTCP.AsUpper()] = sReply;
    return true;
}

bool CUser::DelCTCPReply(const CString& sCTCP) {
    return m_mssCTCPReplies.erase(sCTCP.AsUpper()) > 0;
}

bool CUser::SetStatusPrefix(const CString& s) {
    if ((!s.empty()) && (s.length() < 6) && (!s.Contains(" "))) {
        m_sStatusPrefix = (s.empty()) ? "*" : s;
        return true;
    }

    return false;
}

bool CUser::SetLanguage(const CString& s) {
    // They look like ru-RU
    for (char c : s) {
        if (isalpha(c) || c == '-' || c == '_') {
        } else {
            return false;
        }
    }
    m_sLanguage = s;
    // 1.7.0 accidentally used _ instead of -, which made language
    // non-selectable. But it's possible that someone put _ to znc.conf
    // manually.
    // TODO: cleanup _ some time later.
    m_sLanguage.Replace("_", "-");
    return true;
}
// !Setters

// Getters
vector<CClient*> CUser::GetAllClients() const {
    vector<CClient*> vClients;

    for (CIRCNetwork* pNetwork : m_vIRCNetworks) {
        for (CClient* pClient : pNetwork->GetClients()) {
            vClients.push_back(pClient);
        }
    }

    for (CClient* pClient : m_vClients) {
        vClients.push_back(pClient);
    }

    return vClients;
}

const CString& CUser::GetUserName() const { return m_sUserName; }
const CString& CUser::GetCleanUserName() const { return m_sCleanUserName; }
const CString& CUser::GetNick(bool bAllowDefault) const {
    return (bAllowDefault && m_sNick.empty()) ? GetCleanUserName() : m_sNick;
}
const CString& CUser::GetAltNick(bool bAllowDefault) const {
    return (bAllowDefault && m_sAltNick.empty()) ? GetCleanUserName()
                                                 : m_sAltNick;
}
const CString& CUser::GetIdent(bool bAllowDefault) const {
    return (bAllowDefault && m_sIdent.empty()) ? GetCleanUserName() : m_sIdent;
}
CString CUser::GetRealName() const {
    // Not include version number via GetTag() because of
    // https://github.com/znc/znc/issues/818#issuecomment-70402820
    return (!m_sRealName.Trim_n().empty()) ? m_sRealName
                                           : "ZNC - https://znc.in";
}
const CString& CUser::GetBindHost() const { return m_sBindHost; }
const CString& CUser::GetDCCBindHost() const { return m_sDCCBindHost; }
const CString& CUser::GetPass() const { return m_sPass; }
CUser::eHashType CUser::GetPassHashType() const { return m_eHashType; }
const CString& CUser::GetPassSalt() const { return m_sPassSalt; }
bool CUser::DenyLoadMod() const { return m_bDenyLoadMod; }
bool CUser::IsAdmin() const { return m_bAdmin; }
bool CUser::DenySetBindHost() const { return m_bDenySetBindHost; }
bool CUser::MultiClients() const { return m_bMultiClients; }
bool CUser::AuthOnlyViaModule() const { return m_bAuthOnlyViaModule; }
const CString& CUser::GetStatusPrefix() const { return m_sStatusPrefix; }
const CString& CUser::GetDefaultChanModes() const {
    return m_sDefaultChanModes;
}
const CString& CUser::GetClientEncoding() const { return m_sClientEncoding; }
bool CUser::HasSpaceForNewNetwork() const {
    return GetNetworks().size() < MaxNetworks();
}

CString CUser::GetQuitMsg() const {
    return (!m_sQuitMsg.Trim_n().empty()) ? m_sQuitMsg : "%znc%";
}
const MCString& CUser::GetCTCPReplies() const { return m_mssCTCPReplies; }
unsigned int CUser::GetBufferCount() const { return GetChanBufferSize(); }
unsigned int CUser::GetChanBufferSize() const { return m_uChanBufferSize; }
unsigned int CUser::GetQueryBufferSize() const { return m_uQueryBufferSize; }
bool CUser::AutoClearChanBuffer() const { return m_bAutoClearChanBuffer; }
bool CUser::AutoClearQueryBuffer() const { return m_bAutoClearQueryBuffer; }
// CString CUser::GetSkinName() const { return (!m_sSkinName.empty()) ?
// m_sSkinName : CZNC::Get().GetSkinName(); }
CString CUser::GetSkinName() const { return m_sSkinName; }
CString CUser::GetLanguage() const { return m_sLanguage; }
const CString& CUser::GetUserPath() const {
    if (!CFile::Exists(m_sUserPath)) {
        CDir::MakeDir(m_sUserPath);
    }
    return m_sUserPath;
}
// !Getters

unsigned long long CUser::BytesRead() const {
    unsigned long long uBytes = m_uBytesRead;
    for (const CIRCNetwork* pNetwork : m_vIRCNetworks) {
        uBytes += pNetwork->BytesRead();
    }
    return uBytes;
}

unsigned long long CUser::BytesWritten() const {
    unsigned long long uBytes = m_uBytesWritten;
    for (const CIRCNetwork* pNetwork : m_vIRCNetworks) {
        uBytes += pNetwork->BytesWritten();
    }
    return uBytes;
}
