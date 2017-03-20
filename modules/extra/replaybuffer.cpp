/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Buffer Saving thing, incase your shit goes out while your out
 * Author: crocket <crockabiscuit@gmail.com>
 */

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/Buffer.h>
#include <znc/Server.h>
#include <znc/IRCNetwork.h>
#include <znc/FileUtils.h>
#include <time.h> // localtime_r, strftime

class CReplayBuffer;

class CReplayBufferJob : public CTimer
{
public:
	CReplayBufferJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CReplayBufferJob() {}

protected:
	virtual void RunJob();
};

class CReplayBuffer : public CModule
{
public:
	MODCONSTRUCTOR(CReplayBuffer)
	{
		m_bSeenJoin=false;
		m_pTimer = NULL;

		AddHelpCommand();
		AddCommand("dump", static_cast<CModCommand::ModCmdFunc>(&CReplayBuffer::Dump),
				"<channel>", "Dump a saved channel buffer in " + GetModName() + " module.");
		AddCommand("del", static_cast<CModCommand::ModCmdFunc>(&CReplayBuffer::Del),
				"<channel>", "Delete channels. Supported wildcards : * and ?");
		AddCommand("count", static_cast<CModCommand::ModCmdFunc>(&CReplayBuffer::Count),
				"<channel>", "Count lines in saved channel buffers. Wildcards * and ? supported");
		AddCommand("save", static_cast<CModCommand::ModCmdFunc>(&CReplayBuffer::Save),
				"", "Save all channel buffers.");
		AddCommand("list", static_cast<CModCommand::ModCmdFunc>(&CReplayBuffer::List),
				"", "List all of saved channel buffers.");
		AddCommand("timers", static_cast<CModCommand::ModCmdFunc>(&CReplayBuffer::Timers),
				"", "List the timers associated with " + GetModName());
	}

	virtual ~CReplayBuffer()
	{
		SaveBuffer();
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		// If the module was loaded with ZNC connected to a network,
		// add a timer that saves channel buffers every minute.
		if(GetNetwork()->IsIRCConnected())
		{
			if(!StartTimer())
			{
				sMessage=GetModName()+" failed to start a timer in OnLoad.";
				return false;
			}
		}

		return true;
	}

	virtual void OnIRCConnected()
	{
		StartTimer();
	}

	virtual void OnIRCDisconnected()
	{
		StopTimer();
		SaveBuffer();
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
			
			CString sChan=sLine.Token(2);
			// If it contains the optional ":", remove it
			if(sChan.Left(1) == ":")
				sChan.LeftChomp();
			CString sNick=sLine.Token(0, false, "!");
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
			CString sChan=sLine.Token(3);
			if(sChan != m_sSavedChannel)
			{
				CUtils::PrintMessage("["+GetModName()+".so] 366 was received for ["
						+sChan+"] while "+GetModName()+" was expecting it from ["
						+ m_sSavedChannel+"]");
				return CONTINUE;
			}

			if(!pChan->GetBuffer().IsEmpty())
				ReplayChannel(pChan);
			m_bSeenJoin=false;
		}

		return CONTINUE;
	}

	virtual void OnJoin(const CNick &cNick, CChan& cChan)
	{
		if(cNick.GetNick().Equals(GetNetwork()->GetCurNick()) && cChan.GetBuffer().IsEmpty())
			if(!BootStrap(cChan))
				DEBUG("["+GetModName()+".so] BootStrap not successfull in OnJoin.");
	}

	virtual void OnPart(const CNick& cNick, CChan &cChan, const CString &sMsg)
	{
		if(cNick.GetNick().Equals(GetNetwork()->GetCurNick())) {
			if(!SaveChannel(cChan))
				CUtils::PrintError("["+GetModName()+".so] failed to save the channel buffer for ["
						+cChan.GetName()+"]");
		}
	}

	unsigned int SaveBuffer()
	{
		unsigned int count=0;

		CIRCNetwork* pNetwork=GetNetwork();
		if(pNetwork == NULL)
		{
			CUtils::PrintError("["+GetModName()+".so] not associated with any IRC network, "+
					"and it can't save any channel buffer.");
			return 0;
		}

		// Save each channel buffer to disk
		const vector<CChan *>& vChans = pNetwork->GetChans();
		vector<CChan*>::const_iterator it;
		for (it=vChans.begin(); it != vChans.end(); ++it)
		{
			CChan &cChan=**it;
			if(!SaveChannel(cChan))
				CUtils::PrintError("["+GetModName()+".so] failed to save the channel buffer for ["
						+cChan.GetName()+"]");
			else
				count+=1;
		}

		return count;
	}

protected:
private:
	bool	m_bSeenJoin;
	CString	m_sSavedChannel;
	CTimer *m_pTimer;
	// Start of command functions.
	void Save(const CString &sArgs)
	{
		unsigned int count=SaveBuffer();
		PutModule(CString(count)+" channel buffer(s) saved.");
	}

	void Timers(const CString &sArgs)
	{
		ListTimers();
	}

	void List(const CString &sArgs)
	{
		MCString mcString;
		MCString::iterator it;

		// Fill the map with channel buffers.
		GetBufferList("*", mcString);
		if(mcString.empty())
		{
			PutModule("No channel buffer is saved yet...");
			return;
		}

		CTable table;
		table.AddColumn("Channel");
		table.AddColumn("Size in bytes");
		// Fill the table using the map.
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
		GetBufferList(sArgs, mcChans);
		// Delete channel buffers.
		unsigned int count=0;
		for(it=mcChans.begin(); it != mcChans.end(); ++it)
		{
			const CString &ePath=it->second;
			if(!CFile::Delete(ePath))
				CUtils::PrintError("["+GetModName()+".so] failed to delete "+ePath);
			else
				count+=1;
		}
		PutModule(CString(count)+" channel buffers deleted.");
	}
	// Count the number of lines in channel buffers.
	void Count(const CString &sLine)
	{
		CString sBuf;
		MCString msChan;
		MCString::iterator it;
		CString sCmd=sLine.Token(0);
		CString sArgs=sLine.Token(1, true);

		if(sArgs.empty() || sArgs.find(" ") != CString::npos)
		{
			HandleHelpCommand("help count");
			return;
		}

		CTable table;
		table.AddColumn("Channel");
		table.AddColumn("Lines");

		GetBufferList(sArgs, msChan);

		// Fill the table using the map entries.
		for(it=msChan.begin(); it != msChan.end(); ++it)
		{
			const CString &sChan=it->first;
			const CString &sChanPath=it->second;
			if(!ReadChanFile(sChanPath, sBuf))
			{
				CUtils::PrintError("["+GetModName()+".so] "
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

	void Dump(const CString& sLine)
	{
		// Channel names are case-insensitive.
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
			DEBUG("["+GetModName()+".so] failed to read a channel buffer for ["
					+sChan+"]");
		}
		PutModule("//!-- EOF "+sChan);
	}
	// End of command functions.
	CString GetPath(const CString & sFile)
	{
		return GetSavePath()+"/"+sFile;
	}

	// Fills a map with channel buffers matching sWild.
	// key : decoded filename
	// value : URL-encoded filepath
	void GetBufferList(const CString &sWild, MCString &mcString)
	{
		CDir dir(GetSavePath());
		CDir::iterator dit;
		CString eFile;
		CString ePath;
		CString sFile;
		CString slWild=sWild.AsLower(); // channel names are case-insensitive.
		// Iterate through all of channel buffers.
		for(dit=dir.begin(); dit != dir.end(); ++dit)
		{
			eFile=(**dit).GetShortName();
			ePath=(**dit).GetLongName();
			// Decode URL encoded file names
			sFile=eFile.Escape_n(CString::EURL,CString::EASCII);

			if(sFile.WildCmp(slWild))
				mcString.insert(pair<CString,CString>(sFile, ePath));
		}
	}

	bool ReadChan(const CString & sChan, CString & sBuffer)
	{
		// Use URL encoding.
		// Channel names are case-insensitive.
		CString eChan=sChan.AsLower().Escape_n(CString::EURL);
		return ReadChanFile(GetPath(eChan), sBuffer);
	}

	bool ReadChanFile(const CString & sPath, CString & sBuffer)
	{
		sBuffer = "";

		CFile File(sPath);
		CString sFile;

		if (sPath.empty() || !File.Open() || !File.ReadFile(sFile))
		{
			DEBUG("["+GetModName()+".so] failed to read "
					+sPath+" in ReadChanFile.");
			File.Close();
			return false;
		}

		File.Close();

		sBuffer=sFile;

		return true;
	}
	// Fill the channel buffer with the saved buffer.
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
			DEBUG("["+GetModName()+".so] failed to read a channel buffer for ["
					+cChan.GetName()+"]");
			return false;
		}

		return true;
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

	bool StartTimer()
	{
		if(m_pTimer)
		{
			CUtils::PrintMessage("["+GetModName()+".so] timer is already in action.]");
			return true;
		}

		m_pTimer=new CReplayBufferJob(this, 60, 0, "SaveBuffer", "Saves the current buffer to disk every 1 minute");
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

	bool SaveChannel(CChan& cChan)
	{
		// Set filenames to lower-case channel names in URL encoding to avoid "/"
		// Since channel names are case-insensitive, lower-case names are used in filenames.
		CString sPath = GetPath(cChan.GetName().AsLower().Escape_n(CString::EURL));
		if (!cChan.KeepBuffer()) {
			CUtils::PrintMessage("["+GetModName()+".so] KeepBuffer is not enabled"
					+" for this channel, deleting the channel buffer for "
					+cChan.GetName());
			if(!CFile::Delete(sPath))
				DEBUG("["+GetModName()+".so] failed to delete ["+sPath+"]");
			return false;
		}
		// Rearrange the channel buffer so as to save it.
		const CBuffer &Buffer = cChan.GetBuffer();
		unsigned int bufSize=Buffer.Size();
		CString sBuf;
		for (unsigned int i=0; i<bufSize ; ++i)
		{
			const CBufLine &Line = Buffer.GetBufLine(i);
			sBuf+= "@"+CString(Line.GetTime())+" "+Line.GetFormat()+"\n"+Line.GetText()+"\n";
		}

		CFile File(sPath);
		if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
			File.Chmod(0600);
			File.Write(sBuf);
		}
		else
		{
			CUtils::PrintError("["+GetModName()+".so] failed to open file ["+sPath+"]");
			return false;
		}
		File.Close();

		return true;
	}
};


void CReplayBufferJob::RunJob()
{
	CReplayBuffer *p = (CReplayBuffer *)m_pModule;
	p->SaveBuffer();
}

template<> void TModInfo<CReplayBuffer>(CModInfo& Info) {
	Info.SetWikiPage("replaybuffer");
}

NETWORKMODULEDEFS(CReplayBuffer, "Stores channel buffers on disks, and restores them when you join those channels again.")
