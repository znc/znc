/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "User.h"

struct reply {
	const char *szReply;
	bool bLastResponse;
};

// TODO this list is far from complete, no errors are handled
static const struct {
	const char *szRequest;
	struct reply vReplies[10];
} vRouteReplies[] = {
	{"WHO", {
		{"352", false},
		{"403", true}, // No such chan
		{"315", true},
		{NULL, true}
	}},
	{"LIST", {
		{"321", false},
		{"322", false},
		{"323", true},
		{NULL, true}
	}},
	{"NAMES", {
		{"353", false},
		{"366", true},
		// No such nick/channel
		{"401", true},
		{NULL, true},
	}},
	{"LUSERS", {
		{"251", false},
		{"252", false},
		{"253", false},
		{"254", false},
		{"255", false},
		{"265", false},
		{"266", true},
		// We don't handle 250 here since some IRCds don't sent it
		//{"250", true},
		{NULL, true}
	}},
	{"WHOIS", {
		{"311", false},
		{"319", false},
		{"312", false},
		// "<ip> :actually using host"
		{"338", false},
		{"318", true},
		// No such nick/channel
		{"401", true},
		// No such server
		{"402", true},
		{NULL, true}
	}},
	{"PING", {
		{"PONG", true},
		{NULL, true}
	}},
	{"USERHOST", {
		{"302", true},
		{NULL, true}
	}},
	{"TIME", {
		{"391", true},
		{NULL, true}
	}},
	{"WHOWAS", {
		{"312", false},
		{"314", false},
		{"369", true},
		{NULL, true}
	}},
	{"ISON", {
		{"303", true},
		{NULL, true}
	}},
	{"LINKS", {
		{"364", false},
		{"365", true},
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
		{NULL, true}
	}},
	{"TRACE", {
		{"200", false},
		{"205", false},
		{"262", true},
		{NULL, true}
	}},
	{"USERS", {
		{"265", false},
		{"266", true},
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
	virtual void RunJob();
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

	virtual void OnIRCConnected()
	{
		m_pDoing = NULL;
		m_pReplies = NULL;
		m_vsPending.clear();

		// No way we get a reply, so stop the timer (If it's running)
		RemTimer("RouteTimeout");
	}

	virtual void OnIRCDisconnected()
	{
		OnIRCConnected(); // Let's keep it in one place
	}

	virtual void OnClientDisconnect()
	{
		requestQueue::iterator it;

		if (m_pClient == m_pDoing) {
			// The replies which aren't received yet will be
			// broadcasted to everyone, but at least nothing breaks
			m_pDoing = NULL;
			m_pReplies = NULL;
		}

		it = m_vsPending.find(m_pClient);

		if (it == m_vsPending.end())
			return;

		m_vsPending.erase(it);
	}

	virtual EModRet OnRaw(CString& sLine)
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
				if (RouteReply(sLine, m_pReplies[i].bLastResponse))
					return HALTCORE;
				return CONTINUE;
			}
			i++;
		}

		// TODO HALTCORE is wrong, it should not be passed to
		// the clients, but the core itself should still handle it!

		return CONTINUE;
	}

	virtual EModRet OnUserRaw(CString& sLine)
	{
		CString sCmd = sLine.Token(0).AsUpper();
		size_t i = 0;

		if (!m_pUser->GetIRCSock())
			return CONTINUE;

		while (vRouteReplies[i].szRequest != NULL) {
			if (vRouteReplies[i].szRequest == sCmd) {

				struct queued_req req = {
					sLine, vRouteReplies[i].vReplies
				};
				m_vsPending[m_pClient].push_back(req);
				SendRequest();

				return HALTCORE;
			}
			i++;
		}

		return CONTINUE;
	}

	void Timeout()
	{
		// The timer will be deleted after this by the event loop

		PutModule("This module hit a timeout. This is a BUG!");

		PutModule("Last request: " + m_sLastRequest);

		PutModule("Expected replies: ");

		for (size_t i = 0; m_pReplies[i].szReply != NULL; i++) {
			if (m_pReplies[i].bLastResponse)
				PutModule(m_pReplies[i].szReply +
						CString(" (last)"));
			else
				PutModule(m_pReplies[i].szReply);
		}

		m_pDoing = NULL;
		m_pReplies = NULL;
		SendRequest();
	}

private:
	bool RouteReply(const CString& sLine, bool bFinished = false)
	{
		if (!m_pDoing)
			return false;

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
		AddTimer(new CRouteTimeout(this, 20, 1, "RouteTimeout",
				"Recover from missing / wrong server replies"));

		m_pDoing = it->first;
		m_pReplies = it->second[0].reply;
		m_sLastRequest = it->second[0].sLine;
		PutIRC(it->second[0].sLine);
		it->second.erase(it->second.begin());
	}

	CClient	*m_pDoing;
	const struct reply	*m_pReplies;
	requestQueue	m_vsPending;
	// This field is only used for display purpose.
	CString		m_sLastRequest;
};

void CRouteTimeout::RunJob()
{
	CRouteRepliesMod *pMod = (CRouteRepliesMod *) m_pModule;
	pMod->Timeout();
}

MODULEDEFS(CRouteRepliesMod, "Send replies (e.g. to /who) to the right client only")
