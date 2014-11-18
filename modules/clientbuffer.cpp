/*
 * Copyright (C) 2004-2014  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/Client.h>
#include <znc/Utils.h>
#include <znc/User.h>
#include <znc/znc.h>
#include <sys/time.h>

static const char* ServerTimeFormat = "%Y-%m-%dT%H:%M:%S";

class CClientBufferMod : public CModule
{
public:
	MODCONSTRUCTOR(CClientBufferMod)
	{
		AddHelpCommand();
		AddCommand("AddClient", static_cast<CModCommand::ModCmdFunc>(&CClientBufferMod::OnAddClientCommand), "<identifier>", "Add a client.");
		AddCommand("DelClient", static_cast<CModCommand::ModCmdFunc>(&CClientBufferMod::OnDelClientCommand), "<identifier>", "Delete a client.");
		AddCommand("ListClients", static_cast<CModCommand::ModCmdFunc>(&CClientBufferMod::OnListClientsCommand), "", "List known clients.");
	}

	void OnAddClientCommand(const CString& line);
	void OnDelClientCommand(const CString& line);
	void OnListClientsCommand(const CString& = "");

	virtual EModRet OnSendToClient(CString& line, CClient& client) override;

	virtual EModRet OnPrivBufferPlayLine(CClient& client, CString& line) override;
	virtual EModRet OnChanBufferPlayLine(CChan& chan, CClient& client, CString& line) override;

private:
	bool AddClient(const CString& identifier);
	bool DelClient(const CString& identifier);
	bool HasClient(const CString& identifier);

	timeval GetTimestamp(const CString& identifier);
	void UpdateTimestamp(const CString& identifier);

	EModRet ProcessMessage(const CClient& client, const CString& line);
};

void CClientBufferMod::OnAddClientCommand(const CString& line)
{
	const CString identifier = line.Token(1);
	if (identifier.empty()) {
		PutModule("Usage: AddClient <identifier>");
		return;
	}
	if (HasClient(identifier)) {
		PutModule("Client already exists: " + identifier);
		return;
	}
	AddClient(identifier);
	PutModule("Client added: " + identifier);
}

void CClientBufferMod::OnDelClientCommand(const CString& line)
{
	const CString identifier = line.Token(1);
	if (identifier.empty()) {
		PutModule("Usage: DelClient <identifier>");
		return;
	}
	if (!HasClient(identifier)) {
		PutModule("Unknown client: " + identifier);
		return;
	}
	DelClient(identifier);
	PutModule("Client removed: " + identifier);
}

void CClientBufferMod::OnListClientsCommand(const CString&)
{
	const CString& current = GetClient()->GetIdentifier();

	CTable table;
	table.AddColumn("Client");
	table.AddColumn("Connected");
	table.AddColumn("Last seen message");
	for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
		table.AddRow();
		if (it->first == current)
			table.SetCell("Client",  "*" + it->first);
		else
			table.SetCell("Client",  it->first);
		table.SetCell("Connected", CString(!GetNetwork()->FindClients(it->first).empty()));
		table.SetCell("Last seen message", CUtils::FormatTime(GetTimestamp(it->first).tv_sec, ServerTimeFormat, ""));
	}

	if (table.empty())
		PutModule("No identified clients");
	else
		PutModule(table);
}

CModule::EModRet CClientBufferMod::OnSendToClient(CString& line, CClient& client)
{
	if (client.IsReady() && HasClient(client.GetIdentifier()))
		UpdateTimestamp(client.GetIdentifier());
	return CONTINUE;
}

CModule::EModRet CClientBufferMod::OnPrivBufferPlayLine(CClient& client, CString& line)
{
	return ProcessMessage(client, line);
}

CModule::EModRet CClientBufferMod::OnChanBufferPlayLine(CChan& chan, CClient& client, CString& line)
{
	return ProcessMessage(client, line);
}

bool CClientBufferMod::AddClient(const CString& identifier)
{
	return SetNV(identifier, GetNV(identifier));
}

bool CClientBufferMod::DelClient(const CString& identifier)
{
	return DelNV(identifier);
}

bool CClientBufferMod::HasClient(const CString& identifier)
{
	return !identifier.empty() && FindNV(identifier) != EndNV();
}

timeval CClientBufferMod::GetTimestamp(const CString& identifier)
{
	timeval tv;
	double timestamp = GetNV(identifier).ToDouble();
	tv.tv_sec = timestamp;
	tv.tv_usec = (timestamp - tv.tv_sec) * 1000;
	return tv;
}

void CClientBufferMod::UpdateTimestamp(const CString& identifier)
{
	timeval tv;
	gettimeofday(&tv, NULL);
	double timestamp = tv.tv_sec + tv.tv_usec / 1000.0;
	SetNV(identifier, CString(timestamp));
}

static time_t ParseTimestamp(const CString& timestamp, const CString& format)
{
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	strptime(timestamp.c_str(), format.c_str(), &tm);
	return mktime(&tm);
}

CModule::EModRet CClientBufferMod::ProcessMessage(const CClient& client, const CString& line)
{
	const CString& identifier = client.GetIdentifier();
	if (client.HasServerTime() && HasClient(identifier)) {
		const timeval tv = GetTimestamp(identifier);
		const MCString tags = CUtils::GetMessageTags(line);
		MCString::const_iterator it = tags.find("time");
		if (it != tags.end()) {
			time_t tt = ParseTimestamp(it->second, ServerTimeFormat);
			if (tv.tv_sec < tt)
				return CModule::HALT;
		}
	}
	return CModule::CONTINUE;
}

NETWORKMODULEDEFS(CClientBufferMod, "Client specific buffer playback")
