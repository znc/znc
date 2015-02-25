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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

using std::vector;

class CStickyChan : public CModule
{
public:
	MODCONSTRUCTOR(CStickyChan) {
		AddHelpCommand();
		AddCommand("Stick", static_cast<CModCommand::ModCmdFunc>(&CStickyChan::OnStickCommand), "<#channel> [key]", "Sticks a channel");
		AddCommand("Unstick", static_cast<CModCommand::ModCmdFunc>(&CStickyChan::OnUnstickCommand), "<#channel>", "Unsticks a channel");
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CStickyChan::OnListCommand), "", "Lists sticky channels");
	}
	virtual ~CStickyChan()
	{
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override;

	EModRet OnUserPart(CString& sChannel, CString& sMessage) override
	{
		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it)
		{
			if (sChannel.Equals(it->first))
			{
				CChan* pChan = GetNetwork()->FindChan(sChannel);

				if (pChan)
				{
					pChan->JoinUser();
					return HALT;
				}
			}
		}

		return CONTINUE;
	}

	void OnStickCommand(const CString& sCommand)
	{
		CString sChannel = sCommand.Token(1).AsLower();
		if (sChannel.empty()) {
			PutModule("Usage: Stick <#channel> [key]");
			return;
		}
		SetNV(sChannel, sCommand.Token(2));
		PutModule("Stuck " + sChannel);
	}

	void OnUnstickCommand(const CString& sCommand) {
		CString sChannel = sCommand.Token(1);
		if (sChannel.empty()) {
			PutModule("Usage: Unstick <#channel>");
			return;
		}
		MCString::iterator it = FindNV(sChannel);
		if (it != EndNV())
			DelNV(it);
		PutModule("Unstuck " + sChannel);
	}

	void OnListCommand(const CString& sCommand) {
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

	void RunJob()
	{
		CIRCNetwork* pNetwork = GetNetwork();
		if (!pNetwork->GetIRCSock())
			return;

		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it)
		{
			CChan *pChan = pNetwork->FindChan(it->first);
			if (!pChan) {
				pChan = new CChan(it->first, pNetwork, true);
				if (!it->second.empty())
					pChan->SetKey(it->second);
				if (!pNetwork->AddChan(pChan)) {
					/* AddChan() deleted that channel */
					PutModule("Could not join [" + it->first
							+ "] (# prefix missing?)");
					continue;
				}
			}
			if (!pChan->IsOn() && pNetwork->IsIRCConnected()) {
				PutModule("Joining [" + pChan->GetName() + "]");
				PutIRC("JOIN " + pChan->GetName() + (pChan->GetKey().empty()
							? "" : " " + pChan->GetKey()));
			}
		}
	}

	CString GetWebMenuTitle() override { return "Sticky Chans"; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName == "index") {
			bool bSubmitted = (WebSock.GetParam("submitted").ToInt() != 0);

			const vector<CChan*>& Channels = GetNetwork()->GetChans();
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

	bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
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
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("List of channels, separated by comma.");
}

NETWORKMODULEDEFS(CStickyChan, "configless sticky chans, keeps you there very stickily even")
