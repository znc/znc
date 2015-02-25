/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#define REQUIRESSL

#include <znc/FileUtils.h>
#include <znc/User.h>
#include <znc/Modules.h>
#include <znc/IRCSock.h>

class CCertMod : public CModule {
public:
	void Delete(const CString &line) {
		if (CFile::Delete(PemFile())) {
			PutModule("Pem file deleted");
		} else {
			PutModule("The pem file doesn't exist or there was a error deleting the pem file.");
		}
	}

	void Info(const CString &line) {
		if (HasPemFile()) {
			PutModule("You have a certificate in: " + PemFile());
		} else {
			PutModule("You do not have a certificate. Please use the web interface to add a certificate");
			if (GetUser()->IsAdmin()) {
				PutModule("Alternatively you can either place one at " + PemFile());
			}
		}
	}

	MODCONSTRUCTOR(CCertMod) {
		AddHelpCommand();
		AddCommand("delete", static_cast<CModCommand::ModCmdFunc>(&CCertMod::Delete),
			"", "Delete the current certificate");
		AddCommand("info", static_cast<CModCommand::ModCmdFunc>(&CCertMod::Info));
	}

	virtual ~CCertMod() {}

	CString PemFile() const {
		return GetSavePath() + "/user.pem";
	}

	bool HasPemFile() const {
		return (CFile::Exists(PemFile()));
	}

	EModRet OnIRCConnecting(CIRCSock *pIRCSock) override {
		if (HasPemFile()) {
			pIRCSock->SetPemLocation(PemFile());
		}

		return CONTINUE;
	}

	CString GetWebMenuTitle() override { return "Certificate"; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName == "index") {
			Tmpl["Cert"] = CString(HasPemFile());
			return true;
		} else if (sPageName == "update") {
			CFile fPemFile(PemFile());

			if (fPemFile.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
				fPemFile.Write(WebSock.GetParam("cert", true, ""));
				fPemFile.Close();
			}

			WebSock.Redirect(GetWebPath());
			return true;
		} else if (sPageName == "delete") {
			CFile::Delete(PemFile());
			WebSock.Redirect(GetWebPath());
			return true;
		}

		return false;
	}
};

template<> void TModInfo<CCertMod>(CModInfo& Info) {
	Info.AddType(CModInfo::UserModule);
	Info.SetWikiPage("cert");
}

NETWORKMODULEDEFS(CCertMod, "Use a ssl certificate to connect to a server")
