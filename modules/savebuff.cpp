/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Buffer Saving thing, incase your shit goes out while your out
 * Author: imaginos <imaginos@imaginos.net>
 */

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/Buffer.h>
#include <znc/Server.h>
#include <znc/IRCNetwork.h>
#include <znc/FileUtils.h>
#include <algorithm> // min
#include <cctype> // isdigit
#include <time.h> // localtime_r, strftime

// The default maximum number of lines saved in channel buffers.
#define SAVE_LIMIT 1000

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
		m_bSeenJoin=false;
		m_bBootError = false;
		m_uMaxLine = SAVE_LIMIT;
		m_pTimer = NULL;

		AddHelpCommand();
		AddCommand("args", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Args),
				"", "Shows the list of " + GetModName() + " module arguments.");
		AddCommand("dump", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Dump),
				"<channel>", "Dumps a channel buffer in " + GetModName() + " module.");
		AddCommand("del", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Del),
				"<channel>", "Delete channel(s) with wildcards * and ?");
		AddCommand("count", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Count),
				"<channel> [<channel> ...]", "Counts the number of lines in channels with wildcards * and ?");
		AddCommand("save", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Save),
				"", "Saves all channel buffers.");
		AddCommand("list", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::List),
				"", "Lists all of saved channel buffers.");
		AddCommand("maxline", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Maxline),
				"", "Prints the maximum number of lines saved in a channel buffer.");
		AddCommand("timers", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Timers),
				"", "Lists the timers associated with " + GetModName());
		AddCommand("chanprefix", static_cast<CModCommand::ModCmdFunc>(&CSaveBuff::Chanprefix),
				"", "Shows the list of allowed channel prefixes.");
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
		if(!sArgs.empty())
		{
			if(sArgs.find(" ") != string::npos) // sArgs musn't contain spaces.
			{
				m_bBootError=true;
				sMessage = GetModName() + " takes only one argument.";
				return false;
			}

			// Check if there is any non-digit character in sArgs
			size_t size=sArgs.size();
			for(size_t s=0; s<size; ++s)
			{
				if(!isdigit(sArgs[s]))
				{
					m_bBootError=true;
					sMessage=GetModName() + " doesn't accept non-numeric characters in the argument.";
					return false;
				}
			}

			m_uMaxLine=sArgs.ToUInt();
		}


		// If the module was loaded with ZNC connected to a network,
		// add a timer that saves channel buffers every minute.
		if(GetNetwork()->IsIRCConnected())
			return StartTimer();

		return true;
	}

	virtual void OnIRCConnected()
	{
		StartTimer();
	}

	// Things to do when ZNC disconnects from IRC servers.
	virtual void OnIRCDisconnected()
	{
		// Delete the timer that saves all the buffers every 1 minute
		StopTimer();
		SaveBufferToDisk();
	}

	// It outputs the refilled channel buffer 
	// after znc joins a channel and
	// after the IRC client becomes ready to accept the backlog data.
	virtual EModRet OnRaw(CString &sLine)
	{
		CString sCmd=sLine.Token(1);

		// parse JOIN messages.
		if(sCmd.Equals("JOIN"))
		{
			// JOIN message format
			// ':nick!user@host JOIN [:]channel'
			
			// Retrieve the channel name.
			CString sChan=sLine.Token(2);
			// If it contains the optional ":", remove it
			if(sChan.Left(1) == ":")
				sChan.LeftChomp();
			// Retrieve nickname.
			CString sNick=sLine.Token(0, false, "!");
			// Delete ":" at the beginning.
			sNick.LeftChomp();

			if(sNick.Equals(GetNetwork()->GetCurNick()))
			{
				m_bSeenJoin=true;
				m_sSavedChannel=sChan;
			}
		}
		else if(m_bSeenJoin && sCmd.Equals("366"))
		{
		// 366 denotes the end of /NAMES list after which IRC clients can receive
		// chat data. 366 also means 'the end of JOIN'.
		// 366 should match right after JOIN is seen since /NAMES can be invoked
		// directly by users.

			CChan *pChan = GetNetwork()->FindChan(m_sSavedChannel);
			if(pChan == NULL)
			{
				CUtils::PrintError("["+GetModName()+".so] failed to gain access to the channel ["
						+m_sSavedChannel+"]");
				m_bSeenJoin=false;
				return CONTINUE;
			}

			// 366 message format :
			// prefix 366 nick channel :End of /NAMES list.
			// token 3 is the channel in 366 message.
			CString sChan=sLine.Token(3);
			if(sChan != m_sSavedChannel)
			{
				CUtils::PrintError("["+GetModName()+".so] 366 was received for ["
						+sChan+"] while "+GetModName()+" was expecting it from ["
						+ m_sSavedChannel+"]");
				return CONTINUE;
			}

			// Replay the channel buffer on the channel.
			if(!pChan->GetBuffer().IsEmpty())
				ReplayChannel(pChan);
			// Reset seen_join to 0 after the cycle.
			m_bSeenJoin=false;
		}

		return CONTINUE;
	}

	virtual void OnJoin(const CNick &cNick, CChan& cChan)
	{
		if(cNick.GetNick().Equals(GetNetwork()->GetCurNick()) && cChan.GetBuffer().IsEmpty())
			if(!BootStrap(cChan))
				CUtils::PrintMessage("["+GetModName()+".so] BootStrap not successfull in OnJoin.");
	}

	virtual void OnPart(const CNick& cNick, CChan &cChan, const CString &sMsg)
	{
		if(cNick.GetNick().Equals(GetNetwork()->GetCurNick()))
			SaveBufferToDisk();
	}

	unsigned int SaveBufferToDisk()
	{
		bool bRet=true;
		unsigned int count=0;

		// Save each channel buffer to disk
		CIRCNetwork* pNetwork=GetNetwork();
		if(pNetwork == NULL)
		{
			CUtils::PrintError("["+GetModName()+".so] not associated with any IRC network, "+
					"and it can't save any channel buffer.");
			return 0;
		}

		const vector<CChan *>& vChans = pNetwork->GetChans();
		vector<CChan*>::const_iterator it;
		for (it=vChans.begin(); it != vChans.end(); ++it)
		{
			CChan &cChan=**it;
			if(!SaveChannelToDisk(cChan))
			{
				CUtils::PrintMessage("["+GetModName()+".so] failed to save the channel buffer for ["
						+cChan.GetName()+"]");
				bRet=false;
			}
			else
				count+=1;
		}

		return count;
	}

protected:
	void Args(const CString &sArgs)
	{
		CTable Table;

		PutModule(GetModName()+" Argument [Argument ...]");
		Table.AddColumn("Argument");
		Table.AddColumn("Default");
		Table.AddColumn("Description");

		Table.AddRow();
		Table.SetCell("Argument", "maxline");
		Table.SetCell("Default", "1000");
		Table.SetCell("Description", "The mamximum number of lines saved in a channel buffer.");

		PutModule(Table);
	}

	void Save(const CString &sArgs)
	{
		unsigned int count=SaveBufferToDisk();
		PutModule(CString(count)+" channel buffer(s) saved in "+GetSavePath());
	}

	void List(const CString &sArgs)
	{
		MCString mcString;
		MCString::iterator it;
		// Fill the map with channel buffers.
		if(!GetBufferList("*", mcString) || mcString.empty())
			return;

		CTable table;
		table.AddColumn("Channel");
		table.AddColumn("Size in bytes");
		// Fill the table with the map.
		for(it=mcString.begin(); it!=mcString.end(); ++it)
		{
			CFile File(it->second);
			table.AddRow();
			table.SetCell("Channel", "'"+it->first+"'");
			table.SetCell("Size in bytes", CString(File.GetSize()) );
		}

		PutModule(table);
	}

	void Del(const CString &sLine)
	{
		CString sArgs=sLine.Token(1, true);
		MCString mcChans;
		MCString::iterator it;

		if(sArgs.empty() || sArgs.find(" ") != CString::npos)
		{
			HandleHelpCommand("help del");
			return;
		}
		// Fill the map with channel buffers
		if(!GetBufferList(sArgs, mcChans))
			return;
		// Delete channel buffers.
		unsigned int count=0;
		for(it=mcChans.begin(); it != mcChans.end(); ++it)
		{
			const CString &ePath=it->second;
			if(!CFile::Delete(ePath))
				CUtils::PrintMessage("["+GetModName()+".so] failed to delete "+ePath);
			else
				count+=1;
		}
		PutModule(CString(count)+" channel buffers deleted.");
	}

	void Maxline(const CString &sArgs)
	{
		PutModule("The mamximum number of lines saved in a channel buffer : "+CString(m_uMaxLine));
	}

	void Chanprefix(const CString &sArgs)
	{
		CIRCNetwork *pNet = GetNetwork();
		if(pNet == NULL)
		{
			CUtils::PrintError("["+GetModName() +".so] not associated with any network.");
			return;
		}

		const CString &sServer=pNet->GetCurrentServer()->GetName();
		const CString &sPrefix=pNet->GetChanPrefixes();
		if(sPrefix.empty())
			PutModule(sServer+" doesn't have a list of allowed channel prefixes.");
		else
			PutModule("Channel prefixes allowed at "+sServer+" : "+sPrefix);
	}
	// Counte the number of lines in channel buffers.
	void Count(const CString &sLine)
	{
		CString sBuf;
		MCString msChan;
		MCString::iterator it;
		CString sCmd=sLine.Token(0);
		CString sArgs=sLine.Token(1, true);

		if(sArgs.empty())
		{
			HandleHelpCommand("help count");
			return;
		}

		CTable table;
		table.AddColumn("Channel");
		table.AddColumn("Lines");

		VCString vcString;
		VCString::iterator vcit;
		sArgs.Split(" ", vcString);
		// Each argument fills the map with channel buffers. 
		for(vcit=vcString.begin(); vcit != vcString.end(); ++vcit)
			if(!GetBufferList(*vcit, msChan))
				return;

		// Fill the table.
		for(it=msChan.begin(); it != msChan.end(); ++it)
		{
			const CString &sChan=it->first;
			const CString &eChanPath=it->second;
			if(!ReadChanPath(eChanPath, sBuf))
			{
				CUtils::PrintMessage("["+GetModName()+".so] "
						+"failed to read the channel buffer for ["+sChan+"]");
				continue;
			}
			// Count the number of lines in a channel buffer.
			const char* pBuf=sBuf.c_str();
			size_t size=sBuf.size();
			unsigned int count=0;
			for(size_t i=0; i<size; ++i)
			{
				if(pBuf[i] == '\n')
					count+=1;
			}

			table.AddRow();
			table.SetCell("Channel", sChan);
			table.SetCell("Lines", CString(count/2)); // each line consumes two lines in a saved buffer.
		}
		PutModule(table);
	}

	void ReplayChannel(CChan *pChan)
	{
		if(pChan == NULL)
		{
			CUtils::PrintError("["+GetModName()+".so] pChan was given NULL in ReplayChannel");
			return;
		}

		const CBuffer &Buffer=pChan->GetBuffer();
		unsigned int size=Buffer.Size();
		time_t logtime=0;
		struct tm t;
		char timeStr[1024];

		PutUser(":***!znc@znc.in PRIVMSG "+pChan->GetName()+" :"+GetModName()+" Buffer Playback...");
		for(unsigned int i=0; i<size; ++i)
		{
			const CBufLine &bufLine = Buffer.GetBufLine(i);
			// Format time.
			logtime=bufLine.GetTime();
			localtime_r(&logtime, &t);
			// Time formatted as Year/Month/Date [hour:minute:second am/pm]
			if(!strftime(timeStr, sizeof(timeStr), "%Y/%b/%d [%I:%M:%S %P] ", &t))
			{
				CUtils::PrintError("["+GetModName()+".so] couldn't format a time log with strftime.]");
				break;
			}
			// Replace {text} with the real text.
			CString chanBuf=bufLine.GetFormat().Replace_n("{text}", timeStr+bufLine.GetText());
			PutUser(chanBuf);
		}
		PutUser(":***!znc@znc.in PRIVMSG "+pChan->GetName()+" :"+GetModName()+" Playback Complete.");
	}

	void Timers(const CString &sArgs)
	{
		ListTimers();
	}

	bool StartTimer()
	{
		if(m_pTimer)
		{
			CUtils::PrintMessage("["+GetModName()+".so] timer is already in action.]");
			return true;
		}

		m_pTimer=new CSaveBuffJob(this, 60, 0, "SaveBuff", "Saves the current buffer to disk every 1 minute");
		if(!AddTimer(m_pTimer))
		{
			CUtils::PrintError("["+GetModName()+".so] failed to start timer.");
			delete m_pTimer;
			m_pTimer=NULL;

			return false;
		}

		return true;
	}

	bool StopTimer()
	{
		bool bRet=true;

		if(m_pTimer == NULL)
		{
			CUtils::PrintError("["+GetModName()+".so] timer is not existent.]");
			return false;
		}

		bRet=RemTimer(m_pTimer);
		m_pTimer = NULL;

		if(!bRet)
			CUtils::PrintError("["+GetModName()+".so] there was a problem deleting a timer.");

		return bRet;		
	}

	void Dump(const CString& sLine)
	{
		CString sChan=sLine.Token(1, true);
		if(sChan.empty() || sChan.find(" ") != CString::npos)
		{
			HandleHelpCommand("help dump");
			return;
		}

		CString sFile;
		if (ReadChan(sChan, sFile))
		{
			VCString vsLines;
			VCString::iterator it;

			sFile.Split("\n", vsLines);

			for (it = vsLines.begin(); it != vsLines.end(); ++it) {
				CString line(*it);
				line.Trim();
				PutModule("["+line+"]");
			}
		}
		else
		{
			CUtils::PrintMessage("["+GetModName()+".so] failed to read a channel buffer for ["
					+sChan+"]");
		}
		PutModule("//!-- EOF "+sChan);
	}

	bool SaveChannelToDisk(CChan& cChan)
	{
		// Use URL encoding.
		CString ePath = GetPath(cChan.GetName().Escape_n(CString::EURL));
		CFile File(ePath);

		if (!cChan.KeepBuffer()) {
			CUtils::PrintMessage("["+GetModName()+".so] KeepBuffer is not enabled"
					+" for this channel, deleting the channel buffer for "
					+cChan.GetName());
			if(!File.Delete())
				CUtils::PrintMessage("["+GetModName()+".so] failed to delete ["+ePath+"]");
			return false;
		}

		const CBuffer &Buffer = cChan.GetBuffer();
		unsigned int bufSize=Buffer.Size();
		CString sFile;
		// Only the last m_uMaxLine lines are added to sFile.
		for (unsigned int i=(Buffer.Size() - std::min(Buffer.Size(), m_uMaxLine)); i<bufSize ; ++i)
		{
			const CBufLine &Line = Buffer.GetBufLine(i);
			sFile+= "@"+CString(Line.GetTime())+" "+Line.GetFormat()+"\n"+Line.GetText()+"\n";
		}

		if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
			File.Chmod(0600);
			File.Write(sFile);
		}
		else
		{
			CUtils::PrintError("["+GetModName()+".so] failed to open file ["+ePath+"]");
			return false;
		}
		File.Close();

		return true;
	}
private:
	unsigned int	m_uMaxLine;
	bool	m_bSeenJoin;
	CString	m_sSavedChannel;
	bool    m_bBootError;
	CTimer *m_pTimer;

	CString GetPath(const CString & sFile)
	{
		return GetSavePath() + "/" + sFile;
	}

	// Fills a map with channel buffers matching sWild.
	// key : decoded filename
	// value : URL-encoded filepath
	bool GetBufferList(const CString &sWild, MCString &mcString)
	{
		CDir dir(GetSavePath());
		CDir::iterator dit;
		CString eFile;
		CString ePath;
		CString sFile;
		// Iterate through all of channel buffers.
		for(dit=dir.begin(); dit != dir.end(); ++dit)
		{
			eFile=(**dit).GetShortName();
			ePath=(**dit).GetLongName();
			// Decode URL encoded file names
			sFile=eFile.Escape_n(CString::EURL,CString::EASCII);

			if(sFile.WildCmp(sWild))
				mcString.insert(pair<CString,CString>(sFile, ePath));
		}

		return true;
	}

	bool ReadChan(const CString & sChan, CString & sBuffer)
	{
		// Use URL encoding.
		CString eChan=sChan.Escape_n(CString::EURL);
		return ReadChanPath(GetPath(eChan), sBuffer);
	}

	bool ReadChanPath(const CString & sPath, CString & sBuffer)
	{
		sBuffer = "";

		CFile File(sPath);
		CString sTempBuf;

		if (sPath.empty() || !File.Open() || !File.ReadFile(sTempBuf))
		{
			CUtils::PrintMessage("["+GetModName()+".so] failed to read "
					+sPath+" in ReadChanPath.");
			File.Close();
			return false;
		}

		File.Close();

		sBuffer=sTempBuf;

		return true;
	}

	bool BootStrap(CChan &cChan)
	{
		CString sFile;
		// Read the backlog stored in disk.
		if (ReadChan(cChan.GetName(), sFile))
		{
			VCString vsLines;
			VCString::iterator it;

			sFile.Split("\n", vsLines);

			for (it = vsLines.begin(); it != vsLines.end(); ++it)
			{
				CString sLine(*it);
				sLine.Trim();
				// Each line is saved in two line format.
				// @timestamp IRCmessage :{text}
				// The content of {text}
				if(sLine[0] == '@' && it+1 != vsLines.end())
				{
					// timestamp
					CString sTimestamp = sLine.Token(0);
					sTimestamp.TrimLeft("@");
					time_t tm = sTimestamp.ToLongLong();

					// IRC message
					CString sFormat = sLine.Token(1, true);

					// The content of {text}
					CString sText(*++it);
					sText.Trim();
					// Add them to the channel buffer.
					cChan.AddBuffer(sFormat, sText, tm);
				}
			}
		}
		else
		{
			CUtils::PrintMessage("["+GetModName()+".so] failed to read a channel buffer for ["
					+cChan.GetName()+"]");
			return false;
		}

		return true;
	}
};


void CSaveBuffJob::RunJob()
{
	CSaveBuff *p = (CSaveBuff *)m_pModule;
	p->SaveBufferToDisk();
}

template<> void TModInfo<CSaveBuff>(CModInfo& Info) {
	Info.SetWikiPage("savebuff");
}

NETWORKMODULEDEFS(CSaveBuff, "Stores channel buffers on disks, and restores them when you join those channels again.")