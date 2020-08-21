/*
 * Copyright (C) 2004-2020 ZNC, see the NOTICE file for details.
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

class CSampleWebAPIMod : public CModule {
  public:
    MODCONSTRUCTOR(CSampleWebAPIMod) {}

    ~CSampleWebAPIMod() override {}

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName != "index") {
            // only accept requests to index
            return false;
        }

        if (WebSock.IsPost()) {
            // print the text we just received
            CString text = WebSock.GetRawParam("text", true);
            WebSock.PrintHeader(text.length(), "text/plain; charset=UTF-8");
            WebSock.Write(text);
            WebSock.Close(Csock::CLT_AFTERWRITE);
            return false;
        }

        return true;
    }

    bool WebRequiresLogin() override {
        return false;
    }

    bool ValidateWebRequestCSRFCheck(CWebSock& WebSock,
        const CString& sPageName) override {
        return true;
    }
};

template <>
void TModInfo<CSampleWebAPIMod>(CModInfo& Info) {
    Info.SetWikiPage("samplewebapi");
}

GLOBALMODULEDEFS(CSampleWebAPIMod, t_s("Sample Web API module."))
