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

#include <znc/FileUtils.h>
#include <znc/User.h>
#include <znc/znc.h>
#include <znc/ExecSock.h>

using std::vector;

// Forward Declaration
class CShellMod;

class CShellSock : public CExecSock {
public:
	CShellSock(CShellMod* pShellMod, CClient* pClient, const CString& sExec) : CExecSock() {
		EnableReadLine();
		m_pParent = pShellMod;
		m_pClient = pClient;

		if (Execute(sExec) == -1) {
			CString s = "Failed to execute: ";
			s += strerror(errno);
			ReadLine(s);
			return;
		}

		// Get rid of that write fd, we aren't going to use it
		// (And clients expecting input will fail this way).
		close(GetWSock());
		SetWSock(open("/dev/null", O_WRONLY));
	}
	// These next two function's bodies are at the bottom of the file since they reference CShellMod
	void ReadLine(const CString& sData) override;
	void Disconnected() override;

	CShellMod* m_pParent;

private:
	CClient*   m_pClient;
};

class CShellMod : public CModule {
public:
	MODCONSTRUCTOR(CShellMod) {
		m_sPath = CZNC::Get().GetHomePath();
	}

	virtual ~CShellMod() {
		vector<Csock*> vSocks = GetManager()->FindSocksByName("SHELL");

		for (unsigned int a = 0; a < vSocks.size(); a++) {
			GetManager()->DelSockByAddr(vSocks[a]);
		}
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override
	{
#ifndef MOD_SHELL_ALLOW_EVERYONE
		if (!GetUser()->IsAdmin()) {
			sMessage = "You must be admin to use the shell module";
			return false;
		}
#endif

		return true;
	}

	void OnModCommand(const CString& sLine) override {
		CString sCommand = sLine.Token(0);
		if (sCommand.Equals("cd")) {
			CString sArg = sLine.Token(1, true);
			CString sPath = CDir::ChangeDir(m_sPath, (sArg.empty() ? CString(CZNC::Get().GetHomePath()) : sArg), CZNC::Get().GetHomePath());
			CFile Dir(sPath);

			if (Dir.IsDir()) {
				m_sPath = sPath;
			} else if (Dir.Exists()) {
				PutShell("cd: not a directory [" + sPath + "]");
			} else {
				PutShell("cd: no such directory [" + sPath + "]");
			}

			PutShell("znc$");
		} else {
			RunCommand(sLine);
		}
	}

	void PutShell(const CString& sMsg) {
		CString sPath = m_sPath.Replace_n(" ", "_");
		CString sSource = ":" + GetModNick() + "!shell@" + sPath;
		CString sLine = sSource + " PRIVMSG " + GetClient()->GetNick() + " :" + sMsg;
		GetClient()->PutClient(sLine);
	}

	void RunCommand(const CString& sCommand) {
		GetManager()->AddSock(new CShellSock(this, GetClient(), "cd " + m_sPath + " && " + sCommand), "SHELL");
	}
private:
	CString m_sPath;
};

void CShellSock::ReadLine(const CString& sData) {
	CString sLine = sData;

	sLine.TrimRight("\r\n");
	sLine.Replace("\t", "    ");

	m_pParent->SetClient(m_pClient);
	m_pParent->PutShell(sLine);
	m_pParent->SetClient(NULL);
}

void CShellSock::Disconnected() {
	// If there is some incomplete line in the buffer, read it
	// (e.g. echo echo -n "hi" triggered this)
	CString &sBuffer = GetInternalReadBuffer();
	if (!sBuffer.empty())
		ReadLine(sBuffer);

	m_pParent->SetClient(m_pClient);
	m_pParent->PutShell("znc$");
	m_pParent->SetClient(NULL);
}

template<> void TModInfo<CShellMod>(CModInfo& Info) {
	Info.SetWikiPage("shell");
}

#ifdef MOD_SHELL_ALLOW_EVERYONE
USERMODULEDEFS(CShellMod, "Gives shell access")
#else
USERMODULEDEFS(CShellMod, "Gives shell access. Only ZNC admins can use it.")
#endif
