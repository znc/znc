#include "main.h"
#include "Csocket.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include "FileUtils.h"
#include "znc.h"
#include <sys/wait.h>

// Forward Declaration
class CShellMod;

class CShellSock : public CExecSock {
public:
	CShellSock(CShellMod* pShellMod, CClient* pClient, const CString& sExec) : CExecSock( sExec ) {
		EnableReadLine();
		m_pParent = pShellMod;
		m_pClient = pClient;
	}
	// These next two function's bodies are at the bottom of the file since they reference CShellMod
	virtual void ReadLine(const CString& sData);
	virtual void Disconnected();

	CShellMod*	m_pParent;

private:
	CClient*	m_pClient;
};

class CShellMod : public CModule {
public:
	MODCONSTRUCTOR(CShellMod) {
		m_sPath = CZNC::Get().GetHomePath();
	}

	virtual ~CShellMod() {
		vector<Csock*> vSocks = m_pManager->FindSocksByName("SHELL");

		for (unsigned int a = 0; a < vSocks.size(); a++) {
			m_pManager->DelSockByAddr(vSocks[a]);
		}
	}

	virtual void OnModCommand(const CString& sCommand) {
		if ((strcasecmp(sCommand.c_str(), "cd") == 0) || (strncasecmp(sCommand.c_str(), "cd ", 3) == 0)) {
			CString sPath = CUtils::ChangeDir(m_sPath, ((sCommand.length() == 2) ? CString(CZNC::Get().GetHomePath()) : CString(sCommand.substr(3))), CZNC::Get().GetHomePath());
			CFile Dir(sPath);

			if (Dir.IsDir()) {
				m_sPath = sPath;
			} else if (Dir.Exists()) {
				PutShell("cd: not a directory [" + sPath + "]");
			} else {
				PutShell("cd: no such directory [" + sPath + "]");
			}

			PutShell("znc$");
		} else if (strcasecmp(sCommand.Token(0).c_str(), "SEND") == 0) {
			CString sToNick = sCommand.Token(1);
			CString sFile = sCommand.Token(2);

			if ((sToNick.empty()) || (sFile.empty())) {
				PutShell("usage: Send <nick> <file>");
			} else {
				sFile = CUtils::ChangeDir(m_sPath, sFile, CZNC::Get().GetHomePath());

				if (!CFile::Exists(sFile)) {
					PutShell("get: no such file [" + sFile + "]");
				} else if (!CFile::IsReg(sFile)) {
					PutShell("get: not a file [" + sFile + "]");
				} else {
					m_pUser->SendFile(sToNick, sFile, GetModName());
				}
			}
		} else if (strcasecmp(sCommand.Token(0).c_str(), "GET") == 0) {
			CString sFile = sCommand.Token(1);

			if (sFile.empty()) {
				PutShell("usage: Get <file>");
			} else {
				sFile = CUtils::ChangeDir(m_sPath, sFile, CZNC::Get().GetHomePath());

				if (!CFile::Exists(sFile)) {
					PutShell("get: no such file [" + sFile + "]");
				} else if (!CFile::IsReg(sFile)) {
					PutShell("get: not a file [" + sFile + "]");
				} else {
					m_pUser->SendFile(m_pUser->GetCurNick(), sFile, GetModName());
				}
			}
		} else {
			RunCommand(sCommand);
		}
	}

	virtual EModRet OnStatusCommand(const CString& sCommand) {
		if (strcasecmp(sCommand.c_str(), "SHELL") == 0) {
			PutShell("-- ZNC Shell Service --");
			return HALT;
		}

		return CONTINUE;
	}

	virtual EModRet OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize) {
		if (strcasecmp(RemoteNick.GetNick().c_str(), CString(GetModNick()).c_str()) == 0) {
			CString sLocalFile = CUtils::ChangeDir(m_sPath, sFile, CZNC::Get().GetHomePath());

			m_pUser->GetFile(m_pUser->GetCurNick(), CUtils::GetIP(uLongIP), uPort, sLocalFile, uFileSize, GetModName());

			return HALT;
		}

		return CONTINUE;
	}

	void PutShell(const CString& sLine) {
		CString sPath = m_sPath;

		CString::size_type a = sPath.find(' ');
		while (a != CString::npos) {
			sPath.replace(a, 1, "_");
			a = sPath.find(' ');
		}

		PutModule(sLine, m_pUser->GetCurNick(), sPath);
	}

	void RunCommand(const CString& sCommand) {
		m_pManager->AddSock((Csock*) new CShellSock(this, m_pClient, "cd " + m_sPath + " && " + sCommand), "SHELL");
	}
private:
	CString	m_sPath;
};

void CShellSock::ReadLine(const CString& sData) {
	CString sLine = sData;

	while ((sLine.length()) && (sLine[sLine.length() -1] == '\r') || (sLine[sLine.length() -1] == '\n')) {
		sLine = sLine.substr(0, sLine.length() -1);
	}

	CString::size_type a = sLine.find('\t');
	while (a != CString::npos) {
		sLine.replace(a, 1, "    ");
		a = sLine.find('\t');
	}

	m_pParent->SetClient(m_pClient);
	m_pParent->PutShell(sLine);
	m_pParent->SetClient(NULL);
}

void CShellSock::Disconnected() {
	m_pParent->SetClient(m_pClient);
	m_pParent->PutShell("znc$");
	m_pParent->SetClient(NULL);
}

MODULEDEFS(CShellMod, "Gives shell access")

