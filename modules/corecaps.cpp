/*
 * Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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

#include <znc/Client.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>

#include <memory>

class CCoreCaps : public CModule {
    // Note: for historical reasons CClient and CIRCSock have such fields, but
    // really they should not.
    // TODO: move these fields and their handling from core to this module.
    class AwayNotify : public CCapability {
        void OnServerChangedSupport(CIRCNetwork* pNetwork,
                                    bool bState) override {
            pNetwork->GetIRCSock()->m_bAwayNotify = bState;
        }
        void OnClientChangedSupport(CClient* pClient, bool bState) override {
            pClient->m_bAwayNotify = bState;
        }
    };

    class AccountNotify : public CCapability {
        void OnServerChangedSupport(CIRCNetwork* pNetwork,
                                    bool bState) override {
            pNetwork->GetIRCSock()->m_bAccountNotify = bState;
        }
        void OnClientChangedSupport(CClient* pClient, bool bState) override {
            pClient->m_bAccountNotify = bState;
        }
    };

    class AccountTag : public CCapability {
        void OnClientChangedSupport(CClient* pClient, bool bState) override {
            pClient->SetTagSupport("account", bState);
        }
    };

    class ExtendedJoin : public CCapability {
        void OnServerChangedSupport(CIRCNetwork* pNetwork,
                                    bool bState) override {
            pNetwork->GetIRCSock()->m_bExtendedJoin = bState;
        }
        void OnClientChangedSupport(CClient* pClient, bool bState) override {
            pClient->m_bExtendedJoin = bState;
        }
    };

  public:
    MODCONSTRUCTOR(CCoreCaps) {
        AddServerDependentCapability("away-notify", std::make_unique<AwayNotify>());
        AddServerDependentCapability("account-notify", std::make_unique<AccountNotify>());
        AddServerDependentCapability("account-tag", std::make_unique<AccountTag>());
        AddServerDependentCapability("extended-join", std::make_unique<ExtendedJoin>());
    }
};

GLOBALMODULEDEFS(
    CCoreCaps,
    t_s("Adds support for several IRC capabilities, extracted from ZNC core."))
