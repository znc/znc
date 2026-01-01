/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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
    MODCONSTRUCTOR(CSASLMechanismPlain) {}

    void OnClientGetSASLMechanisms(SCString& ssMechanisms) override {
        ssMechanisms.insert("PLAIN");
    }

    EModRet OnClientSASLAuthenticate(const CString& sMechanism,
                                     const CString& sMessage) override {
        if (sMechanism != "PLAIN") {
            return CONTINUE;
        }

        CString sNullSeparator = std::string("\0", 1);
        CString sAuthzId = sMessage.Token(0, false, sNullSeparator, true);
        CString sAuthcId = sMessage.Token(1, false, sNullSeparator, true);
        CString sPassword = sMessage.Token(2, false, sNullSeparator, true);

        if (sAuthzId.empty()) {
            sAuthzId = sAuthcId;
        }

        GetClient()->StartSASLPasswordCheck(sAuthcId, sPassword, sAuthzId);
        return HALTMODS;
    }
};

template <>
void TModInfo<CSASLMechanismPlain>(CModInfo& Info) {
    Info.SetWikiPage("saslplainauth");
}

GLOBALMODULEDEFS(
    CSASLMechanismPlain,
    t_s("Allows users to authenticate via the PLAIN SASL mechanism."))
