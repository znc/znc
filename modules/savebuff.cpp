/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Buffer Saving thing, incase your shit goes out while your out
 * Author: imaginos <imaginos@imaginos.net>
 *
 * Its only as secure as your shell, the encryption only offers a slightly
 * better solution then plain text.
 */

#define REQUIRESSL

#include "Chan.h"
#include "User.h"
#include <sys/stat.h>

/* TODO list
 * store timestamp to be displayed
 * store OnJoin, OnQuit, OnPart, etc send down as messages
 */

#define CRYPT_VERIFICATION_TOKEN "::__:SAVEBUFF:__::"

class CSaveBuff;

class CSaveBuffJob : public CTimer
{
public:
	CSaveBuffJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CSaveBuffJob() {}

protected:
	virtual void RunJob();
};

class CSaveBuff : public CModule
{
public:
	MODCONSTRUCTOR(CSaveBuff)
	{
		m_bBootError = false;
		// m_sPassword = CBlowfish::MD5("");
		AddTimer(new CSaveBuffJob(this, 60, 0, "SaveBuff", "Saves the current buffer to disk every 1 minute"));
	}
	virtual ~CSaveBuff()
	{
		if (!m_bBootError)
		{
			SaveBufferToDisk();
		}
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		if (!sArgs.empty())
		{
			m_sPassword = CBlowfish::MD5(sArgs);
			return(OnBoot());
		}

		return true;
	}

	virtual bool OnBoot()
	{
		if (m_sPassword.empty())
		{
			char *pTmp = CUtils::GetPass("Enter Encryption Key for " + GetModName() + ".so");

			if (pTmp)
				m_sPassword = CBlowfish::MD5(pTmp);

			*pTmp = 0;
		}

		const vector<CChan *>& vChans = m_pUser->GetChans();
		for(u_int a = 0; a < vChans.size(); a++)
		{
			if (!vChans[a]->KeepBuffer())
				continue;

			if (!BootStrap(vChans[a]))
			{
				m_bBootError = true;
				return(false);
			}
		}

		return true;
	}

	bool BootStrap(CChan *pChan)
	{
		CString sFile;
		if (DecryptChannel(pChan->GetName(), sFile))
		{
			if (!pChan->GetBuffer().empty())
				return(true); // reloaded a module probably in this case, so just verify we can decrypt the file

			CString sLine;
			CString::size_type iPos = 0;
			while(ReadLine(sFile, sLine, iPos))
			{
				sLine.Trim();
				pChan->AddBuffer(sLine);
			}
		} else
		{
			m_sPassword = "";
			CUtils::PrintError("[" + GetModName() + ".so] Failed to Decrypt [" + pChan->GetName() + "]");
			return(false);
		}

		return(true);
	}

	void SaveBufferToDisk()
	{
		if (!m_sPassword.empty())
		{
			const vector<CChan *>& vChans = m_pUser->GetChans();
			for(u_int a = 0; a < vChans.size(); a++)
			{
				if (!vChans[a]->KeepBuffer())
					continue;

				const vector<CString> & vBuffer = vChans[a]->GetBuffer();

				if (vBuffer.empty())
				{
					if (!m_sPassword.empty())
						BootStrap(vChans[a]);

					continue;
				}

				CString sFile = CRYPT_VERIFICATION_TOKEN;

				for(u_int b = 0; b < vBuffer.size(); b++)
						sFile += vBuffer[b] + "\n";

				CBlowfish c(m_sPassword, BF_ENCRYPT);
				sFile = c.Crypt(sFile);
				CString sPath = GetPath(vChans[a]->GetName());
				if (!sPath.empty())
				{
					WriteFile(sPath, sFile);
					chmod(sPath.c_str(), 0600);
				}
			}
		}
	}

	virtual void OnModCommand(const CString& sCommand)
	{
		CString::size_type iPos = sCommand.find(" ");
		CString sCom, sArgs;
		if (iPos == CString::npos)
			sCom = sCommand;
		else
		{
			sCom = sCommand.substr(0, iPos);
			sArgs = sCommand.substr(iPos + 1, CString::npos);
		}

		if (strcasecmp(sCom.c_str(), "setpass") == 0)
		{
			PutModule("Password set to [" + sArgs + "]");
			m_sPassword = CBlowfish::MD5(sArgs);

		} else if (strcasecmp(sCom.c_str(), "dumpbuff") == 0)
		{
			CString sFile;
			if (DecryptChannel(sArgs, sFile))
			{
				CString sLine;
				iPos = 0;
				while(ReadLine(sFile, sLine, iPos))
				{
					sLine.Trim();
					PutModule("[" + sLine + "]");
				}
			}
			PutModule("//!-- EOF " + sArgs);
		} else if (strcasecmp(sCom.c_str(), "replay") == 0)
		{
			Replay(sArgs);
			PutModule("Replayed " + sArgs);

		} else if (strcasecmp(sCom.c_str(), "save") == 0)
		{
			SaveBufferToDisk();
			PutModule("Done.");
		} else
			PutModule("Unknown command [" + sCommand + "]");
	}

	void Replay(const CString & sChan)
	{
		CString sFile;
		PutUser(":***!znc@znc.com PRIVMSG " + sChan + " :Buffer Playback...");
		if (DecryptChannel(sChan, sFile))
		{
			CString sLine;
			CString::size_type iPos = 0;
			while(ReadLine(sFile, sLine, iPos))
			{
				sLine.Trim();
				PutUser(sLine);
			}
		}
		PutUser(":***!znc@znc.com PRIVMSG " + sChan + " :Playback Complete.");
	}

	CString GetPath(const CString & sChannel)
	{
		CString sBuffer = m_pUser->GetUserName() + Lower(sChannel);
		CString sRet = GetSavePath();
		sRet += "/" + CBlowfish::MD5(sBuffer, true);
		return(sRet);
	}

	CString SpoofChanMsg(const CString & sChannel, const CString & sMesg)
	{
		CString sReturn = ":*" + GetModName() + "!znc@znc.com PRIVMSG " + sChannel + " :" + CString(time(NULL)) + " " + sMesg;
		return(sReturn);
	}

	virtual void OnRawMode(const CNick& cOpNick, CChan& cChannel, const CString& sModes, const CString& sArgs)
	{
		if (!cChannel.KeepBuffer())
			return;

		((CChan &)cChannel).AddBuffer(SpoofChanMsg(cChannel.GetName(), cOpNick.GetNickMask() + " MODE " + sModes + " " + sArgs));
	}
	virtual void OnQuit(const CNick& cNick, const CString& sMessage, const vector<CChan*>& vChans)
	{
		for(u_int a = 0; a < vChans.size(); a++)
		{
			if (!vChans[a]->KeepBuffer())
				continue;
			vChans[a]->AddBuffer(SpoofChanMsg(vChans[a]->GetName(), cNick.GetNickMask() + " QUIT " + sMessage));
		}
		if (cNick.GetNick().CaseCmp(m_pUser->GetNick()) == 0)
			SaveBufferToDisk(); // need to force a save here to see this!
	}

	virtual void OnNick(const CNick& cNick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		for(u_int a = 0; a < vChans.size(); a++)
		{
			if (!vChans[a]->KeepBuffer())
				continue;
			vChans[a]->AddBuffer(SpoofChanMsg(vChans[a]->GetName(), cNick.GetNickMask() + " NICK " + sNewNick));
		}
	}
	virtual void OnKick(const CNick& cNick, const CString& sOpNick, CChan& cChannel, const CString& sMessage)
	{
		if (!cChannel.KeepBuffer())
			return;
		((CChan &)cChannel).AddBuffer(SpoofChanMsg(cChannel.GetName(), sOpNick + " KICK " + cNick.GetNickMask() + " " + sMessage));
	}
	virtual void OnJoin(const CNick& cNick, CChan& cChannel)
	{
		if ((cNick.GetNick().CaseCmp(m_pUser->GetNick()) == 0) && (cChannel.GetBuffer().empty()))
		{
			BootStrap((CChan *)&cChannel);
			if (!cChannel.GetBuffer().empty())
				Replay(cChannel.GetName());
		}
		if (!cChannel.KeepBuffer())
			return;
		((CChan &)cChannel).AddBuffer(SpoofChanMsg(cChannel.GetName(), cNick.GetNickMask() + " JOIN"));
	}
	virtual void OnPart(const CNick& cNick, CChan& cChannel)
	{
		if (!cChannel.KeepBuffer())
			return;
		((CChan &)cChannel).AddBuffer(SpoofChanMsg(cChannel.GetName(), cNick.GetNickMask() + " PART"));
		if (cNick.GetNick().CaseCmp(m_pUser->GetNick()) == 0)
			SaveBufferToDisk(); // need to force a save here to see this!
	}

private:
	bool	m_bBootError;
	CString	m_sPassword;
	bool DecryptChannel(const CString & sChan, CString & sBuffer)
	{
		CString sChannel = GetPath(sChan);
		CString sFile;
		sBuffer = "";

		if ((sChannel.empty()) || (!ReadFile(sChannel, sFile)))
			 return(true); // gonna be successful here

		if (!sFile.empty())
		{
			CBlowfish c(m_sPassword, BF_DECRYPT);
			sBuffer = c.Crypt(sFile);

			if (sBuffer.substr(0, strlen(CRYPT_VERIFICATION_TOKEN)) != CRYPT_VERIFICATION_TOKEN)
			{
				// failed to decode :(
				PutModule("Unable to decode Encrypted file [" + sChannel + "]");
				return(false);
			}
			sBuffer.erase(0, strlen(CRYPT_VERIFICATION_TOKEN));
		}
		return(true);
	}
};


void CSaveBuffJob::RunJob()
{
	CSaveBuff *p = (CSaveBuff *)m_pModule;
	p->SaveBufferToDisk();
}

MODULEDEFS(CSaveBuff, "Stores channel buffers to disk, encrypted")

