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

#include <znc/User.h>
#include <znc/znc.h>

class CSocketSorter {
public:
	CSocketSorter(Csock* p) {
		m_pSock = p;
	}
	bool operator<(const CSocketSorter& other) const {
		// The 'biggest' item is displayed first.
		// return false: this is first
		// return true: other is first

		// Listeners go to the top
		if (m_pSock->GetType() != other.m_pSock->GetType()) {
			if (m_pSock->GetType() == Csock::LISTENER)
				return false;
			if (other.m_pSock->GetType() == Csock::LISTENER)
				return true;
		}
		const CString& sMyName = m_pSock->GetSockName();
		const CString& sMyName2 = sMyName.Token(1, true, "::");
		bool bMyEmpty = sMyName2.empty();
		const CString& sHisName = other.GetSock()->GetSockName();
		const CString& sHisName2 = sHisName.Token(1, true, "::");
		bool bHisEmpty = sHisName2.empty();

		// Then sort by first token after "::"
		if (bMyEmpty && !bHisEmpty)
			return false;
		if (bHisEmpty && !bMyEmpty)
			return true;

		if (!bMyEmpty && !bHisEmpty) {
			int c = sMyName2.StrCmp(sHisName2);
			if (c < 0)
				return false;
			if (c > 0)
				return true;
		}
		// and finally sort by the whole socket name
		return sMyName.StrCmp(sHisName) > 0;
	}
	Csock* GetSock() const { return m_pSock; }
private:
	Csock* m_pSock;
};

class CListSockets : public CModule {
public:
	MODCONSTRUCTOR(CListSockets) {
		AddHelpCommand();
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CListSockets::OnListCommand), "[-n]", "Show the list of active sockets. Pass -n to show IP addresses");
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override
	{
#ifndef MOD_LISTSOCKETS_ALLOW_EVERYONE
		if (!GetUser()->IsAdmin()) {
			sMessage = "You must be admin to use this module";
			return false;
		}
#endif

		return true;
	}

	std::priority_queue<CSocketSorter> GetSockets() {
		CSockManager& m = CZNC::Get().GetManager();
		std::priority_queue<CSocketSorter> ret;

		for (unsigned int a = 0; a < m.size(); a++) {
			Csock* pSock = m[a];
			// These sockets went through SwapSockByAddr. That means
			// another socket took over the connection from this
			// socket. So ignore this to avoid listing the
			// connection twice.
			if (pSock->GetCloseType() == Csock::CLT_DEREFERENCE)
				continue;
			ret.push(pSock);
		}

		return ret;
	}

	bool WebRequiresAdmin() override { return true; }
	CString GetWebMenuTitle() override { return "List sockets"; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName == "index") {
			if (CZNC::Get().GetManager().empty()) {
				return false;
			}

			std::priority_queue<CSocketSorter> socks = GetSockets();

			while (!socks.empty()) {
				Csock* pSocket = socks.top().GetSock();
				socks.pop();

				CTemplate& Row = Tmpl.AddRow("SocketsLoop");
				Row["Name"] = pSocket->GetSockName();
				Row["Created"] = GetCreatedTime(pSocket);
				Row["State"] = GetSocketState(pSocket);
				Row["SSL"] = pSocket->GetSSL() ? "Yes" : "No";
				Row["Local"] = GetLocalHost(pSocket, true);
				Row["Remote"] = GetRemoteHost(pSocket, true);
				Row["In"] = CString::ToByteStr(pSocket->GetBytesRead());
				Row["Out"] = CString::ToByteStr(pSocket->GetBytesWritten());
			}

			return true;
		}

		return false;
	}

	void OnListCommand(const CString& sLine) {
		CString sArg = sLine.Token(1, true);

		bool bShowHosts = true;
		if (sArg.Equals("-n")) {
			bShowHosts = false;
		}
		ShowSocks(bShowHosts);
	}

	CString GetSocketState(Csock* pSocket) {
		switch (pSocket->GetType()) {
			case Csock::LISTENER:
				return "Listener";
			case Csock::INBOUND:
				return "Inbound";
			case Csock::OUTBOUND:
				if (pSocket->IsConnected())
					return "Outbound";
				else
					return "Connecting";
		}

		return "UNKNOWN";
	}

	CString GetCreatedTime(Csock* pSocket) {
		unsigned long long iStartTime = pSocket->GetStartTime();
		time_t iTime = iStartTime / 1000;
		return CUtils::FormatTime(iTime, "%Y-%m-%d %H:%M:%S", GetUser()->GetTimezone());
	}

	CString GetLocalHost(Csock* pSocket, bool bShowHosts) {
		CString sBindHost;

		if (bShowHosts) {
			sBindHost = pSocket->GetBindHost();
		}

		if (sBindHost.empty()) {
			sBindHost = pSocket->GetLocalIP();
		}

		return sBindHost + " " + CString(pSocket->GetLocalPort());
	}

	CString GetRemoteHost(Csock* pSocket, bool bShowHosts) {
		CString sHost;
		u_short uPort;

		if (!bShowHosts) {
			sHost = pSocket->GetRemoteIP();
		}

		// While connecting, there might be no ip available
		if (sHost.empty()) {
			sHost = pSocket->GetHostName();
		}

		// While connecting, GetRemotePort() would return 0
		if (pSocket->GetType() == Csock::OUTBOUND) {
			uPort = pSocket->GetPort();
		} else {
			uPort = pSocket->GetRemotePort();
		}

		if (uPort != 0) {
			return sHost + " " + CString(uPort);
		}

		return sHost;
	}

	void ShowSocks(bool bShowHosts) {
		if (CZNC::Get().GetManager().empty()) {
			PutStatus("You have no open sockets.");
			return;
		}

		std::priority_queue<CSocketSorter> socks = GetSockets();

		CTable Table;
		Table.AddColumn("Name");
		Table.AddColumn("Created");
		Table.AddColumn("State");
#ifdef HAVE_LIBSSL
		Table.AddColumn("SSL");
#endif
		Table.AddColumn("Local");
		Table.AddColumn("Remote");
		Table.AddColumn("In");
		Table.AddColumn("Out");

		while (!socks.empty()) {
			Csock* pSocket = socks.top().GetSock();
			socks.pop();

			Table.AddRow();
			Table.SetCell("Name", pSocket->GetSockName());
			Table.SetCell("Created", GetCreatedTime(pSocket));
			Table.SetCell("State", GetSocketState(pSocket));

#ifdef HAVE_LIBSSL
			Table.SetCell("SSL", pSocket->GetSSL() ? "Yes" : "No");
#endif

			Table.SetCell("Local", GetLocalHost(pSocket, bShowHosts));
			Table.SetCell("Remote", GetRemoteHost(pSocket, bShowHosts));
			Table.SetCell("In", CString::ToByteStr(pSocket->GetBytesRead()));
			Table.SetCell("Out", CString::ToByteStr(pSocket->GetBytesWritten()));
		}

		PutModule(Table);
		return;
	}

	virtual ~CListSockets() {
	}

};

template<> void TModInfo<CListSockets>(CModInfo& Info) {
	Info.SetWikiPage("listsockets");
}

USERMODULEDEFS(CListSockets, "List active sockets")

