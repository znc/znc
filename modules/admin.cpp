#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "znc.h"
#include "HTTPSock.h"
#include "Server.h"

class CAdminMod;

class CAdminSock : public CHTTPSock {
public:

	CAdminSock(CAdminMod* pModule);
	CAdminSock(CAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CAdminSock();

	bool OnPageRequest(const CString& sURI, CString& sPageRet);

	virtual bool OnLogin(const CString& sUser, const CString& sPass);

	CString Header(const CString& sTitle) {
		return "<html>\r\n<head><title>" + sTitle + "</title></head>\r\n"
			"<body bgcolor='#CCCCCC' text='#000000' link='#000000' alink='#000000' vlink='#000000'>\r\n"
			"<center><h2>" + sTitle + "</h2></center><hr>\r\n";
	}

	CString Footer() {
		return "</body>\r\n</html>\r\n";
	}

	void PrintMainPage(CString& sPageRet) {
		sPageRet = Header("Main Page");
		sPageRet += "<a href='/adduser'>Add a user</a><br>\r\n"
			"<a href='/listusers'>List all users</a><br>\r\n";
		sPageRet += Footer();
	}

	void GetErrorPage(CString& sPageRet, const CString& sError) {
		sPageRet = Header("Error");
		sPageRet += "<h2>" + sError.Escape_n(CString::EHTML) + "</h2>\r\n";
		sPageRet += Footer();
	}

	void ListUsersPage(CString& sPageRet);
	bool UserPage(CString& sPageRet, CUser* pUser = NULL);
	bool AddNewUser(CString& sPageRet);

	void ListPage(CString& sPageRet) {
		VCString vsParams;
		const map<CString, VCString>& msvsParams = GetParams();

		sPageRet = Header("fooooooo");

		if (msvsParams.empty()) {
			sPageRet += "You passed in no params.\r\n";
		} else {
			for (map<CString, VCString>::const_iterator it = msvsParams.begin(); it != msvsParams.end(); it++) {
				sPageRet += "<h2>" + it->first + "</h2>\r\n<ul>\r\n";
				const VCString vsParams = it->second;

				for (unsigned int a = 0; a < vsParams.size(); a++) {
					sPageRet += "<li>[" + vsParams[a] + "]</li>\r\n";
				}

				sPageRet += "</ul>\r\n";
			}
		}

		sPageRet += Footer();
	}

	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);

private:
protected:
	CAdminMod*	m_pModule;
};

class CAdminMod : public CGlobalModule {
public:
	CAdminMod(void *pDLL, CZNC* pZNC, const CString& sModName) : CGlobalModule(pDLL, pZNC, sModName) {
		m_uPort = 8080;
	}

	virtual ~CAdminMod() {
		for (set<CAdminSock*>::iterator it = m_sSocks.begin(); it != m_sSocks.end(); it++) {
			m_pManager->DelSockByAddr(*it);
		}
	}

	virtual bool OnBoot() {
		return true;
	}

	virtual bool OnLoad(const CString& sArgs) {
		m_uPort = sArgs.Token(0).ToUInt();
		m_sUser = sArgs.Token(1);
		m_sPass = sArgs.Token(2);

		if (m_sPass.empty()) {
			return false;
		}

		CAdminSock* pListenSock = new CAdminSock(this);
		//pListenSock->SetPemLocation(m_pZNC->GetPemLocation());
		return m_pManager->ListenAll(m_uPort, "Admin::Listener", false, SOMAXCONN, pListenSock);
	}

	void AddSock(CAdminSock* pSock) {
		m_sSocks.insert(pSock);
	}

	void SockDestroyed(CAdminSock* pSock) {
		m_sSocks.erase(pSock);
	}

	const CString& GetUser() const { return m_sUser; }
	const CString& GetPass() const { return m_sPass; }
private:
	unsigned int		m_uPort;
	CString				m_sUser;
	CString				m_sPass;
	set<CAdminSock*>	m_sSocks;
};

bool CAdminSock::OnLogin(const CString& sUser, const CString& sPass) {
	if (GetUser() == m_pModule->GetUser() && GetPass() == m_pModule->GetPass()) {
		return true;
	}

	return false;
}

void CAdminSock::ListUsersPage(CString& sPageRet) {
	const map<CString,CUser*>& msUsers = m_pModule->GetZNC()->GetUserMap();
	sPageRet = Header("List Users");

	if (!msUsers.size()) {
		sPageRet += "There are no users defined.  Click <a href=\"/adduser\">here</a> if you would like to add one.\r\n";
	} else {
		sPageRet += "<table border='1' cellspacing='0' cellpadding='4'>\r\n"
			"\t<thead><tr><td>Action</td><td>Username</td><td>Current Server</td></tr></thead>\r\n";

		for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++) {
			CServer* pServer = it->second->GetCurrentServer();
			sPageRet += "\t<tr>\r\n\t\t<td>[<a href=\"/edituser?user=" + it->second->GetUserName().Escape_n(CString::EURL) + "\">Edit</a>] [<a href=\"/deluser?user=" + it->second->GetUserName().Escape_n(CString::EURL) + "\">Delete</a>]</td>\r\n"
				"\t\t<td>" + it->second->GetUserName().Escape_n(CString::EHTML) + "</td>\r\n"
				"\t\t<td>" + CString((pServer) ? pServer->GetName().Escape_n(CString::EHTML) : "-N/A-") + "</td>\r\n"
				"\t</tr>";
		}

		sPageRet +="\r\n</table>\r\n";
	}

	sPageRet += Footer();
}

Csock* CAdminSock::GetSockObj(const CString& sHost, unsigned short uPort) {
	CAdminSock* pSock = new CAdminSock(m_pModule, sHost, uPort);
	pSock->SetSockName("Admin::Client");
	pSock->SetTimeout(120);
	m_pModule->AddSock(pSock);

	return pSock;
}

CAdminSock::CAdminSock(CAdminMod* pModule) : CHTTPSock() {
	m_pModule = pModule;
	m_pModule->AddSock(this);
}
CAdminSock::CAdminSock(CAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout) : CHTTPSock(sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	m_pModule->AddSock(this);
}
CAdminSock::~CAdminSock() {
	m_pModule->SockDestroyed(this);
}

bool CAdminSock::OnPageRequest(const CString& sURI, CString& sPageRet) {
	DEBUG_ONLY(cout << "Request for [" << sURI << "] ");
	if (!ForceLogin()) {
		DEBUG_ONLY(cout << "- User not logged in!" << endl);
		return false;
	}

	if (sURI == "/") {
		PrintMainPage(sPageRet);
	} else if (sURI == "/adduser") {
		if (!UserPage(sPageRet)) {
			DEBUG_ONLY(cout << "- 302 Redirect" << endl);
			return false;
		}
	} else if (sURI == "/edituser") {
		CUser* pUser = m_pModule->GetZNC()->FindUser(GetParam("user"));

		if (pUser) {
			if (!UserPage(sPageRet, pUser)) {
				DEBUG_ONLY(cout << "- 302 Redirect" << endl);
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/listusers") {
		ListUsersPage(sPageRet);
	} else if (sURI == "/deluser") {
		if (m_pModule->GetZNC()->DeleteUser(GetParam("user"))) {
			DEBUG_ONLY(cout << "- 302 Redirect" << endl);
			Redirect("/listusers");
			return false;
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/list") {
		ListPage(sPageRet);
	} else {
		DEBUG_ONLY(cout << "- 404 Not Found!" << endl);
		return false;
	}

	DEBUG_ONLY(cout << "- 200 OK!" << endl);

	return true;
}

bool CAdminSock::UserPage(CString& sPageRet, CUser* pUser) {
	if (!GetParam("submitted").ToUInt()) {
		sPageRet = Header((pUser) ? "Edit User" : "Add User");

		CString sAllowedHosts, sServers, sChans, sCTCPReplies;

		if (pUser) {
			const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
			for (set<CString>::const_iterator it = ssAllowedHosts.begin(); it != ssAllowedHosts.end(); it++) {
				sAllowedHosts += *it + "\r\n";
			}

			const vector<CServer*>& vServers = pUser->GetServers();
			for (unsigned int a = 0; a < vServers.size(); a++) {
				sServers += vServers[a]->GetName() + "\r\n";
			}

			const vector<CChan*>& vChans = pUser->GetChans();
			for (unsigned int b = 0; b < vChans.size(); b++) {
				sChans += vChans[b]->GetName() + "\r\n";
			}
			const MCString& msCTCPReplies = pUser->GetCTCPReplies();
			for (MCString::const_iterator it2 = msCTCPReplies.begin(); it2 != msCTCPReplies.end(); it2++) {
				sCTCPReplies += it2->first + " " + it2->second + "\r\n";
			}
		}

		sPageRet += "<form action='/" + CString((pUser) ? "edituser" : "adduser") + "' method='POST'>\r\n"
			"<input type='hidden' name='submitted' value='1'>\r\n"
			"<fieldset style='border: 1px solid #000; padding: 10px;'><legend style='font-size: 16px; font-weight: bold'>Authentication</legend>\r\n"
			"<div style='float: left; margin-right: 20px;'><small><b>Username:</b></small><br>\r\n";

		if (pUser) {
			sPageRet += "<input type='hidden' name='user' value='" + pUser->GetUserName().Escape_n(CString::EHTML) + "'>\r\n"
				"<input type='text' name='disuser' value='" + pUser->GetUserName().Escape_n(CString::EHTML) + "' size='32' maxlength='12' DISABLED><br>\r\n";
		} else {
			sPageRet += "<input type='text' name='user' value='' size='32' maxlength='12'><br>\r\n";
		}

		sPageRet += "<small><b>Password:</b></small><br><input type='password' name='password' size='32' maxlength='16'><br>\r\n"
			"<small><b>Confirm Password:</b></small><br><input type='password' name='password2' size='32' maxlength='16'></div>\r\n"
			"<div><small><b>Allowed IPs:</b></small><br><textarea name='allowedips' cols='40' rows='5'>" + sAllowedHosts.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"</fieldset><br>\r\n"

			"<fieldset style='border: 1px solid #000; padding: 10px;'><legend style='font-size: 16px; font-weight: bold'>IRC Information</legend>\r\n"
			"<div style='float: left; margin-right: 10px;'><small><b>Nick:</b></small><br>\r\n"
				"<input type='text' name='nick' value='" + CString((pUser) ? pUser->GetNick().Escape_n(CString::EHTML) : "") + "' size='32' maxlength='128'></div>\r\n"
			"<div style='float: left; margin-right: 10px;'><small><b>AltNick:</b></small><br>\r\n"
				"<input type='text' name='altnick' value='" + CString((pUser) ? pUser->GetAltNick().Escape_n(CString::EHTML) : "") + "' size='32' maxlength='128'></div>\r\n"
			"<div style='float: left; margin-right: 10px;'><small><b>AwaySuffix:</b></small><br>\r\n"
				"<input type='text' name='awaysuffix' value='" + CString((pUser) ? pUser->GetAwaySuffix().Escape_n(CString::EHTML) : "") + "' size='32' maxlength='128'></div>\r\n"
			"<div><small><b>StatusPrefix:</b></small><br>\r\n"
				"<input type='text' name='statusprefix' value='" + CString((pUser) ? pUser->GetStatusPrefix().Escape_n(CString::EHTML) : "*") + "' size='14' maxlength='5'></div><br>\r\n"
			"<div style='float: left; margin-right: 10px;'><small><b>Ident:</b></small><br>\r\n"
				"<input type='text' name='ident' value='" + CString((pUser) ? pUser->GetIdent().Escape_n(CString::EHTML) : "") + "' size='32' maxlength='128'></div>\r\n"
			"<div><small><b>RealName:</b></small><br>\r\n"
				"<input type='text' name='realname' value='" + CString((pUser) ? pUser->GetRealName().Escape_n(CString::EHTML) : "") + "' size='90' maxlength='256'></div><br>\r\n"
			"<small><b>VHost:</b></small><br>\r\n"
				"<input type='text' name='vhost' value='" + CString((pUser) ? pUser->GetVHost().Escape_n(CString::EHTML) : "") + "' size='128' maxlength='256'><br><br>\r\n"
			"<small><b>QuitMsg:</b></small><br>\r\n"
				"<input type='text' name='quitmsg' value='" + CString((pUser) ? pUser->GetQuitMsg().Escape_n(CString::EHTML) : "") + "' size='128' maxlength='256'><br><br>\r\n"
			"<div><small><b>Servers:</b></small><br>\r\n"
				"<textarea name='servers' cols='40' rows='5'>" + sServers.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"</fieldset><br>\r\n"

			"<fieldset style='border: 1px solid #000; padding: 10px;'><legend style='font-size: 16px; font-weight: bold'>Modules</legend>\r\n";

		set<CModInfo> ssUserMods;
		m_pModule->GetZNC()->GetModules().GetAvailableMods(ssUserMods, m_pModule->GetZNC());

		for (set<CModInfo>::iterator it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
			const CModInfo& Info = *it;
			sPageRet += "<label><input type='checkbox' name='loadmod' value='" + Info.GetName().Escape_n(CString::EHTML) + "'"
				+ CString((pUser && pUser->GetModules().FindModule(Info.GetName())) ? " CHECKED" : "") + "> " + Info.GetName().Escape_n(CString::EHTML) + "</label>"
				" <small>(" + Info.GetDescription().Escape_n(CString::EHTML) + ")</small><br>";
		}

		sPageRet += "</fieldset><br>\r\n"
			"<fieldset style='border: 1px solid #000; padding: 10px;'><legend style='font-size: 16px; font-weight: bold'>Channels</legend>\r\n"
			"<small><b>Default Modes:</b></small><br>\r\n"
				"<input type='text' name='chanmodes' value='" + CString((pUser) ? pUser->GetDefaultChanModes().Escape_n(CString::EHTML) : "") + "' size='32' maxlength='32'><br><br>\r\n"
			"<div><small><b>Channels:</b></small><br>\r\n"
				"<textarea name='channels' cols='40' rows='5'>" + sChans.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"</fieldset><br>\r\n"

			"<fieldset style='border: 1px solid #000; padding: 10px;'><legend style='font-size: 16px; font-weight: bold'>ZNC Behavior</legend>\r\n"
			"<small><b>Playback Buffer Size:</b></small><br>\r\n"
				"<input type='text' name='bufsize' value='" + CString((pUser) ? CString::ToString(pUser->GetBufferCount()) : "") + "' size='32' maxlength='9'><br><br>\r\n"
			"<small><b>Options:</b></small><br>\r\n"
				"<label><input type='checkbox' name='keepbuffer' value='1'" + CString((!pUser || pUser->KeepBuffer()) ? " CHECKED" : "") + ">Keep Buffer</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='autocycle' value='1'" + CString((!pUser || pUser->AutoCycle()) ? " CHECKED" : "") + ">Auto Cycle</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='keepnick' value='1'" + CString((!pUser || pUser->GetKeepNick()) ? " CHECKED" : "") + ">Keep Nick</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='bouncedccs' value='1'" + CString((!pUser || pUser->BounceDCCs()) ? " CHECKED" : "") + ">Bounce DCCs</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='useclientip' value='1'" + CString((pUser && pUser->UseClientIP()) ? " CHECKED" : "") + ">Use Client IP</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='denyloadmod' value='1'" + CString((pUser && pUser->DenyLoadMod()) ? " CHECKED" : "") + ">Deny LoadMod</label>&nbsp;&nbsp;\r\n"
				"<br><br>"
			"<small><b>CTCP Replies:</b></small><br><textarea name='ctcpreplies' cols='40' rows='5'>" + sCTCPReplies.Escape_n(CString::EHTML) + "</textarea>\r\n"
			"</fieldset><br>\r\n"

			"<input type='submit' value='Submit'>\r\n"
			"</form>\r\n";

		sPageRet += Footer();
		return true;
	}

	// Add User Submission
	if (!pUser) {
		if (AddNewUser(sPageRet)) {
			return true;
		}

		Redirect("/listusers");
		return false;
	}

	// Edit User Submission
	GetErrorPage(sPageRet, "Sorry... the editing of users has not been implemented yet.");
	return true;
}

bool CAdminSock::AddNewUser(CString& sPageRet) {
	CString sUsername = GetParam("user");
	if (sUsername.empty()) {
		GetErrorPage(sPageRet, "Invalid Submission [Username is required]");
		return true;
	}

	if (m_pModule->GetZNC()->FindUser(sUsername)) {
		GetErrorPage(sPageRet, "Invalid Submission [User " + sUsername.Escape_n(CString::EHTML) + " already exists]");
		return true;
	}

	CString sArg = GetParam("password");

	if (sArg != GetParam("password2")) {
		GetErrorPage(sPageRet, "Invalid Submission [Passwords do not match]");
		return true;
	}

	CUser* pNewUser = new CUser(sUsername, m_pModule->GetZNC());

	if (!sArg.empty()) {
		pNewUser->SetPass(sArg.MD5(), true);
	}

	VCString vsArgs;
	GetParam("servers").Split("\n", vsArgs);
	unsigned int a = 0;

	for (a = 0; a < vsArgs.size(); a++) {
		pNewUser->AddServer(vsArgs[a].Trim_n());
	}

	GetParam("allowedips").Split("\n", vsArgs);
	for (a = 0; a < vsArgs.size(); a++) {
		pNewUser->AddAllowedHost(vsArgs[a].Trim_n());
	}

	GetParam("ctcpreplies").Split("\n", vsArgs);
	for (a = 0; a < vsArgs.size(); a++) {
		CString sReply = vsArgs[a].TrimRight_n("\r");
		pNewUser->AddCTCPReply(sReply.Token(0).Trim_n(), sReply.Token(1, true).Trim_n());
	}

	GetParamValues("loadmod", vsArgs);
	for (a = 0; a < vsArgs.size(); a++) {
		CString sModRet;
		CString sArg = vsArgs[a].TrimRight_n("\r");
		if (!sArg.empty()) {
			pNewUser->GetModules().LoadModule(sArg, "", pNewUser, sModRet);
		}
	}

	sArg = GetParam("nick"); if (!sArg.empty()) { pNewUser->SetNick(sArg); }
	sArg = GetParam("altnick"); if (!sArg.empty()) { pNewUser->SetAltNick(sArg); }
	sArg = GetParam("awaysuffix"); if (!sArg.empty()) { pNewUser->SetAwaySuffix(sArg); }
	sArg = GetParam("statusprefix"); if (!sArg.empty()) { pNewUser->SetStatusPrefix(sArg); }
	sArg = GetParam("ident"); if (!sArg.empty()) { pNewUser->SetIdent(sArg); }
	sArg = GetParam("realname"); if (!sArg.empty()) { pNewUser->SetRealName(sArg); }
	sArg = GetParam("vhost"); if (!sArg.empty()) { pNewUser->SetVHost(sArg); }
	sArg = GetParam("quitmsg"); if (!sArg.empty()) { pNewUser->SetQuitMsg(sArg); }
	sArg = GetParam("chanmodes"); if (!sArg.empty()) { pNewUser->SetDefaultChanModes(sArg); }

	pNewUser->SetBufferCount(GetParam("bufsize").ToUInt());
	pNewUser->SetKeepBuffer(GetParam("keepbuffer").ToBool());
	pNewUser->SetBounceDCCs(GetParam("bouncedccs").ToBool());
	pNewUser->SetAutoCycle(GetParam("autocycle").ToBool());
	pNewUser->SetKeepNick(GetParam("keepnick").ToBool());
	pNewUser->SetUseClientIP(GetParam("useclientip").ToBool());
	pNewUser->SetDenyLoadMod(GetParam("denyloadmod").ToBool());

	GetParam("channels").Split("\n", vsArgs);

	for (a = 0; a < vsArgs.size(); a++) {
		pNewUser->AddChan(vsArgs[a].TrimRight_n("\r"));
	}

	CString sErr;
	if (!pNewUser->IsValid(sErr)) {
		delete pNewUser;
		GetErrorPage(sPageRet, "Invalid submission [" + sErr.Escape_n(CString::EHTML) + "]");
		return true;
	}

	m_pModule->GetZNC()->AddUser(pNewUser);

	return false;
}

GLOBALMODULEDEFS(CAdminMod, "Add, Edit, Remove users on the fly")
