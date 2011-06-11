/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "User.h"

class CStickyChan : public CModule
{
public:
	MODCONSTRUCTOR(CStickyChan) {}
	virtual ~CStickyChan()
	{
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage);

	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage)
	{
		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it)
		{
			if (sChannel.Equals(it->first))
			{
				CChan* pChan = m_pUser->FindChan(sChannel);

				if (pChan)
				{
					pChan->JoinUser(true, "", m_pClient);
					return HALT;
				}
			}
		}

		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand)
	{
		CString sCmdName = sCommand.Token(0);
		CString sChannel = sCommand.Token(1);
		sChannel.MakeLower();
		if ((sCmdName == "stick") && (!sChannel.empty()))
		{
			SetNV(sChannel, sCommand.Token(2));
			PutModule("Stuck " + sChannel);
		}
		else if ((sCmdName == "unstick") && (!sChannel.empty()))
		{
			MCString::iterator it = FindNV(sChannel);
			if (it != EndNV())
				DelNV(it);

			PutModule("UnStuck " + sChannel);
		}
		else if ((sCmdName == "list") && (sChannel.empty()))
		{
			int i = 1;
			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it, i++)
			{
				if (it->second.empty())
					PutModule(CString(i) + ": " + it->first);
				else
					PutModule(CString(i) + ": " + it->first + " (" + it->second + ")");
			}
			PutModule(" -- End of List");
		}
		else
		{
			PutModule("USAGE: [un]stick #channel [key], list");
		}
	}

	virtual void RunJob()
	{
		if (!m_pUser->GetIRCSock())
			return;

		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it)
		{
			CChan *pChan = m_pUser->FindChan(it->first);
			if (!pChan) {
				pChan = new CChan(it->first, m_pUser, true);
				if (!it->second.empty())
					pChan->SetKey(it->second);
				if (!m_pUser->AddChan(pChan)) {
					/* AddChan() deleted that channel */
					PutModule("Could not join [" + it->first
							+ "] (# prefix missing?)");
					continue;
				}
			}
			if (!pChan->IsOn()) {
				PutModule("Joining [" + pChan->GetName() + "]");
				PutIRC("JOIN " + pChan->GetName() + (pChan->GetKey().empty()
							? "" : " " + pChan->GetKey()));
			}
		}
	}

	virtual CString GetWebMenuTitle() { return "Sticky Chans"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "index") {
			bool bSubmitted = (WebSock.GetParam("submitted").ToInt() != 0);

			const vector<CChan*>& Channels = m_pUser->GetChans();
			for (unsigned int c = 0; c < Channels.size(); c++) {
				const CString sChan = Channels[c]->GetName();
				bool bStick = FindNV(sChan) != EndNV();

				if(bSubmitted) {
					bool bNewStick = WebSock.GetParam("stick_" + sChan).ToBool();
					if(bNewStick && !bStick)
						SetNV(sChan, ""); // no password support for now unless chansaver is active too
					else if(!bNewStick && bStick) {
						MCString::iterator it = FindNV(sChan);
						if(it != EndNV())
							DelNV(it);
					}
					bStick = bNewStick;
				}

				CTemplate& Row = Tmpl.AddRow("ChannelLoop");
				Row["Name"] = sChan;
				Row["Sticky"] = CString(bStick);
			}

			if(bSubmitted) {
				WebSock.GetSession()->AddSuccess("Changes have been saved!");
			}

			return true;
		}

		return false;
	}

	virtual bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "webadmin/channel") {
			CString sChan = Tmpl["ChanName"];
			bool bStick = FindNV(sChan) != EndNV();
			if (Tmpl["WebadminAction"].Equals("display")) {
				Tmpl["Sticky"] = CString(bStick);
			} else if (WebSock.GetParam("embed_stickychan_presented").ToBool()) {
				bool bNewStick = WebSock.GetParam("embed_stickychan_sticky").ToBool();
				if(bNewStick && !bStick) {
					SetNV(sChan, ""); // no password support for now unless chansaver is active too
					WebSock.GetSession()->AddSuccess("Channel become sticky!");
				} else if(!bNewStick && bStick) {
					DelNV(sChan);
					WebSock.GetSession()->AddSuccess("Channel stopped being sticky!");
				}
			}
			return true;
		}
		return false;
	}

};


static void RunTimer(CModule * pModule, CFPTimer *pTimer)
{
	((CStickyChan *)pModule)->RunJob();
}

bool CStickyChan::OnLoad(const CString& sArgs, CString& sMessage)
{
	VCString vsChans;
	VCString::iterator it;
	sArgs.Split(",", vsChans, false);

	for (it = vsChans.begin(); it != vsChans.end(); ++it) {
		CString sChan = it->Token(0);
		CString sKey = it->Token(1, true);
		SetNV(sChan, sKey);
	}

	// Since we now have these channels added, clear the argument list
	SetArgs("");

	AddTimer(RunTimer, "StickyChanTimer", 15);
	return(true);
}

template<> void TModInfo<CStickyChan>(CModInfo& Info) {
	Info.SetWikiPage("stickychan");
}

MODULEDEFS(CStickyChan, "configless sticky chans, keeps you there very stickily even")
