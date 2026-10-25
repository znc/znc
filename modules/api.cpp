/*
 * Copyright (C) 2004-2021 ZNC, see the NOTICE file for details.
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
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Query.h>
#include <znc/Server.h>
#include <znc/User.h>
#include <znc/znc.h>

/*

Calling conventions: caller consumes the token they used. Callee is responsible
for ensuring they have enough tokens to proceed.

/*

TODO:

* Allow querying detailed information about network channels, probably by introducing subscopes (e.g. `QUERY libera #znc MODE`)
* Ditto module information (`QUERY USER modname DESCRIPTION` for user mods)
* MODIFY: this should have ADD, DELETE, SET commands for lists/scalars

*/

using std::vector;

CString b2s(const bool b) {
    if (b) return CString("TRUE");
    return CString("FALSE");
}

#define USERSEND(fn) _WrapValue(CString(GetUser()->fn));
#define USERSEND_BOOL(fn) _WrapValue(b2s(GetUser()->fn));
#define NETWORKSEND(fn) _WrapValue(CString(network->fn));
#define NETWORKSEND_BOOL(fn) _WrapValue(b2s(network->fn));
#define NETWORKSEND_LIST_SIMPLE(fn)      \
    PutModule("LIST");                   \
    for (const auto val : network->fn) { \
        PutModule(val);                  \
    }                                    \
    PutModule("LISTEND");
#define NETWORKSEND_LIST_OBJECT(fn, resolveFn) \
    PutModule("LIST");                         \
    for (const auto val : network->fn) {       \
        PutModule(resolveFn);                  \
    }                                          \
    PutModule("LISTEND");
#define ACCESS_CHECK(checkFn, reason) \
    if (checkFn) {                    \
        PutModule("EACCES " reason);  \
        return;                       \
    }

class CApi : public CModule {
  public:
    MODCONSTRUCTOR(CApi) {}

    ~CApi() override {}

    std::map<const CString, std::function<void(CApi*, VCString)>> queryMap = {
        {"ZNC", &CApi::HandleZNCQuery}, {"USER", &CApi::HandleUserQuery}};

    std::map<const CString, std::function<void(CApi*, VCString)>> commandMap = {
        {"PING", &CApi::CommandPing},
        {"QUERY", &CApi::CommandQuery},
        {"HELP", &CApi::CommandHelp},
        {"COMMANDS", &CApi::CommandCommands},
        {"QUERYSCOPES", &CApi::CommandQueryscopes}};

    void _WrapValue(CString str) {
        if (str.Equals("")) {
            PutModule("NULL");
        } else {
            PutModule("VALUE");
            PutModule(str);
        }
    }

    void HandleZNCQuery(VCString vsTokens) {
        if (vsTokens[0].Equals("VERSION")) {
            ACCESS_CHECK(CZNC::Get().GetHideVersion(), "HIDEVERSION is TRUE");

            PutModule("VALUE");
            PutModule(CZNC::GetVersion());
        } else if (vsTokens[0].Equals("UPTIME")) {
            PutModule("VALUE");
            PutModule(CZNC::Get().GetUptime());
        } else {
            PutModule("EINVAL Unknown query in scope ZNC");
        }
    }

    void HandleUserQuery(VCString vsTokens) {
        // IRC behavior defaults
        if (vsTokens[0].Equals("NICK")) {
            USERSEND(GetNick());
        } else if (vsTokens[0].Equals("ALTNICK")) {
            USERSEND(GetAltNick());
        } else if (vsTokens[0].Equals("IDENT")) {
            USERSEND(GetIdent());
        } else if (vsTokens[0].Equals("REALNAME")) {
            USERSEND(GetRealName());
        } else if (vsTokens[0].Equals("QUITMESSAGE")) {
            USERSEND(GetQuitMsg());
        } else if (vsTokens[0].Equals("DEFAULTCHANMODES")) {
            USERSEND(GetDefaultChanModes());
        } else if (vsTokens[0].Equals("MAXQUERYBUFFERS")) {
            USERSEND(MaxQueryBuffers());
        } else if (vsTokens[0].Equals("CLIENTENCODING")) {
            USERSEND(GetClientEncoding());
        } else if (vsTokens[0].Equals("JOINTRIES")) {
            USERSEND(JoinTries());
        } else if (vsTokens[0].Equals("JOINSPEED")) {
            USERSEND(MaxJoins());
        } else if (vsTokens[0].Equals("TIMEOUTBEFORERECONNECT")) {
            USERSEND(GetNoTrafficTimeout());
        } else if (vsTokens[0].Equals("CTCPREPLIES")) {
            PutModule("TUPLELIST 2");
            for (std::pair<CString, CString> key_val :
                 GetUser()->GetCTCPReplies()) {
                PutModule(key_val.first);
                PutModule(key_val.second);
            }
            PutModule("TUPLELISTEND");
            // Permissions
        } else if (vsTokens[0].Equals("DENYSETBINDHOST")) {
            USERSEND_BOOL(DenySetBindHost());
            // Admin limits
        } else if (vsTokens[0].Equals("MAXNETWORKS")) {
            USERSEND(MaxNetworks());
            // ZNC config
        } else if (vsTokens[0].Equals("STATUSPREFIX")) {
            USERSEND(GetStatusPrefix());
        } else if (vsTokens[0].Equals("AUTHONLYVIAMODULE")) {
            USERSEND_BOOL(AuthOnlyViaModule());
        } else if (vsTokens[0].Equals("AUTOCLEARCHANBUFFER")) {
            USERSEND_BOOL(AutoClearChanBuffer());
        } else if (vsTokens[0].Equals("AUTOCLEARQUERYBUFFER")) {
            USERSEND_BOOL(AutoClearQueryBuffer());
        } else if (vsTokens[0].Equals("CHANBUFFERSIZE")) {
            USERSEND(GetChanBufferSize());
        } else if (vsTokens[0].Equals("QUERYBUFFERSIZE")) {
            USERSEND(GetQueryBufferSize());
        } else if (vsTokens[0].Equals("MULTICLIENTS")) {
            USERSEND_BOOL(MultiClients());
        } else if (vsTokens[0].Equals("APPENDTIMESTAMPS")) {
            USERSEND_BOOL(GetTimestampAppend());
        } else if (vsTokens[0].Equals("PREPENDTIMESTAMPS")) {
            USERSEND_BOOL(GetTimestampPrepend());
        } else if (vsTokens[0].Equals("TIMESTAMPFORMAT")) {
            USERSEND(GetTimestampFormat());
        } else if (vsTokens[0].Equals("TIMEZONE")) {
            USERSEND(GetTimezone());
        } else if (vsTokens[0].Equals("SKIN")) {
            USERSEND(GetTimezone());
        } else if (vsTokens[0].Equals("LANGUAGE")) {
            USERSEND(GetTimezone());
        } else if (vsTokens[0].Equals("HAVEI18N")) {
            USERSEND(GetTimezone());
        } else if (vsTokens[0].Equals("ALLOWEDIPS")) {
            PutModule("LIST");
            for (const CString ip : GetUser()->GetAllowedHosts()) {
                PutModule(ip);
            }
            PutModule("LISTEND");
            // ZNC access-controlled config
        } else if (vsTokens[0].Equals("NETWORKS")) {
            PutModule("LIST");
            for (const CIRCNetwork* network : GetUser()->GetNetworks()) {
                PutModule(network->GetName());
            }
            PutModule("LISTEND");
        } else if (vsTokens[0].Equals("BINDHOST")) {
            ACCESS_CHECK(GetUser()->DenySetBindHost(),
                         "DENYSETBINDHOST is TRUE");

            USERSEND(GetBindHost());
        } else if (vsTokens[0].Equals("DCCBINDHOST")) {
            ACCESS_CHECK(GetUser()->DenySetBindHost(),
                         "DENYSETBINDHOST is TRUE");

            USERSEND(GetDCCBindHost());
            // Statistics
        } else if (vsTokens[0].Equals("BYTESREAD")) {
            USERSEND(BytesRead());
        } else if (vsTokens[0].Equals("BYTESWRITTEN")) {
            USERSEND(BytesWritten());
        } else {
            PutModule("EINVAL Unknown query in scope USER");
        }
    }

    void HandleNetworkQuery(VCString vsTokens, CIRCNetwork* network) {
        if (vsTokens[0].Equals("NETWORKPATH")) {
            NETWORKSEND(GetNetworkPath());
        } else if (vsTokens[0].Equals("NEXTSERVER")) {
            NETWORKSEND(GetNextServer());
        } else if (vsTokens[0].Equals("NICK")) {
            NETWORKSEND(GetNick());
        } else if (vsTokens[0].Equals("QUERIES")) {
            NETWORKSEND_LIST_OBJECT(GetQueries(), val->GetName());
        } else if (vsTokens[0].Equals("QUITMSG")) {
            NETWORKSEND(GetQuitMsg());
        } else if (vsTokens[0].Equals("REALNAME")) {
            NETWORKSEND(GetRealName());
        } else if (vsTokens[0].Equals("SERVERS")) {
            // TODO check that server->GetString() is really correct
            NETWORKSEND_LIST_OBJECT(GetServers(), val->GetString());
        } else if (vsTokens[0].Equals("TRUSTALLCERTS")) {
            NETWORKSEND_BOOL(GetTrustAllCerts());
        } else if (vsTokens[0].Equals("TRUSTEDFINGERPRINTS")) {
            NETWORKSEND_LIST_SIMPLE(GetTrustedFingerprints());
        } else if (vsTokens[0].Equals("TRUSTPKI")) {
            NETWORKSEND_BOOL(GetTrustPKI());
        } else if (vsTokens[0].Equals("HASSERVERS")) {
            NETWORKSEND_BOOL(HasServers());
        } else if (vsTokens[0].Equals("IRCCONNECTED")) {
            NETWORKSEND_BOOL(IsIRCConnected());
        } else if (vsTokens[0].Equals("ISIRCAWAY")) {
            NETWORKSEND_BOOL(IsIRCAway());
        } else if (vsTokens[0].Equals("ISLASTSERVER")) {
            NETWORKSEND_BOOL(IsLastServer());
        } else if (vsTokens[0].Equals("ISNETWORKATTACHED")) {
            NETWORKSEND_BOOL(IsNetworkAttached());
        } else if (vsTokens[0].Equals("ISUSERATTACHED")) {
            NETWORKSEND_BOOL(IsUserAttached());
        } else if (vsTokens[0].Equals("ISUSERONLINE")) {
            NETWORKSEND_BOOL(IsUserOnline());
        } else if (vsTokens[0].Equals("MODULES")) {
            PutModule("ENOSYS Module querying not implemented");
            return;
        } else if (vsTokens[0].Equals("CHANS")) {
            NETWORKSEND_LIST_OBJECT(GetChans(), val->GetName());
        } else if (vsTokens[0].Equals("QUERIES")) {
            NETWORKSEND_LIST_OBJECT(GetQueries(), val->GetName());
        } else if (vsTokens[0].Equals("CHANPREFIXES")) {
            NETWORKSEND(GetChanPrefixes());
        } else if (vsTokens[0].Equals("CURRENTSERVER")) {
            // TODO REALLY check this is correct
            NETWORKSEND(GetCurrentServer()->GetString());
        } else if (vsTokens[0].Equals("IRCCONNECTENABLED")) {
            NETWORKSEND_BOOL(GetIRCConnectEnabled());
        } else if (vsTokens[0].Equals("CURNICK")) {
            NETWORKSEND(GetCurNick());
        } else if (vsTokens[0].Equals("ALTNICK")) {
            NETWORKSEND(GetAltNick());
        } else if (vsTokens[0].Equals("IDENT")) {
            NETWORKSEND(GetIdent());
        } else if (vsTokens[0].Equals("ENCODING")) {
            NETWORKSEND(GetEncoding());
        } else if (vsTokens[0].Equals("BINDHOST")) {
            ACCESS_CHECK(GetUser()->DenySetBindHost(),
                         "DENYSETBINDHOST is TRUE");

            NETWORKSEND(GetBindHost());
        } else if (vsTokens[0].Equals("ENCODING")) {
            NETWORKSEND(GetEncoding());
        } else if (vsTokens[0].Equals("FLOODRATE")) {
            NETWORKSEND(GetFloodRate());
        } else if (vsTokens[0].Equals("FLOODBURST")) {
            NETWORKSEND(GetFloodBurst());
        } else if (vsTokens[0].Equals("JOINDELAY")) {
            NETWORKSEND(GetJoinDelay());
        } else if (vsTokens[0].Equals("BYTESREAD")) {
            NETWORKSEND(BytesRead());
        } else if (vsTokens[0].Equals("BYTESWRITTEN")) {
            NETWORKSEND(BytesWritten());
        } else {
            PutModule("EINVAL Unknown query in scope " + network->GetName());
        }
    }

    void CommandHelp(VCString vsTokens) {
        PutModule("Available commands can be listed using COMMANDS.");
        PutModule("Available query scopes can be listed using QUERYSCOPES.");
    }

    void CommandQuery(VCString vsTokens) {
        if (vsTokens.empty()) {
            PutModule("EINVAL Empty query");
            return;
        }

        if (vsTokens.size() > 2) {
            PutModule("EINVAL Too many tokens in query");
            return;
        }

        const auto queryFn = queryMap.find(vsTokens[0]);
        if (queryFn != queryMap.end()) {
            vsTokens.erase(vsTokens.begin());

            if (vsTokens.size() < 1) {
                PutModule("EINVAL No property specified");
                return;
            }

            queryFn->second(this, vsTokens);
        } else {
            CIRCNetwork* network = GetUser()->FindNetwork(vsTokens[0]);
            if (network) {
                vsTokens.erase(vsTokens.begin());
                HandleNetworkQuery(vsTokens, network);
            } else {
                PutModule("EINVAL Unknown query scope");
            }
        }
    }

    void CommandPing(VCString vsTokens) { PutModule("PONG"); }

    void CommandCommands(VCString vsTokens) {
        PutModule("LIST");
        for (const auto& command : commandMap) {
            PutModule(command.first);
        }
        PutModule("LISTEND");
    }

    void CommandQueryscopes(VCString vsTokens) {
        PutModule("LIST");
        for (const auto& query : queryMap) {
            PutModule(query.first);
        }
        // TODO make this dynamically dispatched too
        for (const CIRCNetwork* network : GetUser()->GetNetworks()) {
            PutModule(network->GetName());
        }
        PutModule("LISTEND");
    }

    void OnModCommand(const CString& sCommand) override {
        VCString vsTokens;
        sCommand.Split(" ", vsTokens, false, "", "", true, true);

        if (vsTokens.empty()) {
            PutModule("EINVAL Empty command");
            return;
        }

        const auto commandFn = commandMap.find(vsTokens[0]);
        if (commandFn != commandMap.end()) {
            vsTokens.erase(vsTokens.begin());

            commandFn->second(this, vsTokens);
        } else {
            PutModule("EINVAL Unknown command");
        }
    }
};

template <>
void TModInfo<CApi>(CModInfo& Info) {
    Info.SetWikiPage("api");
    Info.AddType(CModInfo::NetworkModule);
}

USERMODULEDEFS(CApi, t_s("Adds an *api user for programmatic queries"))
