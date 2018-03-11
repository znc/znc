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
#include <znc/IRCNetwork.h>

using std::set;

class CBlockMotd : public CModule {
  public:
    MODCONSTRUCTOR(CBlockMotd) {
        AddHelpCommand();
        AddCommand("GetMotd", t_d("[<server>]"),
                   t_d("Override the block with this command. Can optionally "
                       "specify which server to query."),
                   [this](const CString& sLine) { OverrideCommand(sLine); });
    }

    ~CBlockMotd() override {}

    void OverrideCommand(const CString& sLine) {
        if (!GetNetwork() || !GetNetwork()->GetIRCSock()) {
            PutModule(t_s("You are not connected to an IRC Server."));
            return;
        }

        TemporarilyAcceptMotd();
        const CString sServer = sLine.Token(1);

        if (sServer.empty()) {
            PutIRC("MOTD");
        } else {
            PutIRC("MOTD " + sServer);
        }
    }

    EModRet OnNumericMessage(CNumericMessage& Message) override {
        if ((Message.GetCode() == 375 /* begin of MOTD */ ||
             Message.GetCode() == 372 /* MOTD */) &&
            !ShouldTemporarilyAcceptMotd())
            return HALT;

        if (Message.GetCode() == 376 /* End of MOTD */) {
            if (!ShouldTemporarilyAcceptMotd()) {
                Message.SetParam(1, t_s("MOTD blocked by ZNC"));
            }
            StopTemporarilyAcceptingMotd();
        }

        if (Message.GetCode() == 422) {
            // Server has no MOTD
            StopTemporarilyAcceptingMotd();
        }

        return CONTINUE;
    }

    void OnIRCDisconnected() override {
        StopTemporarilyAcceptingMotd();
    }

    bool ShouldTemporarilyAcceptMotd() const {
        return m_sTemporaryAcceptedMotdSocks.count(GetNetwork()->GetIRCSock()) > 0;
    }

    void TemporarilyAcceptMotd() {
        if (ShouldTemporarilyAcceptMotd()) {
            return;
        }

        m_sTemporaryAcceptedMotdSocks.insert(GetNetwork()->GetIRCSock());
    }

    void StopTemporarilyAcceptingMotd() {
        m_sTemporaryAcceptedMotdSocks.erase(GetNetwork()->GetIRCSock());
    }

  private:
    set<CIRCSock *> m_sTemporaryAcceptedMotdSocks;
};

template <>
void TModInfo<CBlockMotd>(CModInfo& Info) {
    Info.AddType(CModInfo::NetworkModule);
    Info.AddType(CModInfo::GlobalModule);
    Info.SetWikiPage("block_motd");
}

USERMODULEDEFS(
    CBlockMotd,
    t_s("Block the MOTD from IRC so it's not sent to your client(s)."))
