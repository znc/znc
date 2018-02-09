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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/User.h>

class CKickClientOnIRCDisconnect : public CModule {
  public:
    MODCONSTRUCTOR(CKickClientOnIRCDisconnect) {}

    void OnIRCDisconnected() override {
        CString sPrefix = GetUser()->GetStatusPrefix();
        for (CChan* pChan : GetNetwork()->GetChans()) {
            if (pChan->IsOn()) {
                PutUser(":" + sPrefix + "disconkick!znc@znc.in KICK " +
                        pChan->GetName() + " " +
                        GetNetwork()->GetIRCNick().GetNick() + " :" +
                        t_s("You have been disconnected from the IRC server"));
            }
        }
    }
};

template <>
void TModInfo<CKickClientOnIRCDisconnect>(CModInfo& Info) {
    Info.SetWikiPage("disconkick");
}

USERMODULEDEFS(
    CKickClientOnIRCDisconnect,
    t_s("Kicks the client from all channels when the connection to the "
        "IRC server is lost"))
