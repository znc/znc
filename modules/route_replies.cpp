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

#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

struct reply {
	const char *szReply;
	bool bLastResponse;
};

// TODO this list is far from complete, no errors are handled
static const struct {
	const char *szRequest;
	struct reply vReplies[19];
} vRouteReplies[] = {
	{"WHO", {
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{"352", false},  /* rfc1459 RPL_WHOREPLY */
		{"315", true},  /* rfc1459 RPL_ENDOFWHO */
		{"354", false}, // e.g. Quaknet uses this for WHO #chan %n
		{"403", true}, // No such chan
		{NULL, true}
	}},
	{"LIST", {
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{"321", false},  /* rfc1459 RPL_LISTSTART */
		{"322", false},  /* rfc1459 RPL_LIST */
		{"323", true},  /* rfc1459 RPL_LISTEND */
		{NULL, true}
	}},
	{"NAMES", {
		{"353", false},  /* rfc1459 RPL_NAMREPLY */
		{"366", true},  /* rfc1459 RPL_ENDOFNAMES */
		// No such nick/channel
		{"401", true},
		{NULL, true},
	}},
	{"LUSERS", {
		{"251", false},  /* rfc1459 RPL_LUSERCLIENT */
		{"252", false},  /* rfc1459 RPL_LUSEROP */
		{"253", false},  /* rfc1459 RPL_LUSERUNKNOWN */
		{"254", false},  /* rfc1459 RPL_LUSERCHANNELS */
		{"255", false},  /* rfc1459 RPL_LUSERME */
		{"265", false},
		{"266", true},
		// We don't handle 250 here since some IRCds don't sent it
		//{"250", true},
		{NULL, true}
	}},
	{"WHOIS", {
		{"311", false},  /* rfc1459 RPL_WHOISUSER */
		{"312", false},  /* rfc1459 RPL_WHOISSERVER */
		{"313", false},  /* rfc1459 RPL_WHOISOPERATOR */
		{"317", false},  /* rfc1459 RPL_WHOISIDLE */
		{"319", false},  /* rfc1459 RPL_WHOISCHANNELS */
		{"301", false},  /* rfc1459 RPL_AWAY */
		{"276", false},  /* oftc-hybrid RPL_WHOISCERTFP */
		{"330", false},  /* ratbox RPL_WHOISLOGGEDIN
		                    aka ircu RPL_WHOISACCOUNT */
		{"338", false},  /* RPL_WHOISACTUALLY -- "actually using host" */
		{"378", false},  /* RPL_WHOISHOST -- real address of vhosts */
		{"671", false},  /* RPL_WHOISSECURE */
		{"307", false},  /* RPL_WHOISREGNICK */
		{"379", false},  /* RPL_WHOISMODES */
		{"760", false},  /* ircv3.2 RPL_WHOISKEYVALUE */
		{"318", true},  /* rfc1459 RPL_ENDOFWHOIS */
		{"401", true},  /* rfc1459 ERR_NOSUCHNICK */
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{"431", true},  /* rfc1459 ERR_NONICKNAMEGIVEN */
		{NULL, true}
	}},
	{"PING", {
		{"PONG", true},
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{"409", true},  /* rfc1459 ERR_NOORIGIN */
		{NULL, true}
	}},
	{"USERHOST", {
		{"302", true},
		{"461", true},  /* rfc1459 ERR_NEEDMOREPARAMS */
		{NULL, true}
	}},
	{"TIME", {
		{"391", true},  /* rfc1459 RPL_TIME */
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{NULL, true}
	}},
	{"WHOWAS", {
		{"406", false},  /* rfc1459 ERR_WASNOSUCHNICK */
		{"312", false},  /* rfc1459 RPL_WHOISSERVER */
		{"314", false},  /* rfc1459 RPL_WHOWASUSER */
		{"369", true},  /* rfc1459 RPL_ENDOFWHOWAS */
		{"431", true},  /* rfc1459 ERR_NONICKNAMEGIVEN */
		{NULL, true}
	}},
	{"ISON", {
		{"303", true},  /* rfc1459 RPL_ISON */
		{"461", true},  /* rfc1459 ERR_NEEDMOREPARAMS */
		{NULL, true}
	}},
	{"LINKS", {
		{"364", false},  /* rfc1459 RPL_LINKS */
		{"365", true},  /* rfc1459 RPL_ENDOFLINKS */
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{NULL, true}
	}},
	{"MAP", {
		{"006", false},
		// inspircd
		{"270", false},
		// SilverLeo wants this two added
		{"015", false},
		{"017", true},
		{"007", true},
		{"481", true},  /* rfc1459 ERR_NOPRIVILEGES */
		{NULL, true}
	}},
	{"TRACE", {
		{"200", false},  /* rfc1459 RPL_TRACELINK */
		{"201", false},  /* rfc1459 RPL_TRACECONNECTING */
		{"202", false},  /* rfc1459 RPL_TRACEHANDSHAKE */
		{"203", false},  /* rfc1459 RPL_TRACEUNKNOWN */
		{"204", false},  /* rfc1459 RPL_TRACEOPERATOR */
		{"205", false},  /* rfc1459 RPL_TRACEUSER */
		{"206", false},  /* rfc1459 RPL_TRACESERVER */
		{"208", false},  /* rfc1459 RPL_TRACENEWTYPE */
		{"261", false},  /* rfc1459 RPL_TRACELOG */
		{"262", true},
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{NULL, true}
	}},
	{"USERS", {
		{"265", false},
		{"266", true},
		{"392", false},  /* rfc1459 RPL_USERSSTART */
		{"393", false},  /* rfc1459 RPL_USERS */
		{"394", true},  /* rfc1459 RPL_ENDOFUSERS */
		{"395", false},  /* rfc1459 RPL_NOUSERS */
		{"402", true},  /* rfc1459 ERR_NOSUCHSERVER */
		{"424", true},  /* rfc1459 ERR_FILEERROR */
		{"446", true},  /* rfc1459 ERR_USERSDISABLED */
		{NULL, true},
	}},
	{"METADATA", {
		{"761", false},  /* ircv3.2 RPL_KEYVALUE */
		{"762", true},  /* ircv3.2 RPL_METADATAEND */
		{"765", true},  /* ircv3.2 ERR_TARGETINVALID */
		{"766", true},  /* ircv3.2 ERR_NOMATCHINGKEYS */
		{"767", true},  /* ircv3.2 ERR_KEYINVALID */
		{"768", true},  /* ircv3.2 ERR_KEYNOTSET */
		{"769", true},  /* ircv3.2 ERR_KEYNOPERMISSION */
		{NULL, true},
	}},
	// This is just a list of all possible /mode replies stuffed together.
	// Since there should never be more than one of these going on, this
	// should work fine and makes the code simpler.
	{"MODE", {
		// "You're not a channel operator"
		{"482", true},
		// MODE I
		{"346", false},
		{"347", true},
		// MODE b
		{"367", false},
		{"368", true},
		// MODE e
		{"348", false},
		{"349", true},
		{"467", true},  /* rfc1459 ERR_KEYSET */
		{"472", true},  /* rfc1459 ERR_UNKNOWNMODE */
		{"501", true},  /* rfc1459 ERR_UMODEUNKNOWNFLAG */
		{"502", true},  /* rfc1459 ERR_USERSDONTMATCH */
		{NULL, true},
	 }},
	// END (last item!)
	{NULL, {{NULL, true}}}
};

class CRouteTimeout : public CTimer {
public:
	CRouteTimeout(CModule* pModule, unsigned int uInterval, unsigned int uCycles,
			const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CRouteTimeout() {}

protected:
	void RunJob() override;
};

struct queued_req {
	CString sLine;
	const struct reply *reply;
};

typedef std::map<CClient *, std::vector<struct queued_req> > requestQueue;

class CRouteRepliesMod : public CModule
{
public:
	MODCONSTRUCTOR(CRouteRepliesMod)
	{
		m_pDoing = NULL;
		m_pReplies = NULL;

		AddHelpCommand();
		AddCommand("Silent", static_cast<CModCommand::ModCmdFunc>(&CRouteRepliesMod::SilentCommand),
			"[yes|no]");
	}

	virtual ~CRouteRepliesMod() {
		requestQueue::iterator it;

		while (!m_vsPending.empty()) {
			it = m_vsPending.begin();

			while (!it->second.empty()) {
				PutIRC(it->second[0].sLine);
				it->second.erase(it->second.begin());
			}

			m_vsPending.erase(it);
		}
	}

	void OnIRCConnected() override
	{
		m_pDoing = NULL;
		m_pReplies = NULL;
		m_vsPending.clear();

		// No way we get a reply, so stop the timer (If it's running)
		RemTimer("RouteTimeout");
	}

	void OnIRCDisconnected() override
	{
		OnIRCConnected(); // Let's keep it in one place
	}

	void OnClientDisconnect() override
	{
		requestQueue::iterator it;

		if (GetClient() == m_pDoing) {
			// The replies which aren't received yet will be
			// broadcasted to everyone, but at least nothing breaks
			RemTimer("RouteTimeout");
			m_pDoing = NULL;
			m_pReplies = NULL;
		}

		it = m_vsPending.find(GetClient());

		if (it != m_vsPending.end())
			m_vsPending.erase(it);

		SendRequest();
	}

	EModRet OnRaw(CString& sLine) override
	{
		CString sCmd = sLine.Token(1).AsUpper();
		size_t i = 0;

		if (!m_pReplies)
			return CONTINUE;

		// Is this a "not enough arguments" error?
		if (sCmd == "461") {
			// :server 461 nick WHO :Not enough parameters
			CString sOrigCmd = sLine.Token(3);

			if (m_sLastRequest.Token(0).Equals(sOrigCmd)) {
				// This is the reply to the last request
				if (RouteReply(sLine, true))
					return HALTCORE;
				return CONTINUE;
			}
		}

		while (m_pReplies[i].szReply != NULL) {
			if (m_pReplies[i].szReply == sCmd) {
				if (RouteReply(sLine, m_pReplies[i].bLastResponse, sCmd == "353"))
					return HALTCORE;
				return CONTINUE;
			}
			i++;
		}

		// TODO HALTCORE is wrong, it should not be passed to
		// the clients, but the core itself should still handle it!

		return CONTINUE;
	}

	EModRet OnUserRaw(CString& sLine) override
	{
		CString sCmd = sLine.Token(0).AsUpper();

		if (!GetNetwork()->GetIRCSock())
			return CONTINUE;

		if (sCmd.Equals("MODE")) {
			// Check if this is a mode request that needs to be handled

			// If there are arguments to a mode change,
			// we must not route it.
			if (!sLine.Token(3, true).empty())
				return CONTINUE;

			// Grab the mode change parameter
			CString sMode = sLine.Token(2);

			// If this is a channel mode request, znc core replies to it
			if (sMode.empty())
				return CONTINUE;

			// Check if this is a mode change or a specific
			// mode request (the later needs to be routed).
			sMode.TrimPrefix("+");
			if (sMode.length() != 1)
				return CONTINUE;

			// Now just check if it's one of the supported modes
			switch (sMode[0]) {
			case 'I':
			case 'b':
			case 'e':
				break;
			default:
				return CONTINUE;
			}

			// Ok, this looks like we should route it.
			// Fall through to the next loop
		}

		for (size_t i = 0; vRouteReplies[i].szRequest != NULL; i++) {
			if (vRouteReplies[i].szRequest == sCmd) {
				struct queued_req req = {
					sLine, vRouteReplies[i].vReplies
				};
				m_vsPending[GetClient()].push_back(req);
				SendRequest();

				return HALTCORE;
			}
		}

		return CONTINUE;
	}

	void Timeout()
	{
		// The timer will be deleted after this by the event loop

		if (!GetNV("silent_timeouts").ToBool()) {
			PutModule("This module hit a timeout which is possibly a bug.");
			PutModule("To disable this message, do \"/msg " + GetModNick()
					+ " silent yes\"");
			PutModule("Last request: " + m_sLastRequest);
			PutModule("Expected replies: ");

			for (size_t i = 0; m_pReplies[i].szReply != NULL; i++) {
				if (m_pReplies[i].bLastResponse)
					PutModule(m_pReplies[i].szReply +
							CString(" (last)"));
				else
					PutModule(m_pReplies[i].szReply);
			}
		}

		m_pDoing = NULL;
		m_pReplies = NULL;
		SendRequest();
	}

private:
	bool RouteReply(const CString& sLine, bool bFinished = false, bool bIsRaw353 = false)
	{
		if (!m_pDoing)
			return false;

		// 353 needs special treatment due to NAMESX and UHNAMES
		if (bIsRaw353)
			GetNetwork()->GetIRCSock()->ForwardRaw353(sLine, m_pDoing);
		else
			m_pDoing->PutClient(sLine);

		if (bFinished) {
			// Stop the timeout
			RemTimer("RouteTimeout");

			m_pDoing = NULL;
			m_pReplies = NULL;
			SendRequest();
		}

		return true;
	}

	void SendRequest()
	{
		requestQueue::iterator it;

		if (m_pDoing || m_pReplies)
			return;

		if (m_vsPending.empty())
			return;

		it = m_vsPending.begin();

		if (it->second.empty()) {
			m_vsPending.erase(it);
			SendRequest();
			return;
		}

		// When we are called from the timer, we need to remove it.
		// We can't delete it (segfault on return), thus we
		// just stop it. The main loop will delete it.
		CTimer *pTimer = FindTimer("RouteTimeout");
		if (pTimer) {
			pTimer->Stop();
			UnlinkTimer(pTimer);
		}
		AddTimer(new CRouteTimeout(this, 60, 1, "RouteTimeout",
				"Recover from missing / wrong server replies"));

		m_pDoing = it->first;
		m_pReplies = it->second[0].reply;
		m_sLastRequest = it->second[0].sLine;
		PutIRC(it->second[0].sLine);
		it->second.erase(it->second.begin());
	}

	void SilentCommand(const CString& sLine) {
		const CString sValue = sLine.Token(1);

		if (!sValue.empty()) {
			SetNV("silent_timeouts", sValue);
		}

		CString sPrefix = GetNV("silent_timeouts").ToBool() ? "dis" : "en";
		PutModule("Timeout messages are " + sPrefix + "abled.");
	}

	CClient            *m_pDoing;
	const struct reply *m_pReplies;
	requestQueue        m_vsPending;
	// This field is only used for display purpose.
	CString             m_sLastRequest;
};

void CRouteTimeout::RunJob()
{
	CRouteRepliesMod *pMod = (CRouteRepliesMod *) GetModule();
	pMod->Timeout();
}

template<> void TModInfo<CRouteRepliesMod>(CModInfo& Info) {
	Info.SetWikiPage("route_replies");
}

NETWORKMODULEDEFS(CRouteRepliesMod, "Send replies (e.g. to /who) to the right client only")
