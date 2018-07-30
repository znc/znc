/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 * Copyright (C) 2008 by Stefan Rado
 * based on admin.cpp by Sebastian Ramacher
 * based on admin.cpp in crox branch
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
#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>

using std::map;
using std::vector;

template <std::size_t N>
struct array_size_helper {
    char __place_holder[N];
};

template <class T, std::size_t N>
static array_size_helper<N> array_size(T (&)[N]) {
    return array_size_helper<N>();
}

#define ARRAY_SIZE(array) sizeof(array_size((array)))

class CAdminMod : public CModule {
    using CModule::PutModule;

    struct Setting {
        const char* name;
        CString type;
    };

    void PrintVarsHelp(const CString& sFilter, const Setting vars[],
                       unsigned int uSize, const CString& sDescription) {
        CTable VarTable;
        VarTable.AddColumn(t_s("Type", "helptable"));
        VarTable.AddColumn(t_s("Variables", "helptable"));
        std::map<CString, VCString> mvsTypedVariables;
        for (unsigned int i = 0; i != uSize; ++i) {
            CString sVar = CString(vars[i].name).AsLower();
            if (sFilter.empty() || sVar.StartsWith(sFilter) ||
                sVar.WildCmp(sFilter)) {
                mvsTypedVariables[vars[i].type].emplace_back(vars[i].name);
            }
        }
        for (const auto& i : mvsTypedVariables) {
            VarTable.AddRow();
            VarTable.SetCell(t_s("Type", "helptable"), i.first);
            VarTable.SetCell(
                t_s("Variables", "helptable"),
                CString(", ").Join(i.second.cbegin(), i.second.cend()));
        }
        if (!VarTable.empty()) {
            PutModule(sDescription);
            PutModule(VarTable);
        }
    }

    void PrintHelp(const CString& sLine) {
        HandleHelpCommand(sLine);

        const CString str = t_s("String");
        const CString boolean = t_s("Boolean (true/false)");
        const CString integer = t_s("Integer");
        const CString number = t_s("Number");

        const CString sCmdFilter = sLine.Token(1, false);
        const CString sVarFilter = sLine.Token(2, true).AsLower();

        if (sCmdFilter.empty() || sCmdFilter.StartsWith("Set") ||
            sCmdFilter.StartsWith("Get")) {
            Setting vars[] = {
                {"Nick", str},
                {"Altnick", str},
                {"Ident", str},
                {"RealName", str},
                {"BindHost", str},
                {"MultiClients", boolean},
                {"DenyLoadMod", boolean},
                {"DenySetBindHost", boolean},
                {"DefaultChanModes", str},
                {"QuitMsg", str},
                {"ChanBufferSize", integer},
                {"QueryBufferSize", integer},
                {"AutoClearChanBuffer", boolean},
                {"AutoClearQueryBuffer", boolean},
                {"Password", str},
                {"JoinTries", integer},
                {"MaxJoins", integer},
                {"MaxNetworks", integer},
                {"MaxQueryBuffers", integer},
                {"Timezone", str},
                {"Admin", boolean},
                {"AppendTimestamp", boolean},
                {"PrependTimestamp", boolean},
                {"AuthOnlyViaModule", boolean},
                {"TimestampFormat", str},
                {"DCCBindHost", str},
                {"StatusPrefix", str},
#ifdef HAVE_I18N
                {"Language", str},
#endif
#ifdef HAVE_ICU
                {"ClientEncoding", str},
#endif
            };
            PrintVarsHelp(sVarFilter, vars, ARRAY_SIZE(vars),
                          t_s("The following variables are available when "
                              "using the Set/Get commands:"));
        }

        if (sCmdFilter.empty() || sCmdFilter.StartsWith("SetNetwork") ||
            sCmdFilter.StartsWith("GetNetwork")) {
            Setting nvars[] = {
                {"Nick", str},
                {"Altnick", str},
                {"Ident", str},
                {"RealName", str},
                {"BindHost", str},
                {"FloodRate", number},
                {"FloodBurst", integer},
                {"JoinDelay", integer},
#ifdef HAVE_ICU
                {"Encoding", str},
#endif
                {"QuitMsg", str},
                {"TrustAllCerts", boolean},
                {"TrustPKI", boolean},
            };
            PrintVarsHelp(sVarFilter, nvars, ARRAY_SIZE(nvars),
                          t_s("The following variables are available when "
                              "using the SetNetwork/GetNetwork commands:"));
        }

        if (sCmdFilter.empty() || sCmdFilter.StartsWith("SetChan") ||
            sCmdFilter.StartsWith("GetChan")) {
            Setting cvars[] = {{"DefModes", str},
                               {"Key", str},
                               {"BufferSize", integer},
                               {"InConfig", boolean},
                               {"AutoClearChanBuffer", boolean},
                               {"Detached", boolean}};
            PrintVarsHelp(sVarFilter, cvars, ARRAY_SIZE(cvars),
                          t_s("The following variables are available when "
                              "using the SetChan/GetChan commands:"));
        }

        if (sCmdFilter.empty())
            PutModule(
                t_s("You can use $user as the user name and $network as the "
                    "network name for modifying your own user and network."));
    }

    CUser* FindUser(const CString& sUsername) {
        if (sUsername.Equals("$me") || sUsername.Equals("$user"))
            return GetUser();
        CUser* pUser = CZNC::Get().FindUser(sUsername);
        if (!pUser) {
            PutModule(t_f("Error: User [{1}] does not exist!")(sUsername));
            return nullptr;
        }
        if (pUser != GetUser() && !GetUser()->IsAdmin()) {
            PutModule(t_s(
                "Error: You need to have admin rights to modify other users!"));
            return nullptr;
        }
        return pUser;
    }

    CIRCNetwork* FindNetwork(CUser* pUser, const CString& sNetwork) {
        if (sNetwork.Equals("$net") || sNetwork.Equals("$network")) {
            if (pUser != GetUser()) {
                PutModule(t_s(
                    "Error: You cannot use $network to modify other users!"));
                return nullptr;
            }
            return CModule::GetNetwork();
        }
        CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
        if (!pNetwork) {
            PutModule(
                t_f("Error: User {1} does not have a network named [{2}].")(
                    pUser->GetUserName(), sNetwork));
        }
        return pNetwork;
    }

    void Get(const CString& sLine) {
        const CString sVar = sLine.Token(1).AsLower();
        CString sUsername = sLine.Token(2, true);
        CUser* pUser;

        if (sVar.empty()) {
            PutModule(t_s("Usage: Get <variable> [username]"));
            return;
        }

        if (sUsername.empty()) {
            pUser = GetUser();
        } else {
            pUser = FindUser(sUsername);
        }

        if (!pUser) return;

        if (sVar == "nick")
            PutModule("Nick = " + pUser->GetNick());
        else if (sVar == "altnick")
            PutModule("AltNick = " + pUser->GetAltNick());
        else if (sVar == "ident")
            PutModule("Ident = " + pUser->GetIdent());
        else if (sVar == "realname")
            PutModule("RealName = " + pUser->GetRealName());
        else if (sVar == "bindhost")
            PutModule("BindHost = " + pUser->GetBindHost());
        else if (sVar == "multiclients")
            PutModule("MultiClients = " + CString(pUser->MultiClients()));
        else if (sVar == "denyloadmod")
            PutModule("DenyLoadMod = " + CString(pUser->DenyLoadMod()));
        else if (sVar == "denysetbindhost")
            PutModule("DenySetBindHost = " + CString(pUser->DenySetBindHost()));
        else if (sVar == "defaultchanmodes")
            PutModule("DefaultChanModes = " + pUser->GetDefaultChanModes());
        else if (sVar == "quitmsg")
            PutModule("QuitMsg = " + pUser->GetQuitMsg());
        else if (sVar == "buffercount")
            PutModule("BufferCount = " + CString(pUser->GetBufferCount()));
        else if (sVar == "chanbuffersize")
            PutModule("ChanBufferSize = " +
                      CString(pUser->GetChanBufferSize()));
        else if (sVar == "querybuffersize")
            PutModule("QueryBufferSize = " +
                      CString(pUser->GetQueryBufferSize()));
        else if (sVar == "keepbuffer")
            // XXX compatibility crap, added in 0.207
            PutModule("KeepBuffer = " + CString(!pUser->AutoClearChanBuffer()));
        else if (sVar == "autoclearchanbuffer")
            PutModule("AutoClearChanBuffer = " +
                      CString(pUser->AutoClearChanBuffer()));
        else if (sVar == "autoclearquerybuffer")
            PutModule("AutoClearQueryBuffer = " +
                      CString(pUser->AutoClearQueryBuffer()));
        else if (sVar == "maxjoins")
            PutModule("MaxJoins = " + CString(pUser->MaxJoins()));
        else if (sVar == "notraffictimeout")
            PutModule("NoTrafficTimeout = " +
                      CString(pUser->GetNoTrafficTimeout()));
        else if (sVar == "maxnetworks")
            PutModule("MaxNetworks = " + CString(pUser->MaxNetworks()));
        else if (sVar == "maxquerybuffers")
            PutModule("MaxQueryBuffers = " + CString(pUser->MaxQueryBuffers()));
        else if (sVar == "jointries")
            PutModule("JoinTries = " + CString(pUser->JoinTries()));
        else if (sVar == "timezone")
            PutModule("Timezone = " + pUser->GetTimezone());
        else if (sVar == "appendtimestamp")
            PutModule("AppendTimestamp = " +
                      CString(pUser->GetTimestampAppend()));
        else if (sVar == "prependtimestamp")
            PutModule("PrependTimestamp = " +
                      CString(pUser->GetTimestampPrepend()));
        else if (sVar == "authonlyviamodule")
            PutModule("AuthOnlyViaModule = " +
                      CString(pUser->AuthOnlyViaModule()));
        else if (sVar == "timestampformat")
            PutModule("TimestampFormat = " + pUser->GetTimestampFormat());
        else if (sVar == "dccbindhost")
            PutModule("DCCBindHost = " + CString(pUser->GetDCCBindHost()));
        else if (sVar == "admin")
            PutModule("Admin = " + CString(pUser->IsAdmin()));
        else if (sVar == "statusprefix")
            PutModule("StatusPrefix = " + pUser->GetStatusPrefix());
#ifdef HAVE_I18N
        else if (sVar == "language")
            PutModule("Language = " + (pUser->GetLanguage().empty()
                                           ? "en"
                                           : pUser->GetLanguage()));
#endif
#ifdef HAVE_ICU
        else if (sVar == "clientencoding")
            PutModule("ClientEncoding = " + pUser->GetClientEncoding());
#endif
        else
            PutModule(t_s("Error: Unknown variable"));
    }

    void Set(const CString& sLine) {
        const CString sVar = sLine.Token(1).AsLower();
        CString sUserName = sLine.Token(2);
        CString sValue = sLine.Token(3, true);

        if (sValue.empty()) {
            PutModule(t_s("Usage: Set <variable> <username> <value>"));
            return;
        }

        CUser* pUser = FindUser(sUserName);
        if (!pUser) return;

        if (sVar == "nick") {
            pUser->SetNick(sValue);
            PutModule("Nick = " + sValue);
        } else if (sVar == "altnick") {
            pUser->SetAltNick(sValue);
            PutModule("AltNick = " + sValue);
        } else if (sVar == "ident") {
            pUser->SetIdent(sValue);
            PutModule("Ident = " + sValue);
        } else if (sVar == "realname") {
            pUser->SetRealName(sValue);
            PutModule("RealName = " + sValue);
        } else if (sVar == "bindhost") {
            if (!pUser->DenySetBindHost() || GetUser()->IsAdmin()) {
                if (sValue.Equals(pUser->GetBindHost())) {
                    PutModule(t_s("This bind host is already set!"));
                    return;
                }

                pUser->SetBindHost(sValue);
                PutModule("BindHost = " + sValue);
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "multiclients") {
            bool b = sValue.ToBool();
            pUser->SetMultiClients(b);
            PutModule("MultiClients = " + CString(b));
        } else if (sVar == "denyloadmod") {
            if (GetUser()->IsAdmin()) {
                bool b = sValue.ToBool();
                pUser->SetDenyLoadMod(b);
                PutModule("DenyLoadMod = " + CString(b));
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "denysetbindhost") {
            if (GetUser()->IsAdmin()) {
                bool b = sValue.ToBool();
                pUser->SetDenySetBindHost(b);
                PutModule("DenySetBindHost = " + CString(b));
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "defaultchanmodes") {
            pUser->SetDefaultChanModes(sValue);
            PutModule("DefaultChanModes = " + sValue);
        } else if (sVar == "quitmsg") {
            pUser->SetQuitMsg(sValue);
            PutModule("QuitMsg = " + sValue);
        } else if (sVar == "chanbuffersize" || sVar == "buffercount") {
            unsigned int i = sValue.ToUInt();
            // Admins don't have to honour the buffer limit
            if (pUser->SetChanBufferSize(i, GetUser()->IsAdmin())) {
                PutModule("ChanBufferSize = " + sValue);
            } else {
                PutModule(t_f("Setting failed, limit for buffer size is {1}")(
                    CString(CZNC::Get().GetMaxBufferSize())));
            }
        } else if (sVar == "querybuffersize") {
            unsigned int i = sValue.ToUInt();
            // Admins don't have to honour the buffer limit
            if (pUser->SetQueryBufferSize(i, GetUser()->IsAdmin())) {
                PutModule("QueryBufferSize = " + sValue);
            } else {
                PutModule(t_f("Setting failed, limit for buffer size is {1}")(
                    CString(CZNC::Get().GetMaxBufferSize())));
            }
        } else if (sVar == "keepbuffer") {
            // XXX compatibility crap, added in 0.207
            bool b = !sValue.ToBool();
            pUser->SetAutoClearChanBuffer(b);
            PutModule("AutoClearChanBuffer = " + CString(b));
        } else if (sVar == "autoclearchanbuffer") {
            bool b = sValue.ToBool();
            pUser->SetAutoClearChanBuffer(b);
            PutModule("AutoClearChanBuffer = " + CString(b));
        } else if (sVar == "autoclearquerybuffer") {
            bool b = sValue.ToBool();
            pUser->SetAutoClearQueryBuffer(b);
            PutModule("AutoClearQueryBuffer = " + CString(b));
        } else if (sVar == "password") {
            const CString sSalt = CUtils::GetSalt();
            const CString sHash = CUser::SaltedHash(sValue, sSalt);
            pUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
            PutModule(t_s("Password has been changed!"));
        } else if (sVar == "maxjoins") {
            unsigned int i = sValue.ToUInt();
            pUser->SetMaxJoins(i);
            PutModule("MaxJoins = " + CString(pUser->MaxJoins()));
        } else if (sVar == "notraffictimeout") {
            unsigned int i = sValue.ToUInt();
            if (i < 30) {
                PutModule(t_s("Timeout can't be less than 30 seconds!"));
            } else {
                pUser->SetNoTrafficTimeout(i);
                PutModule("NoTrafficTimeout = " +
                          CString(pUser->GetNoTrafficTimeout()));
            }
        } else if (sVar == "maxnetworks") {
            if (GetUser()->IsAdmin()) {
                unsigned int i = sValue.ToUInt();
                pUser->SetMaxNetworks(i);
                PutModule("MaxNetworks = " + sValue);
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "maxquerybuffers") {
            unsigned int i = sValue.ToUInt();
            pUser->SetMaxQueryBuffers(i);
            PutModule("MaxQueryBuffers = " + sValue);
        } else if (sVar == "jointries") {
            unsigned int i = sValue.ToUInt();
            pUser->SetJoinTries(i);
            PutModule("JoinTries = " + CString(pUser->JoinTries()));
        } else if (sVar == "timezone") {
            pUser->SetTimezone(sValue);
            PutModule("Timezone = " + pUser->GetTimezone());
        } else if (sVar == "admin") {
            if (GetUser()->IsAdmin() && pUser != GetUser()) {
                bool b = sValue.ToBool();
                pUser->SetAdmin(b);
                PutModule("Admin = " + CString(pUser->IsAdmin()));
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "prependtimestamp") {
            bool b = sValue.ToBool();
            pUser->SetTimestampPrepend(b);
            PutModule("PrependTimestamp = " + CString(b));
        } else if (sVar == "appendtimestamp") {
            bool b = sValue.ToBool();
            pUser->SetTimestampAppend(b);
            PutModule("AppendTimestamp = " + CString(b));
        } else if (sVar == "authonlyviamodule") {
            if (GetUser()->IsAdmin()) {
                bool b = sValue.ToBool();
                pUser->SetAuthOnlyViaModule(b);
                PutModule("AuthOnlyViaModule = " + CString(b));
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "timestampformat") {
            pUser->SetTimestampFormat(sValue);
            PutModule("TimestampFormat = " + sValue);
        } else if (sVar == "dccbindhost") {
            if (!pUser->DenySetBindHost() || GetUser()->IsAdmin()) {
                pUser->SetDCCBindHost(sValue);
                PutModule("DCCBindHost = " + sValue);
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar == "statusprefix") {
            if (sVar.find_first_of(" \t\n") == CString::npos) {
                pUser->SetStatusPrefix(sValue);
                PutModule("StatusPrefix = " + sValue);
            } else {
                PutModule(t_s("That would be a bad idea!"));
            }
        }
#ifdef HAVE_I18N
        else if (sVar == "language") {
            auto mTranslations = CTranslationInfo::GetTranslations();
            // TODO: maybe stop special-casing English
            if (sValue == "en") {
                pUser->SetLanguage("");
                PutModule("Language is set to English");
            } else if (mTranslations.count(sValue)) {
                pUser->SetLanguage(sValue);
                PutModule("Language = " + sValue);
            } else {
                VCString vsCodes = {"en"};
                for (const auto it : mTranslations) {
                    vsCodes.push_back(it.first);
                }
                PutModule(t_f("Supported languages: {1}")(
                    CString(", ").Join(vsCodes.begin(), vsCodes.end())));
            }
        }
#endif
#ifdef HAVE_ICU
        else if (sVar == "clientencoding") {
            pUser->SetClientEncoding(sValue);
            PutModule("ClientEncoding = " + sValue);
        }
#endif
        else
            PutModule(t_s("Error: Unknown variable"));
    }

    void GetNetwork(const CString& sLine) {
        const CString sVar = sLine.Token(1).AsLower();
        const CString sUsername = sLine.Token(2);
        const CString sNetwork = sLine.Token(3);

        CIRCNetwork* pNetwork = nullptr;
        CUser* pUser;

        if (sVar.empty()) {
            PutModule(t_s("Usage: GetNetwork <variable> [username] [network]"));
            return;
        }

        if (sUsername.empty()) {
            pUser = GetUser();
        } else {
            pUser = FindUser(sUsername);
        }

        if (!pUser) {
            return;
        }

        if (sNetwork.empty()) {
            if (pUser == GetUser()) {
                pNetwork = CModule::GetNetwork();
            } else {
                PutModule(
                    t_s("Error: A network must be specified to get another "
                        "users settings."));
                return;
            }

            if (!pNetwork) {
                PutModule(t_s("You are not currently attached to a network."));
                return;
            }
        } else {
            pNetwork = FindNetwork(pUser, sNetwork);
            if (!pNetwork) {
                PutModule(t_s("Error: Invalid network."));
                return;
            }
        }

        if (sVar.Equals("nick")) {
            PutModule("Nick = " + pNetwork->GetNick());
        } else if (sVar.Equals("altnick")) {
            PutModule("AltNick = " + pNetwork->GetAltNick());
        } else if (sVar.Equals("ident")) {
            PutModule("Ident = " + pNetwork->GetIdent());
        } else if (sVar.Equals("realname")) {
            PutModule("RealName = " + pNetwork->GetRealName());
        } else if (sVar.Equals("bindhost")) {
            PutModule("BindHost = " + pNetwork->GetBindHost());
        } else if (sVar.Equals("floodrate")) {
            PutModule("FloodRate = " + CString(pNetwork->GetFloodRate()));
        } else if (sVar.Equals("floodburst")) {
            PutModule("FloodBurst = " + CString(pNetwork->GetFloodBurst()));
        } else if (sVar.Equals("joindelay")) {
            PutModule("JoinDelay = " + CString(pNetwork->GetJoinDelay()));
#ifdef HAVE_ICU
        } else if (sVar.Equals("encoding")) {
            PutModule("Encoding = " + pNetwork->GetEncoding());
#endif
        } else if (sVar.Equals("quitmsg")) {
            PutModule("QuitMsg = " + pNetwork->GetQuitMsg());
        } else if (sVar.Equals("trustallcerts")) {
            PutModule("TrustAllCerts = " + CString(pNetwork->GetTrustAllCerts()));
        } else if (sVar.Equals("trustpki")) {
            PutModule("TrustPKI = " + CString(pNetwork->GetTrustPKI()));
        } else {
            PutModule(t_s("Error: Unknown variable"));
        }
    }

    void SetNetwork(const CString& sLine) {
        const CString sVar = sLine.Token(1).AsLower();
        const CString sUsername = sLine.Token(2);
        const CString sNetwork = sLine.Token(3);
        const CString sValue = sLine.Token(4, true);

        if (sValue.empty()) {
            PutModule(t_s(
                "Usage: SetNetwork <variable> <username> <network> <value>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) {
            return;
        }

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        if (sVar.Equals("nick")) {
            pNetwork->SetNick(sValue);
            PutModule("Nick = " + pNetwork->GetNick());
        } else if (sVar.Equals("altnick")) {
            pNetwork->SetAltNick(sValue);
            PutModule("AltNick = " + pNetwork->GetAltNick());
        } else if (sVar.Equals("ident")) {
            pNetwork->SetIdent(sValue);
            PutModule("Ident = " + pNetwork->GetIdent());
        } else if (sVar.Equals("realname")) {
            pNetwork->SetRealName(sValue);
            PutModule("RealName = " + pNetwork->GetRealName());
        } else if (sVar.Equals("bindhost")) {
            if (!pUser->DenySetBindHost() || GetUser()->IsAdmin()) {
                if (sValue.Equals(pNetwork->GetBindHost())) {
                    PutModule(t_s("This bind host is already set!"));
                    return;
                }

                pNetwork->SetBindHost(sValue);
                PutModule("BindHost = " + sValue);
            } else {
                PutModule(t_s("Access denied!"));
            }
        } else if (sVar.Equals("floodrate")) {
            pNetwork->SetFloodRate(sValue.ToDouble());
            PutModule("FloodRate = " + CString(pNetwork->GetFloodRate()));
        } else if (sVar.Equals("floodburst")) {
            pNetwork->SetFloodBurst(sValue.ToUShort());
            PutModule("FloodBurst = " + CString(pNetwork->GetFloodBurst()));
        } else if (sVar.Equals("joindelay")) {
            pNetwork->SetJoinDelay(sValue.ToUShort());
            PutModule("JoinDelay = " + CString(pNetwork->GetJoinDelay()));
#ifdef HAVE_ICU
        } else if (sVar.Equals("encoding")) {
            pNetwork->SetEncoding(sValue);
            PutModule("Encoding = " + pNetwork->GetEncoding());
#endif
        } else if (sVar.Equals("quitmsg")) {
            pNetwork->SetQuitMsg(sValue);
            PutModule("QuitMsg = " + pNetwork->GetQuitMsg());
        } else if (sVar.Equals("trustallcerts")) {
            bool b = sValue.ToBool();
            pNetwork->SetTrustAllCerts(b);
            PutModule("TrustAllCerts = " + CString(b));
        } else if (sVar.Equals("trustpki")) {
            bool b = sValue.ToBool();
            pNetwork->SetTrustPKI(b);
            PutModule("TrustPKI = " + CString(b));
        } else {
            PutModule(t_s("Error: Unknown variable"));
        }
    }

    void AddChan(const CString& sLine) {
        const CString sUsername = sLine.Token(1);
        const CString sNetwork = sLine.Token(2);
        const CString sChan = sLine.Token(3);

        if (sChan.empty()) {
            PutModule(t_s("Usage: AddChan <username> <network> <channel>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        if (pNetwork->FindChan(sChan)) {
            PutModule(t_f("Error: User {1} already has a channel named {2}.")(
                sUsername, sChan));
            return;
        }

        CChan* pChan = new CChan(sChan, pNetwork, true);
        if (pNetwork->AddChan(pChan))
            PutModule(t_f("Channel {1} for user {2} added to network {3}.")(
                pChan->GetName(), sUsername, pNetwork->GetName()));
        else
            PutModule(t_f(
                "Could not add channel {1} for user {2} to network {3}, does "
                "it already exist?")(sChan, sUsername, pNetwork->GetName()));
    }

    void DelChan(const CString& sLine) {
        const CString sUsername = sLine.Token(1);
        const CString sNetwork = sLine.Token(2);
        const CString sChan = sLine.Token(3);

        if (sChan.empty()) {
            PutModule(t_s("Usage: DelChan <username> <network> <channel>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        std::vector<CChan*> vChans = pNetwork->FindChans(sChan);
        if (vChans.empty()) {
            PutModule(
                t_f("Error: User {1} does not have any channel matching [{2}] "
                    "in network {3}")(sUsername, sChan, pNetwork->GetName()));
            return;
        }

        VCString vsNames;
        for (const CChan* pChan : vChans) {
            const CString& sName = pChan->GetName();
            vsNames.push_back(sName);
            pNetwork->PutIRC("PART " + sName);
            pNetwork->DelChan(sName);
        }

        PutModule(t_p("Channel {1} is deleted from network {2} of user {3}",
                      "Channels {1} are deleted from network {2} of user {3}",
                      vsNames.size())(
            CString(", ").Join(vsNames.begin(), vsNames.end()),
            pNetwork->GetName(), sUsername));
    }

    void GetChan(const CString& sLine) {
        const CString sVar = sLine.Token(1).AsLower();
        CString sUsername = sLine.Token(2);
        CString sNetwork = sLine.Token(3);
        CString sChan = sLine.Token(4, true);

        if (sChan.empty()) {
            PutModule(
                t_s("Usage: GetChan <variable> <username> <network> <chan>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        std::vector<CChan*> vChans = pNetwork->FindChans(sChan);
        if (vChans.empty()) {
            PutModule(t_f("Error: No channels matching [{1}] found.")(sChan));
            return;
        }

        for (CChan* pChan : vChans) {
            if (sVar == "defmodes") {
                PutModule(pChan->GetName() + ": DefModes = " +
                          pChan->GetDefaultModes());
            } else if (sVar == "buffersize" || sVar == "buffer") {
                CString sValue(pChan->GetBufferCount());
                if (!pChan->HasBufferCountSet()) {
                    sValue += " (default)";
                }
                PutModule(pChan->GetName() + ": BufferSize = " + sValue);
            } else if (sVar == "inconfig") {
                PutModule(pChan->GetName() + ": InConfig = " +
                          CString(pChan->InConfig()));
            } else if (sVar == "keepbuffer") {
                // XXX compatibility crap, added in 0.207
                PutModule(pChan->GetName() + ": KeepBuffer = " +
                          CString(!pChan->AutoClearChanBuffer()));
            } else if (sVar == "autoclearchanbuffer") {
                CString sValue(pChan->AutoClearChanBuffer());
                if (!pChan->HasAutoClearChanBufferSet()) {
                    sValue += " (default)";
                }
                PutModule(pChan->GetName() + ": AutoClearChanBuffer = " +
                          sValue);
            } else if (sVar == "detached") {
                PutModule(pChan->GetName() + ": Detached = " +
                          CString(pChan->IsDetached()));
            } else if (sVar == "key") {
                PutModule(pChan->GetName() + ": Key = " + pChan->GetKey());
            } else {
                PutModule(t_s("Error: Unknown variable"));
                return;
            }
        }
    }

    void SetChan(const CString& sLine) {
        const CString sVar = sLine.Token(1).AsLower();
        CString sUsername = sLine.Token(2);
        CString sNetwork = sLine.Token(3);
        CString sChan = sLine.Token(4);
        CString sValue = sLine.Token(5, true);

        if (sValue.empty()) {
            PutModule(
                t_s("Usage: SetChan <variable> <username> <network> <chan> "
                    "<value>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        std::vector<CChan*> vChans = pNetwork->FindChans(sChan);
        if (vChans.empty()) {
            PutModule(t_f("Error: No channels matching [{1}] found.")(sChan));
            return;
        }

        for (CChan* pChan : vChans) {
            if (sVar == "defmodes") {
                pChan->SetDefaultModes(sValue);
                PutModule(pChan->GetName() + ": DefModes = " + sValue);
            } else if (sVar == "buffersize" || sVar == "buffer") {
                unsigned int i = sValue.ToUInt();
                if (sValue.Equals("-")) {
                    pChan->ResetBufferCount();
                    PutModule(pChan->GetName() + ": BufferSize = " +
                              CString(pChan->GetBufferCount()));
                } else if (pChan->SetBufferCount(i, GetUser()->IsAdmin())) {
                    // Admins don't have to honour the buffer limit
                    PutModule(pChan->GetName() + ": BufferSize = " + sValue);
                } else {
                    PutModule(
                        t_f("Setting failed, limit for buffer size is {1}")(
                            CString(CZNC::Get().GetMaxBufferSize())));
                    return;
                }
            } else if (sVar == "inconfig") {
                bool b = sValue.ToBool();
                pChan->SetInConfig(b);
                PutModule(pChan->GetName() + ": InConfig = " + CString(b));
            } else if (sVar == "keepbuffer") {
                // XXX compatibility crap, added in 0.207
                bool b = !sValue.ToBool();
                pChan->SetAutoClearChanBuffer(b);
                PutModule(pChan->GetName() + ": AutoClearChanBuffer = " +
                          CString(b));
            } else if (sVar == "autoclearchanbuffer") {
                if (sValue.Equals("-")) {
                    pChan->ResetAutoClearChanBuffer();
                } else {
                    bool b = sValue.ToBool();
                    pChan->SetAutoClearChanBuffer(b);
                }
                PutModule(pChan->GetName() + ": AutoClearChanBuffer = " +
                          CString(pChan->AutoClearChanBuffer()));
            } else if (sVar == "detached") {
                bool b = sValue.ToBool();
                if (pChan->IsDetached() != b) {
                    if (b)
                        pChan->DetachUser();
                    else
                        pChan->AttachUser();
                }
                PutModule(pChan->GetName() + ": Detached = " + CString(b));
            } else if (sVar == "key") {
                pChan->SetKey(sValue);
                PutModule(pChan->GetName() + ": Key = " + sValue);
            } else {
                PutModule(t_s("Error: Unknown variable"));
                return;
            }
        }
    }

    void ListUsers(const CString&) {
        if (!GetUser()->IsAdmin()) return;

        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
        CTable Table;
        Table.AddColumn(t_s("Username", "listusers"));
        Table.AddColumn(t_s("Realname", "listusers"));
        Table.AddColumn(t_s("IsAdmin", "listusers"));
        Table.AddColumn(t_s("Nick", "listusers"));
        Table.AddColumn(t_s("AltNick", "listusers"));
        Table.AddColumn(t_s("Ident", "listusers"));
        Table.AddColumn(t_s("BindHost", "listusers"));

        for (const auto& it : msUsers) {
            Table.AddRow();
            Table.SetCell(t_s("Username", "listusers"), it.first);
            Table.SetCell(t_s("Realname", "listusers"),
                          it.second->GetRealName());
            if (!it.second->IsAdmin())
                Table.SetCell(t_s("IsAdmin", "listusers"), t_s("No"));
            else
                Table.SetCell(t_s("IsAdmin", "listusers"), t_s("Yes"));
            Table.SetCell(t_s("Nick", "listusers"), it.second->GetNick());
            Table.SetCell(t_s("AltNick", "listusers"), it.second->GetAltNick());
            Table.SetCell(t_s("Ident", "listusers"), it.second->GetIdent());
            Table.SetCell(t_s("BindHost", "listusers"),
                          it.second->GetBindHost());
        }

        PutModule(Table);
    }

    void AddUser(const CString& sLine) {
        if (!GetUser()->IsAdmin()) {
            PutModule(
                t_s("Error: You need to have admin rights to add new users!"));
            return;
        }

        const CString sUsername = sLine.Token(1), sPassword = sLine.Token(2);
        if (sPassword.empty()) {
            PutModule(t_s("Usage: AddUser <username> <password>"));
            return;
        }

        if (CZNC::Get().FindUser(sUsername)) {
            PutModule(t_f("Error: User {1} already exists!")(sUsername));
            return;
        }

        CUser* pNewUser = new CUser(sUsername);
        CString sSalt = CUtils::GetSalt();
        pNewUser->SetPass(CUser::SaltedHash(sPassword, sSalt),
                          CUser::HASH_DEFAULT, sSalt);

        CString sErr;
        if (!CZNC::Get().AddUser(pNewUser, sErr)) {
            delete pNewUser;
            PutModule(t_f("Error: User not added: {1}")(sErr));
            return;
        }

        PutModule(t_f("User {1} added!")(sUsername));
        return;
    }

    void DelUser(const CString& sLine) {
        if (!GetUser()->IsAdmin()) {
            PutModule(
                t_s("Error: You need to have admin rights to delete users!"));
            return;
        }

        const CString sUsername = sLine.Token(1, true);
        if (sUsername.empty()) {
            PutModule(t_s("Usage: DelUser <username>"));
            return;
        }

        CUser* pUser = CZNC::Get().FindUser(sUsername);

        if (!pUser) {
            PutModule(t_f("Error: User [{1}] does not exist!")(sUsername));
            return;
        }

        if (pUser == GetUser()) {
            PutModule(t_s("Error: You can't delete yourself!"));
            return;
        }

        if (!CZNC::Get().DeleteUser(pUser->GetUserName())) {
            // This can't happen, because we got the user from FindUser()
            PutModule(t_s("Error: Internal error!"));
            return;
        }

        PutModule(t_f("User {1} deleted!")(sUsername));
        return;
    }

    void CloneUser(const CString& sLine) {
        if (!GetUser()->IsAdmin()) {
            PutModule(
                t_s("Error: You need to have admin rights to add new users!"));
            return;
        }

        const CString sOldUsername = sLine.Token(1),
                      sNewUsername = sLine.Token(2, true);

        if (sOldUsername.empty() || sNewUsername.empty()) {
            PutModule(t_s("Usage: CloneUser <old username> <new username>"));
            return;
        }

        CUser* pOldUser = CZNC::Get().FindUser(sOldUsername);

        if (!pOldUser) {
            PutModule(t_f("Error: User [{1}] does not exist!")(sOldUsername));
            return;
        }

        CUser* pNewUser = new CUser(sNewUsername);
        CString sError;
        if (!pNewUser->Clone(*pOldUser, sError)) {
            delete pNewUser;
            PutModule(t_f("Error: Cloning failed: {1}")(sError));
            return;
        }

        if (!CZNC::Get().AddUser(pNewUser, sError)) {
            delete pNewUser;
            PutModule(t_f("Error: User not added: {1}")(sError));
            return;
        }

        PutModule(t_f("User {1} added!")(sNewUsername));
        return;
    }

    void AddNetwork(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sNetwork = sLine.Token(2);
        CUser* pUser = GetUser();

        if (sNetwork.empty()) {
            sNetwork = sUser;
        } else {
            pUser = FindUser(sUser);
            if (!pUser) {
                return;
            }
        }

        if (sNetwork.empty()) {
            PutModule(t_s("Usage: AddNetwork [user] network"));
            return;
        }

        if (!GetUser()->IsAdmin() && !pUser->HasSpaceForNewNetwork()) {
            PutStatus(
                t_s("Network number limit reached. Ask an admin to increase "
                    "the limit for you, or delete unneeded networks using /znc "
                    "DelNetwork <name>"));
            return;
        }

        if (pUser->FindNetwork(sNetwork)) {
            PutModule(
                t_f("Error: User {1} already has a network with the name {2}")(
                    pUser->GetUserName(), sNetwork));
            return;
        }

        CString sNetworkAddError;
        if (pUser->AddNetwork(sNetwork, sNetworkAddError)) {
            PutModule(t_f("Network {1} added to user {2}.")(
                sNetwork, pUser->GetUserName()));
        } else {
            PutModule(t_f(
                "Error: Network [{1}] could not be added for user {2}: {3}")(
                sNetwork, pUser->GetUserName(), sNetworkAddError));
        }
    }

    void DelNetwork(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sNetwork = sLine.Token(2);
        CUser* pUser = GetUser();

        if (sNetwork.empty()) {
            sNetwork = sUser;
        } else {
            pUser = FindUser(sUser);
            if (!pUser) {
                return;
            }
        }

        if (sNetwork.empty()) {
            PutModule(t_s("Usage: DelNetwork [user] network"));
            return;
        }

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        if (pNetwork == CModule::GetNetwork()) {
            PutModule(t_f(
                "The currently active network can be deleted via {1}status")(
                GetUser()->GetStatusPrefix()));
            return;
        }

        if (pUser->DeleteNetwork(sNetwork)) {
            PutModule(t_f("Network {1} deleted for user {2}.")(
                sNetwork, pUser->GetUserName()));
        } else {
            PutModule(
                t_f("Error: Network {1} could not be deleted for user {2}.")(
                    sNetwork, pUser->GetUserName()));
        }
    }

    void ListNetworks(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CUser* pUser = GetUser();

        if (!sUser.empty()) {
            pUser = FindUser(sUser);
            if (!pUser) {
                return;
            }
        }

        const vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();

        CTable Table;
        Table.AddColumn(t_s("Network", "listnetworks"));
        Table.AddColumn(t_s("OnIRC", "listnetworks"));
        Table.AddColumn(t_s("IRC Server", "listnetworks"));
        Table.AddColumn(t_s("IRC User", "listnetworks"));
        Table.AddColumn(t_s("Channels", "listnetworks"));

        for (const CIRCNetwork* pNetwork : vNetworks) {
            Table.AddRow();
            Table.SetCell(t_s("Network", "listnetworks"), pNetwork->GetName());
            if (pNetwork->IsIRCConnected()) {
                Table.SetCell(t_s("OnIRC", "listnetworks"), t_s("Yes"));
                Table.SetCell(t_s("IRC Server", "listnetworks"),
                              pNetwork->GetIRCServer());
                Table.SetCell(t_s("IRC User", "listnetworks"),
                              pNetwork->GetIRCNick().GetNickMask());
                Table.SetCell(t_s("Channels", "listnetworks"),
                              CString(pNetwork->GetChans().size()));
            } else {
                Table.SetCell(t_s("OnIRC", "listnetworks"), t_s("No"));
            }
        }

        if (PutModule(Table) == 0) {
            PutModule(t_s("No networks"));
        }
    }

    void AddServer(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sNetwork = sLine.Token(2);
        CString sServer = sLine.Token(3, true);

        if (sServer.empty()) {
            PutModule(
                t_s("Usage: AddServer <username> <network> <server> [[+]port] "
                    "[password]"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        if (pNetwork->AddServer(sServer))
            PutModule(t_f("Added IRC Server {1} to network {2} for user {3}.")(
                sServer, pNetwork->GetName(), pUser->GetUserName()));
        else
            PutModule(t_f(
                "Error: Could not add IRC server {1} to network {2} for user "
                "{3}.")(sServer, pNetwork->GetName(), pUser->GetUserName()));
    }

    void DelServer(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sNetwork = sLine.Token(2);
        CString sServer = sLine.Token(3, true);
        unsigned short uPort = sLine.Token(4).ToUShort();
        CString sPass = sLine.Token(5);

        if (sServer.empty()) {
            PutModule(
                t_s("Usage: DelServer <username> <network> <server> [[+]port] "
                    "[password]"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        if (pNetwork->DelServer(sServer, uPort, sPass))
            PutModule(
                t_f("Deleted IRC Server {1} from network {2} for user {3}.")(
                    sServer, pNetwork->GetName(), pUser->GetUserName()));
        else
            PutModule(
                t_f("Error: Could not delete IRC server {1} from network {2} "
                    "for user {3}.")(sServer, pNetwork->GetName(),
                                     pUser->GetUserName()));
    }

    void ReconnectUser(const CString& sLine) {
        CString sUserName = sLine.Token(1);
        CString sNetwork = sLine.Token(2);

        if (sNetwork.empty()) {
            PutModule(t_s("Usage: Reconnect <username> <network>"));
            return;
        }

        CUser* pUser = FindUser(sUserName);
        if (!pUser) {
            return;
        }

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        CIRCSock* pIRCSock = pNetwork->GetIRCSock();
        // cancel connection attempt:
        if (pIRCSock && !pIRCSock->IsConnected()) {
            pIRCSock->Close();
        }
        // or close existing connection:
        else if (pIRCSock) {
            pIRCSock->Quit();
        }

        // then reconnect
        pNetwork->SetIRCConnectEnabled(true);

        PutModule(t_f("Queued network {1} of user {2} for a reconnect.")(
            pNetwork->GetName(), pUser->GetUserName()));
    }

    void DisconnectUser(const CString& sLine) {
        CString sUserName = sLine.Token(1);
        CString sNetwork = sLine.Token(2);

        if (sNetwork.empty()) {
            PutModule(t_s("Usage: Disconnect <username> <network>"));
            return;
        }

        CUser* pUser = FindUser(sUserName);
        if (!pUser) {
            return;
        }

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        pNetwork->SetIRCConnectEnabled(false);
        PutModule(t_f("Closed IRC connection for network {1} of user {2}.")(
            pNetwork->GetName(), pUser->GetUserName()));
    }

    void ListCTCP(const CString& sLine) {
        CString sUserName = sLine.Token(1, true);

        if (sUserName.empty()) {
            sUserName = GetUser()->GetUserName();
        }
        CUser* pUser = FindUser(sUserName);
        if (!pUser) return;

        const MCString& msCTCPReplies = pUser->GetCTCPReplies();
        CTable Table;
        Table.AddColumn(t_s("Request", "listctcp"));
        Table.AddColumn(t_s("Reply", "listctcp"));
        for (const auto& it : msCTCPReplies) {
            Table.AddRow();
            Table.SetCell(t_s("Request", "listctcp"), it.first);
            Table.SetCell(t_s("Reply", "listctcp"), it.second);
        }

        if (Table.empty()) {
            PutModule(t_f("No CTCP replies for user {1} are configured")(
                pUser->GetUserName()));
        } else {
            PutModule(t_f("CTCP replies for user {1}:")(pUser->GetUserName()));
            PutModule(Table);
        }
    }

    void AddCTCP(const CString& sLine) {
        CString sUserName = sLine.Token(1);
        CString sCTCPRequest = sLine.Token(2);
        CString sCTCPReply = sLine.Token(3, true);

        if (sCTCPRequest.empty()) {
            sCTCPRequest = sUserName;
            sCTCPReply = sLine.Token(2, true);
            sUserName = GetUser()->GetUserName();
        }
        if (sCTCPRequest.empty()) {
            PutModule(t_s("Usage: AddCTCP [user] [request] [reply]"));
            PutModule(
                t_s("This will cause ZNC to reply to the CTCP instead of "
                    "forwarding it to clients."));
            PutModule(t_s(
                "An empty reply will cause the CTCP request to be blocked."));
            return;
        }

        CUser* pUser = FindUser(sUserName);
        if (!pUser) return;

        pUser->AddCTCPReply(sCTCPRequest, sCTCPReply);
        if (sCTCPReply.empty()) {
            PutModule(t_f("CTCP requests {1} to user {2} will now be blocked.")(
                sCTCPRequest.AsUpper(), pUser->GetUserName()));
        } else {
            PutModule(
                t_f("CTCP requests {1} to user {2} will now get reply: {3}")(
                    sCTCPRequest.AsUpper(), pUser->GetUserName(), sCTCPReply));
        }
    }

    void DelCTCP(const CString& sLine) {
        CString sUserName = sLine.Token(1);
        CString sCTCPRequest = sLine.Token(2, true);

        if (sCTCPRequest.empty()) {
            sCTCPRequest = sUserName;
            sUserName = GetUser()->GetUserName();
        }
        CUser* pUser = FindUser(sUserName);
        if (!pUser) return;

        if (sCTCPRequest.empty()) {
            PutModule(t_s("Usage: DelCTCP [user] [request]"));
            return;
        }

        if (pUser->DelCTCPReply(sCTCPRequest)) {
            PutModule(t_f(
                "CTCP requests {1} to user {2} will now be sent to IRC clients")(
                sCTCPRequest.AsUpper(), pUser->GetUserName()));
        } else {
            PutModule(
                t_f("CTCP requests {1} to user {2} will be sent to IRC clients "
                    "(nothing has changed)")(sCTCPRequest.AsUpper(),
                                             pUser->GetUserName()));
        }
    }

    void LoadModuleFor(CModules& Modules, const CString& sModName,
                       const CString& sArgs, CModInfo::EModuleType eType,
                       CUser* pUser, CIRCNetwork* pNetwork) {
        if (pUser->DenyLoadMod() && !GetUser()->IsAdmin()) {
            PutModule(t_s("Loading modules has been disabled."));
            return;
        }

        CString sModRet;
        CModule* pMod = Modules.FindModule(sModName);
        if (!pMod) {
            if (!Modules.LoadModule(sModName, sArgs, eType, pUser, pNetwork,
                                    sModRet)) {
                PutModule(t_f("Error: Unable to load module {1}: {2}")(
                    sModName, sModRet));
            } else {
                PutModule(t_f("Loaded module {1}")(sModName));
            }
        } else if (pMod->GetArgs() != sArgs) {
            if (!Modules.ReloadModule(sModName, sArgs, pUser, pNetwork,
                                      sModRet)) {
                PutModule(t_f("Error: Unable to reload module {1}: {2}")(
                    sModName, sModRet));
            } else {
                PutModule(t_f("Reloaded module {1}")(sModName));
            }
        } else {
            PutModule(
                t_f("Error: Unable to load module {1} because it is already "
                    "loaded")(sModName));
        }
    }

    void LoadModuleForUser(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sModName = sLine.Token(2);
        CString sArgs = sLine.Token(3, true);

        if (sModName.empty()) {
            PutModule(t_s("Usage: LoadModule <username> <modulename> [args]"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        LoadModuleFor(pUser->GetModules(), sModName, sArgs,
                      CModInfo::UserModule, pUser, nullptr);
    }

    void LoadModuleForNetwork(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sNetwork = sLine.Token(2);
        CString sModName = sLine.Token(3);
        CString sArgs = sLine.Token(4, true);

        if (sModName.empty()) {
            PutModule(
                t_s("Usage: LoadNetModule <username> <network> <modulename> "
                    "[args]"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        LoadModuleFor(pNetwork->GetModules(), sModName, sArgs,
                      CModInfo::NetworkModule, pUser, pNetwork);
    }

    void UnLoadModuleFor(CModules& Modules, const CString& sModName,
                         CUser* pUser) {
        if (pUser->DenyLoadMod() && !GetUser()->IsAdmin()) {
            PutModule(t_s("Loading modules has been disabled."));
            return;
        }

        if (Modules.FindModule(sModName) == this) {
            PutModule(t_f("Please use /znc unloadmod {1}")(sModName));
            return;
        }

        CString sModRet;
        if (!Modules.UnloadModule(sModName, sModRet)) {
            PutModule(t_f("Error: Unable to unload module {1}: {2}")(sModName,
                                                                     sModRet));
        } else {
            PutModule(t_f("Unloaded module {1}")(sModName));
        }
    }

    void UnLoadModuleForUser(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sModName = sLine.Token(2);

        if (sModName.empty()) {
            PutModule(t_s("Usage: UnloadModule <username> <modulename>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        UnLoadModuleFor(pUser->GetModules(), sModName, pUser);
    }

    void UnLoadModuleForNetwork(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sNetwork = sLine.Token(2);
        CString sModName = sLine.Token(3);

        if (sModName.empty()) {
            PutModule(t_s(
                "Usage: UnloadNetModule <username> <network> <modulename>"));
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) {
            return;
        }

        UnLoadModuleFor(pNetwork->GetModules(), sModName, pUser);
    }

    void ListModulesFor(CModules& Modules) {
        CTable Table;
        Table.AddColumn(t_s("Name", "listmodules"));
        Table.AddColumn(t_s("Arguments", "listmodules"));

        for (const CModule* pMod : Modules) {
            Table.AddRow();
            Table.SetCell(t_s("Name", "listmodules"), pMod->GetModName());
            Table.SetCell(t_s("Arguments", "listmodules"), pMod->GetArgs());
        }

        PutModule(Table);
    }

    void ListModulesForUser(const CString& sLine) {
        CString sUsername = sLine.Token(1);

        if (sUsername.empty()) {
            PutModule("Usage: ListMods <username>");
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        if (pUser->GetModules().empty()) {
            PutModule(
                t_f("User {1} has no modules loaded.")(pUser->GetUserName()));
            return;
        }

        PutModule(t_f("Modules loaded for user {1}:")(pUser->GetUserName()));
        ListModulesFor(pUser->GetModules());
    }

    void ListModulesForNetwork(const CString& sLine) {
        CString sUsername = sLine.Token(1);
        CString sNetwork = sLine.Token(2);

        if (sNetwork.empty()) {
            PutModule("Usage: ListNetMods <username> <network>");
            return;
        }

        CUser* pUser = FindUser(sUsername);
        if (!pUser) return;

        CIRCNetwork* pNetwork = FindNetwork(pUser, sNetwork);
        if (!pNetwork) return;

        if (pNetwork->GetModules().empty()) {
            PutModule(t_f("Network {1} of user {2} has no modules loaded.")(
                pNetwork->GetName(), pUser->GetUserName()));
            return;
        }

        PutModule(t_f("Modules loaded for network {1} of user {2}:")(
            pNetwork->GetName(), pUser->GetUserName()));
        ListModulesFor(pNetwork->GetModules());
    }

  public:
    MODCONSTRUCTOR(CAdminMod) {
        AddCommand("Help", t_d("[command] [variable]"),
                   t_d("Prints help for matching commands and variables"),
                   [=](const CString& sLine) { PrintHelp(sLine); });
        AddCommand(
            "Get", t_d("<variable> [username]"),
            t_d("Prints the variable's value for the given or current user"),
            [=](const CString& sLine) { Get(sLine); });
        AddCommand("Set", t_d("<variable> <username> <value>"),
                   t_d("Sets the variable's value for the given user"),
                   [=](const CString& sLine) { Set(sLine); });
        AddCommand("GetNetwork", t_d("<variable> [username] [network]"),
                   t_d("Prints the variable's value for the given network"),
                   [=](const CString& sLine) { GetNetwork(sLine); });
        AddCommand("SetNetwork", t_d("<variable> <username> <network> <value>"),
                   t_d("Sets the variable's value for the given network"),
                   [=](const CString& sLine) { SetNetwork(sLine); });
        AddCommand("GetChan", t_d("<variable> [username] <network> <chan>"),
                   t_d("Prints the variable's value for the given channel"),
                   [=](const CString& sLine) { GetChan(sLine); });
        AddCommand("SetChan",
                   t_d("<variable> <username> <network> <chan> <value>"),
                   t_d("Sets the variable's value for the given channel"),
                   [=](const CString& sLine) { SetChan(sLine); });
        AddCommand("AddChan", t_d("<username> <network> <chan>"),
                   t_d("Adds a new channel"),
                   [=](const CString& sLine) { AddChan(sLine); });
        AddCommand("DelChan", t_d("<username> <network> <chan>"),
                   t_d("Deletes a channel"),
                   [=](const CString& sLine) { DelChan(sLine); });
        AddCommand("ListUsers", "", t_d("Lists users"),
                   [=](const CString& sLine) { ListUsers(sLine); });
        AddCommand("AddUser", t_d("<username> <password>"),
                   t_d("Adds a new user"),
                   [=](const CString& sLine) { AddUser(sLine); });
        AddCommand("DelUser", t_d("<username>"), t_d("Deletes a user"),
                   [=](const CString& sLine) { DelUser(sLine); });
        AddCommand("CloneUser", t_d("<old username> <new username>"),
                   t_d("Clones a user"),
                   [=](const CString& sLine) { CloneUser(sLine); });
        AddCommand("AddServer", t_d("<username> <network> <server>"),
                   t_d("Adds a new IRC server for the given or current user"),
                   [=](const CString& sLine) { AddServer(sLine); });
        AddCommand("DelServer", t_d("<username> <network> <server>"),
                   t_d("Deletes an IRC server from the given or current user"),
                   [=](const CString& sLine) { DelServer(sLine); });
        AddCommand("Reconnect", t_d("<username> <network>"),
                   t_d("Cycles the user's IRC server connection"),
                   [=](const CString& sLine) { ReconnectUser(sLine); });
        AddCommand("Disconnect", t_d("<username> <network>"),
                   t_d("Disconnects the user from their IRC server"),
                   [=](const CString& sLine) { DisconnectUser(sLine); });
        AddCommand("LoadModule", t_d("<username> <modulename> [args]"),
                   t_d("Loads a Module for a user"),
                   [=](const CString& sLine) { LoadModuleForUser(sLine); });
        AddCommand("UnLoadModule", t_d("<username> <modulename>"),
                   t_d("Removes a Module of a user"),
                   [=](const CString& sLine) { UnLoadModuleForUser(sLine); });
        AddCommand("ListMods", t_d("<username>"),
                   t_d("Get the list of modules for a user"),
                   [=](const CString& sLine) { ListModulesForUser(sLine); });
        AddCommand("LoadNetModule",
                   t_d("<username> <network> <modulename> [args]"),
                   t_d("Loads a Module for a network"),
                   [=](const CString& sLine) { LoadModuleForNetwork(sLine); });
        AddCommand(
            "UnLoadNetModule", t_d("<username> <network> <modulename>"),
            t_d("Removes a Module of a network"),
            [=](const CString& sLine) { UnLoadModuleForNetwork(sLine); });
        AddCommand("ListNetMods", t_d("<username> <network>"),
                   t_d("Get the list of modules for a network"),
                   [=](const CString& sLine) { ListModulesForNetwork(sLine); });
        AddCommand("ListCTCPs", t_d("<username>"),
                   t_d("List the configured CTCP replies"),
                   [=](const CString& sLine) { ListCTCP(sLine); });
        AddCommand("AddCTCP", t_d("<username> <ctcp> [reply]"),
                   t_d("Configure a new CTCP reply"),
                   [=](const CString& sLine) { AddCTCP(sLine); });
        AddCommand("DelCTCP", t_d("<username> <ctcp>"),
                   t_d("Remove a CTCP reply"),
                   [=](const CString& sLine) { DelCTCP(sLine); });

        // Network commands
        AddCommand("AddNetwork", t_d("[username] <network>"),
                   t_d("Add a network for a user"),
                   [=](const CString& sLine) { AddNetwork(sLine); });
        AddCommand("DelNetwork", t_d("[username] <network>"),
                   t_d("Delete a network for a user"),
                   [=](const CString& sLine) { DelNetwork(sLine); });
        AddCommand("ListNetworks", t_d("[username]"),
                   t_d("List all networks for a user"),
                   [=](const CString& sLine) { ListNetworks(sLine); });
    }

    ~CAdminMod() override {}
};

template <>
void TModInfo<CAdminMod>(CModInfo& Info) {
    Info.SetWikiPage("controlpanel");
}

USERMODULEDEFS(CAdminMod,
               t_s("Dynamic configuration through IRC. Allows editing only "
               "yourself if you're not ZNC admin."))
