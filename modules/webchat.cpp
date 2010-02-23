/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "HTTPSock.h"
#include "Server.h"
#include "Template.h"
#include "User.h"
#include "znc.h"
#include "WebModules.h"
#include <sstream>

using std::stringstream;

class CWebChatMod : public CModule {
public:
	MODCONSTRUCTOR(CWebChatMod) {
	}

	virtual ~CWebChatMod() {
	}

	virtual bool OnLoad(const CString& sArgStr, CString& sMessage) {
		return true;
	}

	virtual bool WebRequiresLogin() { return true; }
	virtual bool WebRequiresAdmin() { return false; }
	virtual CString GetWebMenuTitle() { return "webchat"; }

	virtual VWebSubPages& GetSubPages() {
		ClearSubPages();

		// @todo Note: I don't actually suggest we use "sub pages" for the channel nav bar
		//             The channel tabs should be in the main window and updated via jscript
		//             Examples of good sub pages would be like Status, Chat, Settings, etc.
		//             Under the Chat subpage we'd have the jscript client with its own chan tabs
		const vector<CChan*>& vChans = m_pUser->GetChans();

		for (size_t a = 0; a < vChans.size(); a++) {
			CString sName(vChans[a]->GetName());
			VPair vParams;

			vParams.push_back(make_pair("c", sName));
			AddSubPage(new CWebSubPage("chan", sName, vParams));
		}

		return CModule::GetSubPages();
	}

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		std::cerr << "=============================== webchat      sPageName=[" << sPageName << "]" << std::endl;

		if (sPageName.empty() || sPageName == "index") {
			return true;
		} else if (sPageName == "chan") {
			return ChannelPage(WebSock, Tmpl);
		}

		return false;
	}

	bool ChannelPage(CWebSock& WebSock, CTemplate& Tmpl) {
		CChan* pChan = m_pUser->FindChan(WebSock.GetParam("c"));

		if (pChan) {
			Tmpl["Title"] = pChan->GetName();

			const VCString& vLines = pChan->GetBuffer();

			for (size_t a = 0; a < vLines.size(); a++) {
				const CString& sLine(vLines[a]);
				CNick Nick(sLine.Token(0).LeftChomp_n());
				CTemplate& Row = Tmpl.AddRow("BufferLoop");

				if (sLine.Token(1).Equals("PRIVMSG")) {
					Row["Type"] = "PRIVMSG";
					Row["Nick"] = Nick.GetNick();
					Row["Message"] = sLine.Token(3, true).TrimLeft_n(":");
				}
			}

			const map<CString,CNick*>& msNicks = pChan->GetNicks();

			for (map<CString,CNick*>::const_iterator it = msNicks.begin(); it != msNicks.end(); it++) {
				CTemplate& Row = Tmpl.AddRow("NickLoop");
				CNick& Nick = *it->second;

				Row["Nick"] = Nick.GetNick();
				Row["Ident"] = Nick.GetIdent();
				Row["Host"] = Nick.GetHost();
				Row["ModePrefix"] = CString(Nick.GetPermChar());
			}

			return true;
		}

		return false;
	}

private:
	map<CString, unsigned int>	m_suSwitchCounters;
};

MODULEDEFS(CWebChatMod, "Web based chat")
