/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Quiet Away and message logger
 * Author: imaginos <imaginos@imaginos.net>
 */

#define REQUIRESSL

#include "User.h"
#include <sys/stat.h>

#define CRYPT_VERIFICATION_TOKEN "::__:AWAY:__::"

class CAway;

class CAwayJob : public CTimer
{
public:
	CAwayJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CAwayJob() {}

protected:
	virtual void RunJob();
};

class CAway : public CModule
{
public:
	MODCONSTRUCTOR(CAway)
	{
		Ping();
		m_bIsAway = false;
		m_bBootError = false;
		m_saveMessages = true;
		SetAwayTime(300);
		AddTimer(new CAwayJob(this, 60, 0, "AwayJob", "Checks for idle and saves messages every 1 minute"));
	}
	virtual ~CAway()
	{
		if (!m_bBootError)
			SaveBufferToDisk();
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		CString sMyArgs = sArgs;
		size_t uIndex = 0;
		if (sMyArgs.Token(0) == "-nostore")
		{
			uIndex++;
			m_saveMessages = false;
		}
		if (sMyArgs.Token(uIndex) == "-notimer")
		{
			SetAwayTime(0);
			sMyArgs = sMyArgs.Token(uIndex + 1, true);
		} else if (sMyArgs.Token(uIndex) == "-timer")
		{
			SetAwayTime(sMyArgs.Token(uIndex + 1).ToInt());
			sMyArgs = sMyArgs.Token(uIndex + 2, true);
		}
		if (m_saveMessages)
		{
			if (!sMyArgs.empty())
			{
				m_sPassword = CBlowfish::MD5(sMyArgs);
			} else {
				sMessage = "This module needs as an argument a keyphrase used for encryption";
				return false;
			}

			if (!BootStrap())
			{
				sMessage = "Failed to decrypt your saved messages - "
					"Did you give the right encryption key as an argument to this module?";
				m_bBootError = true;
				return false;
			}
		}

		return true;
	}

	virtual void OnIRCConnected()
	{
		if (m_bIsAway)
			Away(true); // reset away if we are reconnected
		else
			Back(); // ircd seems to remember your away if you killed the client and came back
	}

	bool BootStrap()
	{
		CString sFile;
		if (DecryptMessages(sFile))
		{
			VCString vsLines;
			VCString::iterator it;

			sFile.Split("\n", vsLines);

			for (it = vsLines.begin(); it != vsLines.end(); ++it) {
				CString sLine(*it);
				sLine.Trim();
				AddMessage(sLine);
			}
		} else {
			m_sPassword = "";
			CUtils::PrintError("[" + GetModName() + ".so] Failed to Decrypt Messages");
			return(false);
		}

		return(true);
	}

	void SaveBufferToDisk()
	{
		if (!m_sPassword.empty())
		{
			CString sFile = CRYPT_VERIFICATION_TOKEN;

			for (u_int b = 0; b < m_vMessages.size(); b++)
				sFile += m_vMessages[b] + "\n";

			CBlowfish c(m_sPassword, BF_ENCRYPT);
			sFile = c.Crypt(sFile);
			CString sPath = GetPath();
			if (!sPath.empty())
			{
				CFile File(sPath);
				if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
					File.Chmod(0600);
					File.Write(sFile);
				}
				File.Close();
			}
		}
	}

	virtual void OnClientLogin()
	{
		Back(true);
	}
	virtual void OnClientDisconnect()
	{
		Away();
	}

	virtual void OnModCommand(const CString& sCommand)
	{
		CString sCmdName = sCommand.Token(0);
		if (sCmdName == "away")
		{
			CString sReason;
			if (sCommand.Token(1) != "-quiet")
			{
				sReason = sCommand.Token(1, true);
				PutModNotice("You have been marked as away", "away");
			}
			else
				sReason = sCommand.Token(2, true);
			Away(false, sReason);
		}
		else if (sCmdName == "back")
		{
			if ((m_vMessages.empty()) && (sCommand.Token(1) != "-quiet"))
				PutModNotice("Welcome Back!", "away");
			Back();
		}
		else if (sCmdName == "messages")
		{
			for (u_int a = 0; a < m_vMessages.size(); a++)
				PutModule(m_vMessages[a], "away");
		}
		else if (sCmdName == "delete")
		{
			CString sWhich = sCommand.Token(1);
			if (sWhich == "all")
			{
				PutModNotice("Deleted " + CString(m_vMessages.size()) + " Messages.", "away");
				for (u_int a = 0; a < m_vMessages.size(); a++)
					m_vMessages.erase(m_vMessages.begin() + a--);

			}
			else if (sWhich.empty())
			{
				PutModNotice("USAGE: delete <num|all>", "away");
				return;
			} else
			{
				u_int iNum = sWhich.ToUInt();
				if (iNum >= m_vMessages.size())
				{
					PutModNotice("Illegal Message # Requested", "away");
					return;
				}
				else
				{
					m_vMessages.erase(m_vMessages.begin() + iNum);
					PutModNotice("Message Erased.", "away");
				}
				SaveBufferToDisk();
			}
		}
		else if (sCmdName == "save" && m_saveMessages)
		{
			SaveBufferToDisk();
			PutModNotice("Messages saved to disk.", "away");
		}
		else if (sCmdName == "ping")
		{
			Ping();
			if (m_bIsAway)
				Back();
		}
		else if (sCmdName == "pass")
		{
			m_sPassword = sCommand.Token(1);
			PutModNotice("Password Updated to [" + m_sPassword + "]");
		}
		else if (sCmdName == "show")
		{
			map< CString, vector< CString> > msvOutput;
			for (u_int a = 0; a < m_vMessages.size(); a++)
			{
				CString sTime = m_vMessages[a].Token(0, false, ":");
				CString sWhom = m_vMessages[a].Token(1, false, ":");
				CString sMessage = m_vMessages[a].Token(2, true, ":");

				if ((sTime.empty()) || (sWhom.empty()) || (sMessage.empty()))
				{
					// illegal format
					PutModule("Corrupt message! [" + m_vMessages[a] + "]", "away");
					m_vMessages.erase(m_vMessages.begin() + a--);
					continue;
				}
				time_t iTime = sTime.ToULong();
				char szFormat[64];
				struct tm t;
				localtime_r(&iTime, &t);
				size_t iCount = strftime(szFormat, 64, "%F %T", &t);
				if (iCount <= 0)
				{
					PutModule("Corrupt time stamp! [" + m_vMessages[a] + "]", "away");
					m_vMessages.erase(m_vMessages.begin() + a--);
					continue;
				}
				CString sTmp = "    " + CString(a) + ") [";
				sTmp.append(szFormat, iCount);
				sTmp += "] ";
				sTmp += sMessage;
				msvOutput[sWhom].push_back(sTmp);
			}
			for (map< CString, vector< CString> >::iterator it = msvOutput.begin(); it != msvOutput.end(); ++it)
			{
				PutModule(it->first, "away");
				for (u_int a = 0; a < it->second.size(); a++)
					PutModule(it->second[a]);
			}
			PutModule("#--- End Messages", "away");
		} else if (sCmdName == "enabletimer")
		{
			SetAwayTime(300);
			PutModule("Timer set to 300 seconds");
		} else if (sCmdName == "disabletimer")
		{
			SetAwayTime(0);
			PutModule("Timer disabled");
		} else if (sCmdName == "settimer")
		{
			int iSetting = sCommand.Token(1).ToInt();

			SetAwayTime(iSetting);

			if (iSetting == 0)
				PutModule("Timer disabled");
			else
				PutModule("Timer set to " + CString(iSetting) + " seconds");

		} else if (sCmdName == "timer")
		{
			PutModule("Current timer setting: " + CString(GetAwayTime()) + " seconds");
		} else
		{
			PutModule("Commands: away [-quiet], back [-quiet], delete <num|all>, ping, show, save, enabletimer, disabletimer, settimer <secs>, timer", "away");
		}
	}

	CString GetPath()
	{
		CString sBuffer = m_pUser->GetUserName();
		CString sRet = GetSavePath();
		sRet += "/.znc-away-" + CBlowfish::MD5(sBuffer, true);
		return(sRet);
	}

	virtual void Away(bool bForce = false, const CString & sReason = "")
	{
		if ((!m_bIsAway) || (bForce))
		{
			if (!bForce)
				m_sReason = sReason;
			else if (!sReason.empty())
				m_sReason = sReason;

			time_t iTime = time(NULL);
			char *pTime = ctime(&iTime);
			CString sTime;
			if (pTime)
			{
				sTime = pTime;
				sTime.Trim();
			}
			if (m_sReason.empty())
				m_sReason = "Auto Away at " + sTime;
			PutIRC("AWAY :" + m_sReason);
			m_bIsAway = true;
		}
	}

	virtual void Back(bool bUsePrivMessage = false)
	{
		PutIRC("away");
		m_bIsAway = false;
		if (!m_vMessages.empty())
		{
			if (bUsePrivMessage)
			{
				PutModule("Welcome Back!", "away");
				PutModule("You have " + CString(m_vMessages.size()) + " messages!", "away");
			}
			else
			{
				PutModNotice("Welcome Back!", "away");
				PutModNotice("You have " + CString(m_vMessages.size()) + " messages!", "away");
			}
		}
		m_sReason = "";
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage)
	{
		if (m_bIsAway)
			AddMessage(time(NULL), Nick, sMessage);
		return(CONTINUE);
	}

	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage)
	{
		Ping();
		if (m_bIsAway)
			Back();

		return(CONTINUE);
	}
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage)
	{
		Ping();
		if (m_bIsAway)
			Back();

		return(CONTINUE);
	}

	time_t GetTimeStamp() const { return(m_iLastSentData); }
	void Ping() { m_iLastSentData = time(NULL); }
	time_t GetAwayTime() { return m_iAutoAway; }
	void SetAwayTime(time_t u) { m_iAutoAway = u; }

	bool IsAway() { return(m_bIsAway); }

private:
	CString m_sPassword;
	bool    m_bBootError;
	bool DecryptMessages(CString & sBuffer)
	{
		CString sMessages = GetPath();
		CString sFile;
		sBuffer = "";

		CFile File(sMessages);

		if (sMessages.empty() || !File.Open() || !File.ReadFile(sFile))
		{
			 PutModule("Unable to find buffer");
			 return(true); // gonna be successful here
		}

		File.Close();

		if (!sFile.empty())
		{
			CBlowfish c(m_sPassword, BF_DECRYPT);
			sBuffer = c.Crypt(sFile);

			if (sBuffer.Left(strlen(CRYPT_VERIFICATION_TOKEN)) != CRYPT_VERIFICATION_TOKEN)
			{
				// failed to decode :(
				PutModule("Unable to decode Encrypted messages");
				return(false);
			}
			sBuffer.erase(0, strlen(CRYPT_VERIFICATION_TOKEN));
		}
		return(true);
	}

	void AddMessage(time_t iTime, const CNick & Nick, CString & sMessage)
	{
		if (m_pUser && Nick.GetNick() == m_pUser->GetIRCNick().GetNick())
			return; // ignore messages from self
		AddMessage(CString(iTime) + ":" + Nick.GetNickMask() + ":" + sMessage);
	}

	void AddMessage(const CString & sText)
	{
		if (m_saveMessages)
		{
			m_vMessages.push_back(sText);
		}
	}

	time_t          m_iLastSentData;
	bool            m_bIsAway;
	time_t          m_iAutoAway;
	vector<CString> m_vMessages;
	CString         m_sReason;
	bool            m_saveMessages;
};


void CAwayJob::RunJob()
{
	CAway *p = (CAway *)m_pModule;
	p->SaveBufferToDisk();

	if (!p->IsAway())
	{
		time_t iNow = time(NULL);

		if ((iNow - p->GetTimeStamp()) > p->GetAwayTime() && p->GetAwayTime() != 0)
			p->Away();
	}
}

MODULEDEFS(CAway, "Stores messages while away, also auto away")

