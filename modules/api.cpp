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

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/znc.h>

using std::vector;

CString b2s(const bool b) {
    if (b) return CString("TRUE");
    return CString("FALSE");
}

#define USERSEND(fn) _WrapValue(CString(GetUser()->fn));
#define USERSEND_BOOL(fn) _WrapValue(b2s(GetUser()->fn));
#define ACCESS_CHECK(checkFn, reason) \
    if (checkFn) {                    \
        PutModule("EACCES " reason);  \
        return;                       \
    }

class CApi : public CModule {
  public:
    MODCONSTRUCTOR(CApi) {}

    ~CApi() override {}

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

            PutModule(CZNC::GetVersion());
        } else if (vsTokens[0].Equals("UPTIME")) {
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

    void OnModCommand(const CString& sCommand) override {
        VCString vsTokens;
        sCommand.Split(" ", vsTokens, false, "", "", true, true);

        if (vsTokens.empty()) {
            PutModule("EINVAL Empty command");
            return;
        }

        if (vsTokens[0].Equals("QUERY")) {
            vsTokens.erase(vsTokens.begin());

            if (vsTokens.empty()) {
                PutModule("EINVAL Empty query");
                return;
            }

            if (vsTokens.size() > 2) {
                PutModule("EINVAL Too many tokens in query");
                return;
            }

            if (vsTokens[0].Equals("USER")) {
                vsTokens.erase(vsTokens.begin());

                HandleUserQuery(vsTokens);
            } else if (vsTokens[0].Equals("ZNC")) {
                vsTokens.erase(vsTokens.begin());

                HandleZNCQuery(vsTokens);
            } else {
                PutModule("EINVAL Unknown query scope");
            }
        } else if (vsTokens[0].Equals("PING")) {
            PutModule("PONG");
        } else if (vsTokens[0].Equals("HELP")) {
            PutModule("Available commands can be listed using COMMANDS.");
            PutModule(
                "Available query scopes can be listed using QUERYSCOPES.");
        } else if (vsTokens[0].Equals("COMMANDS")) {
            PutModule("LIST");
            PutModule("QUERY");
            PutModule("PING");
            PutModule("HELP");
            PutModule("COMMANDS");
            PutModule("QUERYSCOPES");
            PutModule("LISTEND");
        } else if (vsTokens[0].Equals("QUERYSCOPES")) {
            PutModule("LIST");
            PutModule("ZNC");
            PutModule("USER");
            PutModule("LISTEND");
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
