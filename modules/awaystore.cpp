/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
 * Author: imaginos <imaginos@imaginos.net>
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

/*
 * Quiet Away and message logger
 *
 * I originally wrote this module for when I had multiple clients connected to ZNC. I would leave work and forget to close my client, arriving at home
 * and re-attaching there someone may have messaged me in commute and I wouldn't know it until I would arrive back at work the next day. I wrote it such that
 * my xchat client would monitor desktop activity and ping the module to let it know I was active. Within a few minutes of inactivity the pinging stops and
 * the away module sets the user as away and logging commences.
 */

#define REQUIRESSL

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/FileUtils.h>

using std::vector;
using std::map;

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
	void AwayCommand(const CString& sCommand) {
		CString sReason;

		if (sCommand.Token(1) != "-quiet") {
			sReason = sCommand.Token(1, true);
			PutModNotice("You have been marked as away");
		} else {
			sReason = sCommand.Token(2, true);
		}

		Away(false, sReason);
	}

	void BackCommand(const CString& sCommand) {
		if ((m_vMessages.empty()) && (sCommand.Token(1) != "-quiet"))
			PutModNotice("Welcome Back!");
		Ping();
		Back();
	}

	void MessagesCommand(const CString& sCommand) {
		for (u_int a = 0; a < m_vMessages.size(); a++)
			PutModule(m_vMessages[a]);
	}

	void ReplayCommand(const CString& sCommand) {
		CString nick = GetClient()->GetNick();
		for (u_int a = 0; a < m_vMessages.size(); a++) {
			CString sWhom = m_vMessages[a].Token(1, false, ":");
			CString sMessage = m_vMessages[a].Token(2, true, ":");
			PutUser(":" + sWhom + " PRIVMSG " + nick + " :" + sMessage);
		}
	}

	void DeleteCommand(const CString& sCommand) {
		CString sWhich = sCommand.Token(1);
		if (sWhich == "all") {
			PutModNotice("Deleted " + CString(m_vMessages.size()) + " Messages.");
			for (u_int a = 0; a < m_vMessages.size(); a++)
				m_vMessages.erase(m_vMessages.begin() + a--);
		} else if (sWhich.empty()) {
			PutModNotice("USAGE: delete <num|all>");
			return;
		} else {
			u_int iNum = sWhich.ToUInt();
			if (iNum >= m_vMessages.size()) {
				PutModNotice("Illegal Message # Requested");
				return;
			} else {
				m_vMessages.erase(m_vMessages.begin() + iNum);
				PutModNotice("Message Erased.");
			}
			SaveBufferToDisk();
		}
	}

	void SaveCommand(const CString& sCommand) {
		if (m_saveMessages) {
			SaveBufferToDisk();
			PutModNotice("Messages saved to disk.");
		} else {
			PutModNotice("There are no messages to save.");
		}
	}

	void PingCommand(const CString& sCommand) {
		Ping();
		if (m_bIsAway)
			Back();
	}

	void PassCommand(const CString& sCommand) {
		m_sPassword = sCommand.Token(1);
		PutModNotice("Password Updated to [" + m_sPassword + "]");
	}

	void ShowCommand(const CString& sCommand) {
		map< CString, vector< CString> > msvOutput;
		for (u_int a = 0; a < m_vMessages.size(); a++) {
			CString sTime = m_vMessages[a].Token(0, false);
			CString sWhom = m_vMessages[a].Token(1, false);
			CString sMessage = m_vMessages[a].Token(2, true);

			if ((sTime.empty()) || (sWhom.empty()) || (sMessage.empty())) {
				// illegal format
				PutModule("Corrupt message! [" + m_vMessages[a] + "]");
				m_vMessages.erase(m_vMessages.begin() + a--);
				continue;
			}

			time_t iTime = sTime.ToULong();
			char szFormat[64];
			struct tm t;
			localtime_r(&iTime, &t);
			size_t iCount = strftime(szFormat, 64, "%F %T", &t);

			if (iCount <= 0) {
				PutModule("Corrupt time stamp! [" + m_vMessages[a] + "]");
				m_vMessages.erase(m_vMessages.begin() + a--);
				continue;
			}

			CString sTmp = "    " + CString(a) + ") [";
			sTmp.append(szFormat, iCount);
			sTmp += "] ";
			sTmp += sMessage;
			msvOutput[sWhom].push_back(sTmp);
		}

		for (map< CString, vector< CString> >::iterator it = msvOutput.begin(); it != msvOutput.end(); ++it) {
			PutModule(it->first);
			for (u_int a = 0; a < it->second.size(); a++)
				PutModule(it->second[a]);
		}

		PutModule("#--- End Messages");
	}

	void EnableTimerCommand(const CString& sCommand) {
		SetAwayTime(300);
		PutModule("Timer set to 300 seconds");
	}

	void DisableTimerCommand(const CString& sCommand) {
		SetAwayTime(0);
		PutModule("Timer disabled");
	}

	void SetTimerCommand(const CString& sCommand) {
		int iSetting = sCommand.Token(1).ToInt();

		SetAwayTime(iSetting);

		if (iSetting == 0)
			PutModule("Timer disabled");
		else
			PutModule("Timer set to " + CString(iSetting) + " seconds");
	}

	void TimerCommand(const CString& sCommand) {
		PutModule("Current timer setting: " + CString(GetAwayTime()) + " seconds");
	}

public:
	MODCONSTRUCTOR(CAway)
	{
		Ping();
		m_bIsAway = false;
		m_bBootError = false;
		m_saveMessages = true;
		SetAwayTime(300);
		AddTimer(new CAwayJob(this, 60, 0, "AwayJob", "Checks for idle and saves messages every 1 minute"));

		AddHelpCommand();
		AddCommand("Away",         static_cast<CModCommand::ModCmdFunc>(&CAway::AwayCommand),
			"[-quiet]");
		AddCommand("Back",         static_cast<CModCommand::ModCmdFunc>(&CAway::BackCommand),
			"[-quiet]");
		AddCommand("Messages",     static_cast<CModCommand::ModCmdFunc>(&CAway::BackCommand));
		AddCommand("Delete",       static_cast<CModCommand::ModCmdFunc>(&CAway::DeleteCommand),
			"delete <num|all>");
		AddCommand("Save",         static_cast<CModCommand::ModCmdFunc>(&CAway::SaveCommand));
		AddCommand("Ping",         static_cast<CModCommand::ModCmdFunc>(&CAway::PingCommand));
		AddCommand("Pass",         static_cast<CModCommand::ModCmdFunc>(&CAway::PassCommand));
		AddCommand("Show",         static_cast<CModCommand::ModCmdFunc>(&CAway::ShowCommand));
		AddCommand("Replay",       static_cast<CModCommand::ModCmdFunc>(&CAway::ReplayCommand));
		AddCommand("EnableTimer",  static_cast<CModCommand::ModCmdFunc>(&CAway::EnableTimerCommand));
		AddCommand("DisableTimer", static_cast<CModCommand::ModCmdFunc>(&CAway::DisableTimerCommand));
		AddCommand("SetTimer",     static_cast<CModCommand::ModCmdFunc>(&CAway::SetTimerCommand),
			"<secs>");
		AddCommand("Timer",        static_cast<CModCommand::ModCmdFunc>(&CAway::TimerCommand));

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
				PutModule("Welcome Back!");
				PutModule("You have " + CString(m_vMessages.size()) + " messages!");
			}
			else
			{
				PutModNotice("Welcome Back!");
				PutModNotice("You have " + CString(m_vMessages.size()) + " messages!");
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
		if (Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick())
			return; // ignore messages from self
		AddMessage(CString(iTime) + " " + Nick.GetNickMask() + " " + sMessage);
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

template<> void TModInfo<CAway>(CModInfo& Info) {
	Info.SetWikiPage("awaystore");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("[ -notimer | -timer N ]  passw0rd . N is number of seconds, 600 by default.");
}

NETWORKMODULEDEFS(CAway, "Adds auto-away with logging, useful when you use ZNC from different locations");

