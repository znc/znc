/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#define REQUIRESSL

#include "FileUtils.h"
#include "User.h"
#include "Modules.h"
#include "IRCSock.h"

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
			if (m_pUser->IsAdmin()) {
				PutModule("Alternatively you can either place one at " + PemFile() + " or use the GENERATE command to generate a new certificate");
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

	virtual EModRet OnIRCConnecting(CIRCSock *pIRCSock) {
		if (HasPemFile()) {
			pIRCSock->SetPemLocation(PemFile());
		}

		return CONTINUE;
	}

	virtual CString GetWebMenuTitle() { return "Certificate"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "index") {
			Tmpl["Cert"] = CString(HasPemFile());
			return true;
		} else if (sPageName == "update") {
			CFile fPemFile(PemFile());

			if (fPemFile.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
				fPemFile.Write(WebSock.GetParam("cert", true, ""));
				fPemFile.Close();
			}

			WebSock.Redirect("/mods/cert/");
			return true;
		} else if (sPageName == "delete") {
			CFile::Delete(PemFile());
			WebSock.Redirect("/mods/cert/");
			return true;
		}

		return false;
	}
};

template<> void TModInfo<CCertMod>(CModInfo& Info) {
	Info.SetWikiPage("cert");
}

MODULEDEFS(CCertMod, "Use a ssl certificate to connect to a server")
