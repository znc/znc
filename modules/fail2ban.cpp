/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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
#include <znc/znc.h>

#include <syslog.h>

class CFailToBanMod : public CModule {
public:
	MODCONSTRUCTOR(CFailToBanMod) {
		openlog("znc-fail2ban", LOG_PID, LOG_DAEMON);
	}

	virtual ~CFailToBanMod() {
		Log("fail2ban Logging ended.");
		closelog();
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		CString sTimeout = sArgs.Token(0);
		CString sAttempts = sArgs.Token(1);
		CString sLogTarget = sArgs.Token(2);
		unsigned int timeout = sTimeout.ToUInt();

		if (sAttempts.empty())
			m_uiAllowedFailed = 2;
		else
			m_uiAllowedFailed = sAttempts.ToUInt();;

		if (sLogTarget.Equals("syslog"))
			m_eLogMode = LOG_TO_SYSLOG;
		else if (sLogTarget.Equals("both"))
			m_eLogMode = LOG_TO_BOTH;
		else if (sLogTarget.Equals("file"))
			m_eLogMode = LOG_TO_FILE;
		else
			m_eLogMode = LOG_TO_NONE;

		m_sLogFile = GetSavePath() + "/fail2ban.log";

		if (sArgs.empty()) {
			timeout = 1;
		} else if (timeout == 0 || m_uiAllowedFailed == 0 || !sArgs.Token(3, true).empty()) {
			sMessage = "Invalid argument, must be the number of minutes "
				"IPs are blocked after a failed login and can be "
				"followed by number of allowed failed login attempts"
			        "and can be followed optionally by 'file', 'syslog', or 'both'"
			        "to log all actions to the specified destination.";
			return false;
		}

		// SetTTL() wants milliseconds
		m_Cache.SetTTL(timeout * 60 * 1000);

		Log("Logging started. ZNC PID[" + CString(getpid()) + "] UID/GID[" + CString(getuid()) + ":" + CString(getgid()) + "]");

		return true;
	}

	void Log(CString sLine, int iPrio = LOG_INFO) {
		if (m_eLogMode & LOG_TO_SYSLOG)
			syslog(iPrio, "%s", sLine.c_str());

		if (m_eLogMode & LOG_TO_FILE) {
			time_t curtime;
			tm* timeinfo;
			char buf[23];

			time(&curtime);
			timeinfo = localtime(&curtime);
			strftime(buf,sizeof(buf),"[%Y-%m-%d %H:%M:%S] ",timeinfo);

			CFile LogFile(m_sLogFile);

			if (LogFile.Open(O_WRONLY | O_APPEND | O_CREAT))
				LogFile.Write(buf + sLine + "\n");
			else
				DEBUG("Failed to write to [" << m_sLogFile  << "]: " << strerror(errno));
		}
	}


	virtual void OnPostRehash() {
		m_Cache.Clear();
	}

	void Add(const CString& sHost, unsigned int count) {
		m_Cache.AddItem(sHost, count, m_Cache.GetTTL());
	}

	virtual void OnModCommand(const CString& sCommand) {
		PutModule("This module can only be configured through its arguments.");
		PutModule("The module argument is the number of minutes an IP");
		PutModule("is blocked after a failed login, followed optionally by.");
		PutModule("the number of failed logins that trigger a block, followed");
		PutModule("optionally by 'file', 'syslog' or 'both' to write all actions");
		PutModule("to the specified destination.");
	}

	virtual void OnClientConnect(CZNCSock* pClient, const CString& sHost, unsigned short uPort) {
		unsigned int *pCount = m_Cache.GetItem(sHost);
		if (sHost.empty() || pCount == NULL || *pCount < m_uiAllowedFailed) {
			return;
		}

		// refresh their ban
		Add(sHost, *pCount);
		Log("banning client " + sHost + " - reconnecting too fast - count is " + CString(*pCount));

		pClient->Write("ERROR :Closing link [Please try again later - reconnecting too fast]\r\n");
		pClient->Close(Csock::CLT_AFTERWRITE);
	}

	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {
		unsigned int *pCount = m_Cache.GetItem(sRemoteIP);
		if (pCount) {
			Add(sRemoteIP, *pCount + 1);
			Log("Failed login from user " + sUsername + " remote IP " + sRemoteIP + " previous count " + CString(*pCount));
		}
		else {
			Add(sRemoteIP, 1);
		        Log("Failed login from user " + sUsername + " remote IP " + sRemoteIP);
		}
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		// e.g. webadmin ends up here
		const CString& sRemoteIP = Auth->GetRemoteIP();

		if (sRemoteIP.empty())
			return CONTINUE;

		unsigned int *pCount = m_Cache.GetItem(sRemoteIP);
		if (pCount && *pCount >= m_uiAllowedFailed) {
			// OnFailedLogin() will refresh their ban
		        Log("OnLoginAttempt (web admin) - blocked login from " + sRemoteIP + " fail count is " + CString(*pCount));
			Auth->RefuseLogin("Please try again later - reconnecting too fast");
			return HALT;
		}

		return CONTINUE;
	}

private:
	TCacheMap<CString, unsigned int> m_Cache;
	unsigned int                     m_uiAllowedFailed;
	enum LogMode {
	        LOG_TO_NONE   = 0,
		LOG_TO_FILE   = 1 << 0,
		LOG_TO_SYSLOG = 1 << 1,
		LOG_TO_BOTH   = LOG_TO_FILE | LOG_TO_SYSLOG
	};
	LogMode m_eLogMode;
        CString                          m_sLogFile;
};

template<> void TModInfo<CFailToBanMod>(CModInfo& Info) {
	Info.SetWikiPage("fail2ban");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Module takes three arguments: 1) the time in minutes for the IP banning, 2) the number of failed logins before any action is taken, 3) optional, 'file', 'syslog' or 'both' to cause the module to log all actions to the specified destination.");
}

GLOBALMODULEDEFS(CFailToBanMod, "Block IPs for some time after a failed login")
