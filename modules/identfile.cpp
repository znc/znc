/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/FileUtils.h>
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/znc.h>

class CIdentFileModule : public CModule {
	CString m_sOrigISpoof;
	CFile* m_pISpoofLockFile;
	CIRCSock *m_pIRCSock;

public:
	MODCONSTRUCTOR(CIdentFileModule) {
		AddHelpCommand();
		AddCommand("GetFile",   static_cast<CModCommand::ModCmdFunc>(&CIdentFileModule::GetFile));
		AddCommand("SetFile",   static_cast<CModCommand::ModCmdFunc>(&CIdentFileModule::SetFile),
			"<file>");
		AddCommand("GetFormat", static_cast<CModCommand::ModCmdFunc>(&CIdentFileModule::GetFormat));
		AddCommand("SetFormat", static_cast<CModCommand::ModCmdFunc>(&CIdentFileModule::SetFormat),
			"<format>");
		AddCommand("Show",      static_cast<CModCommand::ModCmdFunc>(&CIdentFileModule::Show));

		m_pISpoofLockFile = NULL;
		m_pIRCSock = NULL;
	}

	virtual ~CIdentFileModule() {
		ReleaseISpoof();
	}

	void GetFile(const CString& sLine) {
		PutModule("File is set to: " + GetNV("File"));
	}

	void SetFile(const CString& sLine) {
		SetNV("File", sLine.Token(1, true));
		PutModule("File has been set to: " + GetNV("File"));
	}

	void SetFormat(const CString& sLine) {
		SetNV("Format", sLine.Token(1, true));
		PutModule("Format has been set to: " + GetNV("Format"));
		PutModule("Format would be expanded to: " + m_pUser->ExpandString(GetNV("Format")));
	}

	void GetFormat(const CString& sLine) {
		PutModule("Format is set to: " + GetNV("Format"));
		PutModule("Format would be expanded to: " + m_pUser->ExpandString(GetNV("Format")));
	}

	void Show(const CString& sLine) {
		PutModule("m_pISpoofLockFile = " + CString((long long)m_pISpoofLockFile));
		PutModule("m_pIRCSock = " + CString((long long)m_pIRCSock));
		if (m_pIRCSock) {
			PutModule("user/network - " + m_pIRCSock->GetNetwork()->GetUser()->GetUserName() + "/" + m_pIRCSock->GetNetwork()->GetName());
		}
	}

	void OnModCommand(const CString& sCommand) {
		if (m_pUser->IsAdmin()) {
			HandleCommand(sCommand);
		} else {
			PutModule("Access denied");
		}
	}

	void SetIRCSock(CIRCSock *pIRCSock) {
		if (m_pIRCSock) {
			CZNC::Get().ResumeConnectQueue();
		}

		m_pIRCSock = pIRCSock;

		if (m_pIRCSock) {
			CZNC::Get().PauseConnectQueue();
		}
	}

	bool WriteISpoof() {
		if (m_pISpoofLockFile != NULL) {
			return false;
		}

		m_pISpoofLockFile = new CFile;
		if (!m_pISpoofLockFile->TryExLock(GetNV("File"), O_RDWR | O_CREAT)) {
			delete m_pISpoofLockFile;
			m_pISpoofLockFile = NULL;
			return false;
		}

		char buf[1024];
		memset((char*) buf, 0, 1024);
		m_pISpoofLockFile->Read(buf ,1024);
		m_sOrigISpoof = buf;

		if (!m_pISpoofLockFile->Seek(0) || !m_pISpoofLockFile->Truncate()) {
			delete m_pISpoofLockFile;
			m_pISpoofLockFile = NULL;
			return false;
		}

		CString sData = m_pNetwork->ExpandString(GetNV("Format"));

		// If the format doesn't contain anything expandable, we'll
		// assume this is an "old"-style format string.
		if (sData == GetNV("Format")) {
			sData.Replace("%", m_pUser->GetIdent());
		}

		DEBUG("Writing [" + sData + "] to ident spoof file [" + m_pISpoofLockFile->GetLongName() + "] for user/network [" + m_pUser->GetUserName() + "/" + m_pNetwork->GetName() + "]");

		m_pISpoofLockFile->Write(sData + "\n");

		return true;
	}

	void ReleaseISpoof() {
		DEBUG("Releasing ident spoof for user/network [" + (m_pUser ? m_pUser->GetUserName() : "<no user>") + "/" +
				(m_pNetwork ? m_pNetwork->GetName() : "<no network>") + "]");

		SetIRCSock(NULL);

		if (m_pISpoofLockFile != NULL) {
			if (m_pISpoofLockFile->Seek(0) && m_pISpoofLockFile->Truncate()) {
				m_pISpoofLockFile->Write(m_sOrigISpoof);
			}

			delete m_pISpoofLockFile;
			m_pISpoofLockFile = NULL;
		}
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		m_pISpoofLockFile = NULL;
		m_pIRCSock = NULL;

		if (GetNV("Format").empty()) {
			SetNV("Format", "global { reply \"%ident%\" }");
		}

		if (GetNV("File").empty()) {
			SetNV("File", "~/.oidentd.conf");
		}

		return true;
	}

	virtual EModRet OnIRCConnecting(CIRCSock *pIRCSock) {
		if (m_pISpoofLockFile != NULL) {
			DEBUG("Aborting connection, ident spoof lock file exists");
			PutModule("Aborting connection, another user or network is currently connecting and using the ident spoof file");
			return HALTCORE;
		}

		if (!WriteISpoof()) {
			DEBUG("identfile [" + GetNV("File") + "] could not be written");
			PutModule("[" + GetNV("File") + "] could not be written, retrying...");
			return HALTCORE;
		}

		SetIRCSock(pIRCSock);
		return CONTINUE;
	}

	virtual void OnIRCConnected() {
		if (m_pIRCSock == m_pNetwork->GetIRCSock()) {
			ReleaseISpoof();
		}
	}

	virtual void OnIRCConnectionError(CIRCSock *pIRCSock) {
		if (m_pIRCSock == pIRCSock) {
			ReleaseISpoof();
		}
	}

	virtual void OnIRCDisconnected() {
		if (m_pIRCSock == m_pNetwork->GetIRCSock()) {
			ReleaseISpoof();
		}
	}
};

template<> void TModInfo<CIdentFileModule>(CModInfo& Info) {
	Info.SetWikiPage("identfile");
}

GLOBALMODULEDEFS(CIdentFileModule, "Write the ident of a user to a file when they are trying to connect")
