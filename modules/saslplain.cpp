/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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
#include <znc/znc.h>

class CSASLMechanismPlain : public CModule {
  public:
    MODCONSTRUCTOR(CSASLMechanismPlain) { AddHelpCommand(); }

    EModRet OnClientSASLAuthenticate(const CString& sMechanism,
                                     const CString& sBuffer, CString& sUser,
                                     CString& sMechanismResponse,
                                     bool& bAuthenticationSuccess) override {
        if (!sMechanism.Equals("PLAIN")) {
            return CONTINUE;
        }

        bAuthenticationSuccess = false;

        CString sNullSeparator = std::string("\0", 1);
        auto sAuthzId = sBuffer.Token(0, false, sNullSeparator, true);
        auto sAuthcId = sBuffer.Token(1, false, sNullSeparator, true);
        auto sPassword = sBuffer.Token(2, false, sNullSeparator, true);

        if (!sAuthzId.empty() && sAuthzId != sAuthcId) {
            // Reject custom SASL plain authorization identifiers
            return HALTMODS;
        }

        auto spAuth = std::make_shared<CClientAuth>(this, sAuthcId, sPassword);
        CZNC::Get().AuthUser(spAuth);
        return HALTMODS;
    }

    void OnGetSASLMechanisms(SCString& ssMechanisms) override {
        ssMechanisms.insert("PLAIN");
    }
};

template <>
void TModInfo<CSASLMechanismPlain>(CModInfo& Info) {
    Info.SetWikiPage("saslplain");
}

GLOBALMODULEDEFS(
    CSASLMechanismPlain,
    t_s("Allows users to authenticate via the PLAIN SASL mechanism."))
