#ifndef _NO_CSOCKET_NS
#define _NO_CSOCKET_NS
#endif

#include "Csocket.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <sys/wait.h>

// Forward Declaration
class CShellMod;

class CExecSock : public Csock {
public:
	CExecSock(CShellMod* pShellMod, const string& sExec) : Csock() {
		EnableReadLine();
		m_pParent = pShellMod;
		int iReadFD, iWriteFD;
		m_iPid = popen2(iReadFD, iWriteFD, sExec);
		ConnectFD(iReadFD, iWriteFD, "0.0.0.0:0");
	}

	virtual ~CExecSock() {
		close2(m_iPid, GetRSock(), GetWSock());
		SetRSock( -1 );
		SetWSock( -1 );
	}

	// These next two function's bodies are at the bottom of the file since they reference CShellMod
	virtual void ReadLine(const string& sData);
	virtual void Disconnected();

	CShellMod*	m_pParent;
	int			m_iPid;

	int popen2(int & iReadFD, int & iWriteFD, const string & sCommand) {
		int rpipes[2] = { -1, -1 };
		int wpipes[2] = { -1, -1 };
		iReadFD = -1;
		iWriteFD = -1;

		pipe(rpipes);
		pipe(wpipes);
		
		int iPid = fork();

		if (iPid == -1) {
			return -1;
		}

		if (iPid == 0) {
			close(wpipes[1]);
			close(rpipes[0]);
			dup2(wpipes[0], 0);
			dup2(rpipes[1], 1);
			dup2(rpipes[1], 2);
			close(wpipes[0]);
			close(rpipes[1]);
			system( sCommand.c_str() );
			exit(0);
		}

		close(wpipes[0]);
		close(rpipes[1]);

		iWriteFD = wpipes[1];
		iReadFD = rpipes[0];

		return iPid;
	}

	void close2(int iPid, int iReadFD, int iWriteFD) {
		close( iReadFD );
		close( iWriteFD );
		u_int iNow = time( NULL );
		while( waitpid( iPid, NULL, WNOHANG ) == 0 )
		{
			if ( ( time( NULL ) - iNow ) > 5 )
				break;	// giveup
			usleep( 100 );
		}
		return;
	}
};

class CShellMod : public CModule {
public:
	MODCONSTRUCTOR(CShellMod) {
		m_sPath = pUser->GetHomePath();
	}

	virtual ~CShellMod() {
		vector<Csock*> vSocks = m_pManager->FindSocksByName("SHELL");

		for (unsigned int a = 0; a < vSocks.size(); a++) {
			m_pManager->DelSockByAddr(vSocks[a]);
		}
	}

	virtual string GetDescription() {
		return "Gives shell access.";
	}

	virtual void OnModCommand(const string& sCommand) {
		if ((strcasecmp(sCommand.c_str(), "cd") == 0) || (strncasecmp(sCommand.c_str(), "cd ", 3) == 0)) {
			string sPath = CUtils::ChangeDir(m_sPath, ((sCommand.length() == 2) ? m_pUser->GetHomePath() : sCommand.substr(3)), m_pUser->GetHomePath());
			CFile Dir(sPath);

			if (Dir.IsDir()) {
				m_sPath = sPath;
			} else if (Dir.Exists()) {
				PutShell("cd: not a directory [" + sPath + "]");
			} else {
				PutShell("cd: no such directory [" + sPath + "]");
			}

			PutShell("znc$");
		} else if (strcasecmp(CUtils::Token(sCommand, 0).c_str(), "SEND") == 0) {
			string sToNick = CUtils::Token(sCommand, 1);
			string sFile = CUtils::Token(sCommand, 2);

			if ((sToNick.empty()) || (sFile.empty())) {
				PutShell("usage: Send <nick> <file>");
			} else {
				sFile = CUtils::ChangeDir(m_sPath, sFile, m_pUser->GetHomePath());

				if (!CFile::Exists(sFile)) {
					PutShell("get: no such file [" + sFile + "]");
				} else if (!CFile::IsReg(sFile)) {
					PutShell("get: not a file [" + sFile + "]");
				} else {
					m_pUser->SendFile(sToNick, sFile, GetModName());
				}
			}
		} else if (strcasecmp(CUtils::Token(sCommand, 0).c_str(), "GET") == 0) {
			string sFile = CUtils::Token(sCommand, 1);

			if (sFile.empty()) {
				PutShell("usage: Get <file>");
			} else {
				sFile = CUtils::ChangeDir(m_sPath, sFile, m_pUser->GetHomePath());

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

	virtual bool OnStatusCommand(const string& sCommand) {
		if (strcasecmp(sCommand.c_str(), "SHELL") == 0) {
			PutShell("-- ZNC Shell Service --");
			return true;
		}

		return false;
	}

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const string& sFile, unsigned long uFileSize) {
		if (strcasecmp(RemoteNick.GetNick().c_str(), string(GetModNick()).c_str()) == 0) {
			string sLocalFile = CUtils::ChangeDir(m_sPath, sFile, m_pUser->GetHomePath());

			m_pUser->GetFile(m_pUser->GetCurNick(), CUtils::GetIP(uLongIP), uPort, sLocalFile, uFileSize, GetModName());

			return true;
		}

		return false;
	}

	void PutShell(const string& sLine) {
		string sPath = m_sPath;

		string::size_type a = sPath.find(' ');
		while (a != string::npos) {
			sPath.replace(a, 1, "_");
			a = sPath.find(' ');
		}

		PutModule(sLine, m_pUser->GetCurNick(), sPath);
	}

	void RunCommand(const string& sCommand) {
		m_pManager->AddSock((Csock*) new CExecSock(this, "cd " + m_sPath + " && " + sCommand), "SHELL");
	}
private:
	string	m_sPath;
};

void CExecSock::ReadLine(const string& sData) {
	string sLine = sData;

	while ((sLine.length()) && (sLine[sLine.length() -1] == '\r') || (sLine[sLine.length() -1] == '\n')) {
		sLine = sLine.substr(0, sLine.length() -1);
	}

	string::size_type a = sLine.find('\t');
	while (a != string::npos) {
		sLine.replace(a, 1, "    ");
		a = sLine.find('\t');
	}

	m_pParent->PutShell(sLine);
}

void CExecSock::Disconnected() {
	m_pParent->PutShell("znc$");
}

MODULEDEFS(CShellMod)

