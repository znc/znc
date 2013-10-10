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
 * Buffer Saving thing, incase your shit goes out while your out
 *
 * Its only as secure as your shell, the encryption only offers a slightly
 * better solution then plain text.
 */

#define REQUIRESSL

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/FileUtils.h>

using std::vector;

#define CRYPT_VERIFICATION_TOKEN "::__:SAVEBUFF:__::"
// this is basically plain text, but so is having the pass in the command line so *shrug*
// you could at least do something kind of cool like a bunch of unprintable text
#define CRYPT_LAME_PASS "::__:NOPASS:__::"
#define CRYPT_ASK_PASS "--ask-pass"

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
		m_bFirstLoad = false;
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
		if( sArgs == CRYPT_ASK_PASS )
		{
			char *pPass = getpass( "Enter pass for savebuff: " );
			if( pPass )
				m_sPassword = CBlowfish::MD5( pPass );
			else
			{
				m_bBootError = true;
				sMessage = "Nothing retrieved from console. aborting";
			}
		}
		else if( sArgs.empty() )
			m_sPassword = CBlowfish::MD5( CRYPT_LAME_PASS );
		else
			m_sPassword = CBlowfish::MD5(sArgs);

		return( !m_bBootError );
	}

	virtual void OnIRCConnected()
	{
		// dropped this into here because there seems to have been a changed where the module is loaded before the channels.
		// this is a good trigger to tell it to backfill the channels
		if( !m_bFirstLoad )
		{
			m_bFirstLoad = true;
			AddTimer(new CSaveBuffJob(this, 60, 0, "SaveBuff", "Saves the current buffer to disk every 1 minute"));
			const vector<CChan *>& vChans = m_pNetwork->GetChans();
			for (u_int a = 0; a < vChans.size(); a++)
			{
				if (vChans[a]->AutoClearChanBuffer())
					continue;

				if (!BootStrap(vChans[a]))
				{
					PutUser(":***!znc@znc.in PRIVMSG " + vChans[a]->GetName() + " :Failed to decrypt this channel, did you change the encryption pass?");
				}
			}
		}
	}

	bool BootStrap(CChan *pChan)
	{
		CString sFile;
		if (DecryptChannel(pChan->GetName(), sFile))
		{
			if (!pChan->GetBuffer().IsEmpty())
				return(true); // reloaded a module probably in this case, so just verify we can decrypt the file

			VCString vsLines;
			VCString::iterator it;

			sFile.Split("\n", vsLines);

			for (it = vsLines.begin(); it != vsLines.end(); ++it) {
				CString sLine(*it);
				sLine.Trim();
				if (sLine[0] == '@' && it+1 != vsLines.end())
				{
					CString sTimestamp = sLine.Token(0);
					sTimestamp.TrimLeft("@");
					timeval ts;
					ts.tv_sec = sTimestamp.Token(0, false, ",").ToLongLong();
					ts.tv_usec = sTimestamp.Token(1, false, ",").ToLong();

					CString sFormat = sLine.Token(1, true);

					CString sText(*++it);
					sText.Trim();

					pChan->AddBuffer(sFormat, sText, &ts);
				} else
				{
					// Old format, escape the line and use as is.
					pChan->AddBuffer(_NAMEDFMT(sLine));
				}
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
			const vector<CChan *>& vChans = m_pNetwork->GetChans();
			for (u_int a = 0; a < vChans.size(); a++)
			{
				CString sPath = GetPath(vChans[a]->GetName());
				CFile File(sPath);

				if (vChans[a]->AutoClearChanBuffer()) {
					File.Delete();
					continue;
				}

				const CBuffer& Buffer = vChans[a]->GetBuffer();
				CString sLine;

				CString sFile = CRYPT_VERIFICATION_TOKEN;

				size_t uSize = Buffer.Size();
				for (unsigned int uIdx = 0; uIdx < uSize; uIdx++) {
					const CBufLine& Line = Buffer.GetBufLine(uIdx);
					timeval ts = Line.GetTime();
					sFile +=
						"@" + CString(ts.tv_sec) + "," + CString(ts.tv_usec) + " " +
						Line.GetFormat() + "\n" +
						Line.GetText() + "\n";
				}

				CBlowfish c(m_sPassword, BF_ENCRYPT);
				sFile = c.Crypt(sFile);
				if (!sPath.empty())
				{
					if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
						File.Chmod(0600);
						File.Write(sFile);
					}
					File.Close();
				}
			}
		}
		else
		{
			PutModule( "Password is unset usually meaning the decryption failed. You can setpass to the appropriate pass and things should start working, or setpass to a new pass and save to reinstantiate" );
		}
	}

	virtual void OnModCommand(const CString& sCmdLine)
	{
		CString sCommand = sCmdLine.Token(0);
		CString sArgs    = sCmdLine.Token(1, true);

		if (sCommand.Equals("setpass"))
		{
			PutModule("Password set to [" + sArgs + "]");
			m_sPassword = CBlowfish::MD5(sArgs);

		} else if (sCommand.Equals("dumpbuff"))
		{
			CString sFile;
			if (DecryptChannel(sArgs, sFile))
			{
				VCString vsLines;
				VCString::iterator it;

				sFile.Split("\n", vsLines);

				for (it = vsLines.begin(); it != vsLines.end(); ++it) {
					CString sLine(*it);
					sLine.Trim();
					PutModule("[" + sLine + "]");
				}
			}
			PutModule("//!-- EOF " + sArgs);
		} else if (sCommand.Equals("replay"))
		{
			Replay(sArgs);
			PutModule("Replayed " + sArgs);

		} else if (sCommand.Equals("save"))
		{
			SaveBufferToDisk();
			PutModule("Done.");
		} else
			PutModule("Unknown command [" + sCommand + "]");
	}

	void Replay(const CString & sChan)
	{
		CString sFile;
		PutUser(":***!znc@znc.in PRIVMSG " + sChan + " :Buffer Playback...");
		if (DecryptChannel(sChan, sFile))
		{
			VCString vsLines;
			VCString::iterator it;

			sFile.Split("\n", vsLines);

			for (it = vsLines.begin(); it != vsLines.end(); ++it) {
				CString sLine(*it);
				sLine.Trim();
				PutUser(sLine);
			}
		}
		PutUser(":***!znc@znc.in PRIVMSG " + sChan + " :Playback Complete.");
	}

	CString GetPath(const CString & sChannel)
	{
		CString sBuffer = m_pUser->GetUserName() + sChannel.AsLower();
		CString sRet = GetSavePath();
		sRet += "/" + CBlowfish::MD5(sBuffer, true);
		return(sRet);
	}

#ifdef LEGACY_SAVEBUFF /* event logging is deprecated now in savebuf. Use buffextras module along side of this */
	CString SpoofChanMsg(const CString & sChannel, const CString & sMesg)
	{
		CString sReturn = ":*" + GetModName() + "!znc@znc.in PRIVMSG " + sChannel + " :" + CString(time(NULL)) + " " + sMesg;
		return(sReturn);
	}

	void AddBuffer(CChan& chan, const CString &sLine)
	{
		// If they have AutoClearChanBuffer enabled, only add messages if no client is connected
		if (chan.AutoClearChanBuffer() && m_pNetwork->IsUserAttached())
			return;
		chan.AddBuffer(sLine);
	}

	virtual void OnRawMode(const CNick& cOpNick, CChan& cChannel, const CString& sModes, const CString& sArgs)
	{
		AddBuffer(cChannel, SpoofChanMsg(cChannel.GetName(), cOpNick.GetNickMask() + " MODE " + sModes + " " + sArgs));
	}
	virtual void OnQuit(const CNick& cNick, const CString& sMessage, const vector<CChan*>& vChans)
	{
		for (size_t a = 0; a < vChans.size(); a++)
		{
			AddBuffer(*vChans[a], SpoofChanMsg(vChans[a]->GetName(), cNick.GetNickMask() + " QUIT " + sMessage));
		}
		if (cNick.NickEquals(m_pUser->GetNick()))
			SaveBufferToDisk(); // need to force a save here to see this!
	}

	virtual void OnNick(const CNick& cNick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		for (size_t a = 0; a < vChans.size(); a++)
		{
			AddBuffer(*vChans[a], SpoofChanMsg(vChans[a]->GetName(), cNick.GetNickMask() + " NICK " + sNewNick));
		}
	}
	virtual void OnKick(const CNick& cNick, const CString& sOpNick, CChan& cChannel, const CString& sMessage)
	{
		AddBuffer(cChannel, SpoofChanMsg(cChannel.GetName(), sOpNick + " KICK " + cNick.GetNickMask() + " " + sMessage));
	}
	virtual void OnJoin(const CNick& cNick, CChan& cChannel)
	{
		if (cNick.NickEquals(m_pUser->GetNick()) && cChannel.GetBuffer().empty())
		{
			BootStrap((CChan *)&cChannel);
			if (!cChannel.GetBuffer().empty())
				Replay(cChannel.GetName());
		}
		AddBuffer(cChannel, SpoofChanMsg(cChannel.GetName(), cNick.GetNickMask() + " JOIN"));
	}
	virtual void OnPart(const CNick& cNick, CChan& cChannel)
	{
		AddBuffer(cChannel, SpoofChanMsg(cChannel.GetName(), cNick.GetNickMask() + " PART"));
		if (cNick.NickEquals(m_pUser->GetNick()))
			SaveBufferToDisk(); // need to force a save here to see this!
	}
#endif /* LEGACY_SAVEBUFF */

private:
	bool    m_bBootError;
	bool    m_bFirstLoad;
	CString m_sPassword;
	bool DecryptChannel(const CString & sChan, CString & sBuffer)
	{
		CString sChannel = GetPath(sChan);
		CString sFile;
		sBuffer = "";

		CFile File(sChannel);

		if (sChannel.empty() || !File.Open() || !File.ReadFile(sFile))
			 return(true); // gonna be successful here

		File.Close();

		if (!sFile.empty())
		{
			CBlowfish c(m_sPassword, BF_DECRYPT);
			sBuffer = c.Crypt(sFile);

			if (sBuffer.Left(strlen(CRYPT_VERIFICATION_TOKEN)) != CRYPT_VERIFICATION_TOKEN)
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

template<> void TModInfo<CSaveBuff>(CModInfo& Info) {
	Info.SetWikiPage("savebuff");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("This user module takes up to one arguments. Either --ask-pass or the password itself (which may contain spaces) or nothing");
}

NETWORKMODULEDEFS(CSaveBuff, "Stores channel buffers to disk, encrypted")

