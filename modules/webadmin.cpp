#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "znc.h"
#include "HTTPSock.h"
#include "Server.h"

class CWebAdminMod;

class CWebAdminSock : public CHTTPSock {
public:
	CWebAdminSock(CWebAdminMod* pModule);
	CWebAdminSock(CWebAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CWebAdminSock();

	virtual bool OnPageRequest(const CString& sURI, CString& sPageRet);
	virtual bool OnLogin(const CString& sUser, const CString& sPass);

	CString Header(const CString& sTitle);
	CString Footer();

	void PrintMainPage(CString& sPageRet) {
		sPageRet = Header("Main Page");
		sPageRet += "Welcome to the ZNC webadmin module.\r\n";
		sPageRet += Footer();
	}

	void GetErrorPage(CString& sPageRet, const CString& sError) {
		sPageRet = Header("Error");
		sPageRet += "<h2>" + sError.Escape_n(CString::EHTML) + "</h2>\r\n";
		sPageRet += Footer();
	}

	void ListUsersPage(CString& sPageRet);
	bool SettingsPage(CString& sPageRet);
	bool UserPage(CString& sPageRet, CUser* pUser = NULL);
	CUser* GetNewUser(CString& sPageRet, CUser* pUser);

	void ListPage(CString& sPageRet) {
		VCString vsParams;
		const map<CString, VCString>& msvsParams = GetParams();

		sPageRet = Header("fooooooo");

		if (msvsParams.empty()) {
			sPageRet += "You passed in no params.\r\n";
		} else {
			sPageRet += "foo [" + GetParamString().Escape_n(CString::EHTML) + "]<br><br>";

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

	CString GetModArgs(const CString& sModName, bool bGlobal = false) {
		if (!bGlobal && !m_pUser) {
			return "";
		}

		CModules& Modules = (bGlobal) ? CZNC::New()->GetModules() : m_pUser->GetModules();

		for (unsigned int a = 0; a < Modules.size(); a++) {
			CModule* pModule = Modules[a];

			if (pModule->GetModName() == sModName) {
				return pModule->GetArgs();
			}
		}

		return "";
	}

	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	bool IsAdmin() const { return m_bAdmin; }

private:
protected:
	CWebAdminMod*	m_pModule;
	CUser*			m_pUser;
	bool			m_bAdmin;
};

class CWebAdminMod : public CGlobalModule {
public:
	CWebAdminMod(void *pDLL, CZNC* pZNC, const CString& sModName) : CGlobalModule(pDLL, pZNC, sModName) {
		m_uPort = 8080;
	}

	virtual ~CWebAdminMod() {
		for (set<CWebAdminSock*>::iterator it = m_sSocks.begin(); it != m_sSocks.end(); it++) {
			m_pManager->DelSockByAddr(*it);
		}
	}

	virtual bool OnBoot() {
		return true;
	}

	virtual bool OnLoad(const CString& sArgs) {
		bool bSSL = false;
		CString sPort = sArgs.Token(0);

		if (sPort.Left(1) == "+") {
#ifdef HAVE_LIBSSL
			sPort.TrimLeft("+");
			bSSL = true;
#else
			return false;
#endif
		}

		m_uPort = sPort.ToUInt();
		m_sUser = sArgs.Token(1);
		m_sPass = sArgs.Token(2);

		if (m_sPass.empty()) {
			return false;
		}

		CWebAdminSock* pListenSock = new CWebAdminSock(this);

#ifdef HAVE_LIBSSL
		if (bSSL) {
			pListenSock->SetPemLocation(m_pZNC->GetPemLocation());
		}
#endif

		return m_pManager->ListenAll(m_uPort, "WebAdmin::Listener", bSSL, SOMAXCONN, pListenSock);
	}

	void AddSock(CWebAdminSock* pSock) {
		m_sSocks.insert(pSock);
	}

	void SockDestroyed(CWebAdminSock* pSock) {
		m_sSocks.erase(pSock);
	}

	const CString& GetUser() const { return m_sUser; }
	const CString& GetPass() const { return m_sPass; }
private:
	unsigned int		m_uPort;
	CString				m_sUser;
	CString				m_sPass;
	set<CWebAdminSock*>	m_sSocks;
};

CString CWebAdminSock::Header(const CString& sTitle) {
	CString sRet = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html>\r\n<head><title>ZNC - " + sTitle.Escape_n(CString::EHTML) + "</title></head>\r\n"
		"<body bgcolor='#FFFFFF' text='#000000' link='#000000' alink='#000000' vlink='#000000'>\r\n"
		"<table border='0' cellpadding='10' cellspacing='0' height='100%' width='100%'>\r\n"
		"<tr><td style='border-bottom: 2px solid #000;' colspan='2' align='center' valign='top'><h2>" + sTitle.Escape_n(CString::EHTML) + "</h2></td></tr>\r\n"
		"<tr><td style='white-space: nowrap; border-right: 1px solid #000;' valign='top'>\r\n";

	if (IsAdmin()) {
		sRet += "[<a href='/'>Home</a>]<br>\r\n"
			"[<a href='/settings'>Settings</a>]<br>\r\n"
			"[<a href='/adduser'>Add User</a>]<br>\r\n"
			"[<a href='/listusers'>List Users</a>]<br>\r\n";
	}

	sRet += "</td><td height='100%' width='100%' valign='top'>\r\n";

	return sRet;
}

CString CWebAdminSock::Footer() {
	return "</td></tr><tr><td colspan='2' align='right' valign='bottom'>" + m_pModule->GetZNC()->GetTag() + "</td></tr>\r\n"
		"</table></body>\r\n</html>\r\n";
}

bool CWebAdminSock::OnLogin(const CString& sUser, const CString& sPass) {
	if (GetUser() == m_pModule->GetUser() && GetPass() == m_pModule->GetPass()) {
		m_bAdmin = true;
		return true;
	}

	CUser* pUser = m_pModule->GetZNC()->FindUser(GetUser());

	if (pUser && pUser->CheckPass(GetPass())) {
		m_pUser = pUser;
		return true;
	}

	return false;
}

void CWebAdminSock::ListUsersPage(CString& sPageRet) {
	const map<CString,CUser*>& msUsers = m_pModule->GetZNC()->GetUserMap();
	sPageRet = Header("List Users");

	if (!msUsers.size()) {
		sPageRet += "There are no users defined.  Click <a href=\"/adduser\">here</a> if you would like to add one.\r\n";
	} else {
		sPageRet += "<table style='border: 1px solid #000;' cellspacing='0' cellpadding='4'>\r\n"
			"\t<thead><tr bgcolor='#FFFF99'><td style='border: 1px solid #000;'><b>Action</b></td><td style='border: 1px solid #000;'><b>Username</b></td><td style='border: 1px solid #000;'><b>Current Server</b></td></tr></thead>\r\n";

		unsigned int a = 0;
	
		for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++, a++) {
			CServer* pServer = it->second->GetCurrentServer();
			sPageRet += "\t<tr bgcolor='" + CString((a %2) ? "#FFFFCC" : "#CCCC99") + "'>\r\n\t\t<td style='border: 1px solid #000;'>[<a href=\"/edituser?user=" + it->second->GetUserName().Escape_n(CString::EURL) + "\">Edit</a>] [<a href=\"/deluser?user=" + it->second->GetUserName().Escape_n(CString::EURL) + "\">Delete</a>]</td>\r\n"
				"\t\t<td style='border: 1px solid #000;'>" + it->second->GetUserName().Escape_n(CString::EHTML) + "</td>\r\n"
				"\t\t<td style='border: 1px solid #000;'>" + CString((pServer) ? pServer->GetName().Escape_n(CString::EHTML) : "-N/A-") + "</td>\r\n"
				"\t</tr>";
		}

		sPageRet +="\r\n</table>\r\n";
	}

	sPageRet += Footer();
}

Csock* CWebAdminSock::GetSockObj(const CString& sHost, unsigned short uPort) {
	CWebAdminSock* pSock = new CWebAdminSock(m_pModule, sHost, uPort);
	pSock->SetSockName("WebAdmin::Client");
	pSock->SetTimeout(120);
	m_pModule->AddSock(pSock);

	return pSock;
}

CWebAdminSock::CWebAdminSock(CWebAdminMod* pModule) : CHTTPSock() {
	m_pModule = pModule;
	m_pUser = NULL;
	m_bAdmin = false;
	m_pModule->AddSock(this);
}
CWebAdminSock::CWebAdminSock(CWebAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout) : CHTTPSock(sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	m_pUser = NULL;
	m_bAdmin = false;
	m_pModule->AddSock(this);
}
CWebAdminSock::~CWebAdminSock() {
	m_pModule->SockDestroyed(this);
}

bool CWebAdminSock::OnPageRequest(const CString& sURI, CString& sPageRet) {
	DEBUG_ONLY(cout << "Request for [" << sURI << "] ");
	if (!ForceLogin()) {
		DEBUG_ONLY(cout << "- User not logged in!" << endl);
		return false;
	}

	if (sURI == "/") {
		if (!IsAdmin()) {
			Redirect("/edituser");
			return false;
		}

		PrintMainPage(sPageRet);
	} else if (sURI == "/settings") {
		if (!IsAdmin()) {
			return false;
		}

		if (!SettingsPage(sPageRet)) {
			DEBUG_ONLY(cout << "- 302 Redirect" << endl);
			return false;
		}
	} else if (sURI == "/adduser") {
		if (!IsAdmin()) {
			return false;
		}

		if (!UserPage(sPageRet)) {
			DEBUG_ONLY(cout << "- 302 Redirect" << endl);
			return false;
		}
	} else if (sURI == "/edituser") {
		if (!m_pUser) {
			m_pUser = m_pModule->GetZNC()->FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!UserPage(sPageRet, m_pUser)) {
				DEBUG_ONLY(cout << "- 302 Redirect" << endl);
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/listusers") {
		if (!IsAdmin()) {
			return false;
		}

		ListUsersPage(sPageRet);
	} else if (sURI == "/deluser") {
		if (!IsAdmin()) {
			return false;
		}

		if (m_pModule->GetZNC()->DeleteUser(GetParam("user"))) {
			DEBUG_ONLY(cout << "- 302 Redirect" << endl);
			Redirect("/listusers");
			return false;
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	//} else if (sURI == "/list") {
	//	ListPage(sPageRet);
	} else {
		DEBUG_ONLY(cout << "- 404 Not Found!" << endl);
		return false;
	}

	DEBUG_ONLY(cout << "- 200 OK!" << endl);

	return true;
}

bool CWebAdminSock::SettingsPage(CString& sPageRet) {
	if (!GetParam("submitted").ToUInt()) {
		sPageRet = Header("Settings");

		CString sVHosts;

		const VCString& vsVHosts = m_pModule->GetZNC()->GetVHosts();
		for (unsigned int a = 0; a < vsVHosts.size(); a++) {
			sVHosts += vsVHosts[a] + "\r\n";
		}

		sPageRet += "<br><form action='/settings' method='POST'>\r\n"
			"<input type='hidden' name='submitted' value='1'>\r\n"
			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>Global Settings</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"

			"<div><small><b>Status Prefix:</b></small><br>\r\n"
				"<input type='text' name='statusprefix' value='" + m_pModule->GetZNC()->GetStatusPrefix().Escape_n(CString::EHTML) + "' size='32' maxlength='128'></div><br>\r\n"

			"<div style='float: left; margin-right: 10px;'><small><b>ISpoofFile:</b></small><br>\r\n"
				"<input type='text' name='ispooffile' value='" + m_pModule->GetZNC()->GetISpoofFile().Escape_n(CString::EHTML) + "' size='32' maxlength='128'></div>\r\n"
			"<div><small><b>ISpoofFormat:</b></small><br>\r\n"
				"<input type='text' name='ispoofformat' value='" + m_pModule->GetZNC()->GetISpoofFormat().Escape_n(CString::EHTML) + "' size='32' maxlength='128'></div>\r\n"

			"<br><div><small><b>VHosts:</b></small><br>\r\n"
				"<textarea name='vhosts' cols='40' rows='5'>" + sVHosts.Escape_n(CString::EHTML) + "</textarea></div>\r\n"

			"<br></div></div><br><br>\r\n"

			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>Global Modules</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"
			"<table cellspacing='0' cellpadding='3' style='border: 1px solid #000;'>\r\n"
			"<tr style='font-weight: bold; background: #ff9;'><td style='border: 1px solid #000;'>Name</td><td style='border: 1px solid #000;'>Arguments</td><td style='border: 1px solid #000;'>Description</td></tr>\r\n";

		set<CModInfo> ssGlobalMods;
		m_pModule->GetZNC()->GetModules().GetAvailableMods(ssGlobalMods, m_pModule->GetZNC(), true);

		unsigned int uIdx = 0;

		for (set<CModInfo>::iterator it = ssGlobalMods.begin(); it != ssGlobalMods.end(); it++) {
			const CModInfo& Info = *it;
			sPageRet += "<tr style='background: " + CString((uIdx++ %2) ? "#ffc" : "#cc9") + "'><td style='border: 1px solid #000;'><label><input type='checkbox' name='loadmod' value='" + Info.GetName().Escape_n(CString::EHTML) + "'" + CString((m_pModule->GetZNC()->GetModules().FindModule(Info.GetName())) ? " CHECKED" : "") + CString((Info.GetName() == m_pModule->GetModName()) ? " DISABLED" : "") + "> " + Info.GetName().Escape_n(CString::EHTML) + "</label></td>"
				"<td style='border: 1px solid #000;'><input type='text' name='modargs_" + Info.GetName().Escape_n(CString::EHTML) + "' value='" + GetModArgs(Info.GetName(), true) + "'" + CString((Info.GetName() == m_pModule->GetModName()) ? " DISABLED" : "") + "></td>"
				"<td style='border: 1px solid #000;'>" + Info.GetDescription().Escape_n(CString::EHTML) + "</td></tr>";
		}

		sPageRet += "</table><br></div></div><br><br>\r\n"
			"<br></div></div><br><br>\r\n"
			"<input type='submit' value='Submit'>\r\n"
			"</form>\r\n";

		sPageRet += Footer();
		return true;
	}

	CString sArg;
	sArg = GetParam("statusprefix"); m_pModule->GetZNC()->SetStatusPrefix(sArg);
	sArg = GetParam("ispooffile"); m_pModule->GetZNC()->SetISpoofFile(sArg);
	sArg = GetParam("ispoofformat"); m_pModule->GetZNC()->SetISpoofFormat(sArg);
	//sArg = GetParam(""); if (!sArg.empty()) { m_pModule->GetZNC()->Set(sArg); }

	VCString vsArgs = GetParam("vhosts").Split("\n");
	m_pModule->GetZNC()->ClearVHosts();

	unsigned int a = 0;
	for (a = 0; a < vsArgs.size(); a++) {
		m_pModule->GetZNC()->AddVHost(vsArgs[a].Trim_n());
	}

	set<CString> ssArgs;
	GetParamValues("loadmod", ssArgs);

	for (set<CString>::iterator it = ssArgs.begin(); it != ssArgs.end(); it++) {
		CString sModRet;
		CString sModName = (*it).TrimRight_n("\r");

		if (!sModName.empty()) {
			CString sArgs = GetParam("modargs_" + sModName);

			try {
				if (!m_pModule->GetZNC()->GetModules().FindModule(sModName)) {
					if (!m_pModule->GetZNC()->GetModules().LoadModule(sModName, sArgs, NULL, sModRet)) {
						DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "] [" << sModRet << "]" << endl);
					}
				} else {
					DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "] because it is already loaded" << endl);
				}
			} catch(...) {
				DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "] [" << sArgs << "]" << endl);
			}
		}
	}

	const CModules& vCurMods = m_pModule->GetZNC()->GetModules();
	set<CString> ssUnloadMods;

	for (a = 0; a < vCurMods.size(); a++) {
		CModule* pCurMod = vCurMods[a];

		if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() && pCurMod->GetModName() != m_pModule->GetModName()) {
			ssUnloadMods.insert(pCurMod->GetModName());
		}
	}

	for (set<CString>::iterator it2 = ssUnloadMods.begin(); it2 != ssUnloadMods.end(); it2++) {
		m_pModule->GetZNC()->GetModules().UnloadModule(*it2);
	}

	if (!m_pModule->GetZNC()->WriteConfig()) {
		GetErrorPage(sPageRet, "Settings changed, but config was not written");
		return true;
	}

	Redirect("/");
	return false;
}

bool CWebAdminSock::UserPage(CString& sPageRet, CUser* pUser) {
	if (!GetParam("submitted").ToUInt()) {
		sPageRet = Header((pUser) ? CString("Edit User [" + pUser->GetUserName() + "]") : CString("Add User"));

		CString sAllowedHosts, sServers, sChans, sCTCPReplies;

		if (pUser) {
			const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
			for (set<CString>::const_iterator it = ssAllowedHosts.begin(); it != ssAllowedHosts.end(); it++) {
				sAllowedHosts += *it + "\r\n";
			}

			const vector<CServer*>& vServers = pUser->GetServers();
			for (unsigned int a = 0; a < vServers.size(); a++) {
				sServers += vServers[a]->GetString() + "\r\n";
			}

			const vector<CChan*>& vChans = pUser->GetChans();
			for (unsigned int b = 0; b < vChans.size(); b++) {
				CChan* pChan = vChans[b];

				if (pChan->InConfig()) {
					sChans += vChans[b]->GetName() + "\r\n";
				}
			}
			const MCString& msCTCPReplies = pUser->GetCTCPReplies();
			for (MCString::const_iterator it2 = msCTCPReplies.begin(); it2 != msCTCPReplies.end(); it2++) {
				sCTCPReplies += it2->first + " " + it2->second + "\r\n";
			}
		}

		sPageRet += "<br><form action='/" + CString((pUser) ? "edituser" : "adduser") + "' method='POST'>\r\n";

		sPageRet += "<input type='hidden' name='submitted' value='1'>\r\n"
			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>Authentication</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"
			"<div style='float: left; margin-right: 20px;'><small><b>Username:</b></small><br>\r\n";

		if (pUser) {
			sPageRet += "<input type='hidden' name='user' value='" + pUser->GetUserName().Escape_n(CString::EHTML) + "'>\r\n"
				"<input type='text' name='newuser' value='" + pUser->GetUserName().Escape_n(CString::EHTML) + "' size='32' maxlength='12' DISABLED><br>\r\n";
		} else {
			sPageRet += "<input type='text' name='user' value='' size='32' maxlength='12'><br>\r\n";
		}

		sPageRet += "<small><b>Password:</b></small><br><input type='password' name='password' size='32' maxlength='16'><br>\r\n"
			"<small><b>Confirm Password:</b></small><br><input type='password' name='password2' size='32' maxlength='16'></div>\r\n"
			"<div><small><b>Allowed IPs:</b></small><br><textarea name='allowedips' cols='40' rows='5'>" + sAllowedHosts.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"<br></div></div><br><br>\r\n"

			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>IRC Information</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"
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
				"<input type='text' name='realname' value='" + CString((pUser) ? pUser->GetRealName().Escape_n(CString::EHTML) : "") + "' size='90' maxlength='256'></div><br>\r\n";

		const VCString& vsVHosts = m_pModule->GetZNC()->GetVHosts();

		if (vsVHosts.size()) {
			sPageRet += "<small><b>VHost:</b></small><br>\r\n"
				"<select name='vhost'>\r\n<option value=''>- None -</option>\r\n";

			for (unsigned int a = 0; a < vsVHosts.size(); a++) {
				sPageRet += "<option value='" + vsVHosts[a].Escape_n(CString::EHTML) + "'" + CString((pUser && pUser->GetVHost() == vsVHosts[a]) ? " SELECTED" : "") + ">" + vsVHosts[a].Escape_n(CString::EHTML) + "</option>\r\n";
			}

			sPageRet += "</select><br><br>\r\n";
		}

		sPageRet += "<small><b>QuitMsg:</b></small><br>\r\n"
				"<input type='text' name='quitmsg' value='" + CString((pUser) ? pUser->GetQuitMsg().Escape_n(CString::EHTML) : "") + "' size='128' maxlength='256'><br><br>\r\n"
			"<div><small><b>Servers:</b></small><br>\r\n"
				"<textarea name='servers' cols='40' rows='5'>" + sServers.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"<br></div></div><br><br>\r\n"

			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>Modules</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"
			"<table cellspacing='0' cellpadding='3' style='border: 1px solid #000;'>\r\n"
			"<tr style='font-weight: bold; background: #ff9;'><td style='border: 1px solid #000;'>Name</td><td style='border: 1px solid #000;'>Arguments</td><td style='border: 1px solid #000;'>Description</td></tr>\r\n";

		set<CModInfo> ssUserMods;
		m_pModule->GetZNC()->GetModules().GetAvailableMods(ssUserMods, m_pModule->GetZNC());

		unsigned int uIdx = 0;

		for (set<CModInfo>::iterator it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
			const CModInfo& Info = *it;
			sPageRet += "<tr style='background: " + CString((uIdx++ %2) ? "#ffc" : "#cc9") + "'><td style='border: 1px solid #000;'><label><input type='checkbox' name='loadmod' value='" + Info.GetName().Escape_n(CString::EHTML) + "'" + CString((pUser && pUser->GetModules().FindModule(Info.GetName())) ? " CHECKED" : "") + CString((!IsAdmin() && pUser && pUser->DenyLoadMod()) ? " DISABLED" : "") + "> " + Info.GetName().Escape_n(CString::EHTML) + "</label></td>"
				"<td style='border: 1px solid #000;'><input type='text' name='modargs_" + Info.GetName().Escape_n(CString::EHTML) + "' value='" + GetModArgs(Info.GetName()) + "'></td>"
				"<td style='border: 1px solid #000;'>" + Info.GetDescription().Escape_n(CString::EHTML) + "</td></tr>";
		}

		sPageRet += "</table><br></div></div><br><br>\r\n"
			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>Channels</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"
			"<small><b>Default Modes:</b></small><br>\r\n"
				"<input type='text' name='chanmodes' value='" + CString((pUser) ? pUser->GetDefaultChanModes().Escape_n(CString::EHTML) : "") + "' size='32' maxlength='32'><br><br>\r\n"
			"<div><small><b>Channels:</b></small><br>\r\n"
				"<textarea name='channels' cols='40' rows='5'>" + sChans.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"<br></div></div><br><br>\r\n"

			"<div style='white-space: nowrap; margin-top: -8px; margin-right: 8px; margin-left: 8px; padding: 1px 5px 1px 5px; float: left; border: 1px solid #000; font-size: 16px; font-weight: bold; background: #ff9;'>ZNC Behavior</div><div style='padding: 25px 5px 5px 15px; border: 2px solid #000; background: #cc9;'><div style='clear: both;'>\r\n"
			"<small><b>Playback Buffer Size:</b></small><br>\r\n"
				"<input type='text' name='bufsize' value='" + CString((pUser) ? CString::ToString(pUser->GetBufferCount()) : "") + "' size='32' maxlength='9'><br><br>\r\n"
			"<small><b>Options:</b></small><br>\r\n"
				"<label><input type='checkbox' name='keepbuffer' value='1'" + CString((!pUser || pUser->KeepBuffer()) ? " CHECKED" : "") + ">Keep Buffer</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='autocycle' value='1'" + CString((!pUser || pUser->AutoCycle()) ? " CHECKED" : "") + ">Auto Cycle</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='keepnick' value='1'" + CString((!pUser || pUser->GetKeepNick()) ? " CHECKED" : "") + ">Keep Nick</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='bouncedccs' value='1'" + CString((!pUser || pUser->BounceDCCs()) ? " CHECKED" : "") + ">Bounce DCCs</label>&nbsp;&nbsp;\r\n"
				"<label><input type='checkbox' name='useclientip' value='1'" + CString((pUser && pUser->UseClientIP()) ? " CHECKED" : "") + ">Use Client IP</label>&nbsp;&nbsp;\r\n";

		if (IsAdmin()) {
			sPageRet += "<label><input type='checkbox' name='denyloadmod' value='1'" + CString((pUser && pUser->DenyLoadMod()) ? " CHECKED" : "") + ">Deny LoadMod</label>&nbsp;&nbsp;\r\n";
		}

		sPageRet += "<br><br>"
			"<div><small><b>CTCP Replies:</b></small><br>"
				"<textarea name='ctcpreplies' cols='40' rows='5'>" + sCTCPReplies.Escape_n(CString::EHTML) + "</textarea></div>\r\n"
			"<br></div></div><br><br>\r\n"

			"<input type='submit' value='Submit'>\r\n"
			"</form>\r\n";

		sPageRet += Footer();
		return true;
	}

	CString sUsername = GetParam("user");
	if (!pUser && m_pModule->GetZNC()->FindUser(sUsername)) {
		GetErrorPage(sPageRet, "Invalid Submission [User " + sUsername + " already exists]");
		return true;
	}

	CUser* pNewUser = GetNewUser(sPageRet, pUser);
	if (!pNewUser) {
		return true;
	}

	CString sErr;

	if (!pUser) {
		// Add User Submission
		if (!pNewUser->IsValid(sErr)) {
			delete pNewUser;
			GetErrorPage(sPageRet, "Invalid submission [" + sErr + "]");
			return true;
		}

		m_pModule->GetZNC()->AddUser(pNewUser);
		if (!m_pModule->GetZNC()->WriteConfig()) {
			GetErrorPage(sPageRet, "User added, but config was not written");
			return true;
		}
	} else {
		// Edit User Submission
		if (!pUser->Clone(*pNewUser, sErr)) {
			delete pNewUser;
			GetErrorPage(sPageRet, "Invalid Submission [" + sErr + "]");
			return true;
		}

		delete pNewUser;
		if (!m_pModule->GetZNC()->WriteConfig()) {
			GetErrorPage(sPageRet, "User edited, but config was not written");
			return true;
		}
	}

	if (!IsAdmin()) {
		Redirect("/edituser");
	} else {
		Redirect("/listusers");
	}

	return false;
}

CUser* CWebAdminSock::GetNewUser(CString& sPageRet, CUser* pUser) {
	CString sUsername = GetParam("newuser");

	if (sUsername.empty()) {
		sUsername = GetParam("user");
	}

	if (sUsername.empty()) {
		GetErrorPage(sPageRet, "Invalid Submission [Username is required]");
		return NULL;
	}

	CString sArg = GetParam("password");

	if (sArg != GetParam("password2")) {
		GetErrorPage(sPageRet, "Invalid Submission [Passwords do not match]");
		return NULL;
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

	if (IsAdmin() || (pUser && !pUser->DenyLoadMod())) {
		GetParamValues("loadmod", vsArgs);

		for (a = 0; a < vsArgs.size(); a++) {
			CString sModRet;
			CString sModName = vsArgs[a].TrimRight_n("\r");

			if (!sModName.empty()) {
				CString sArgs = GetParam("modargs_" + sModName);

				try {
					if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet)) {
						DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "] [" << sModRet << "]" << endl);
					}
				} catch (...) {
					DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "] [" << sArgs << "]" << endl);
				}
			}
		}
	} else if (pUser) {
		CModules& Modules = pUser->GetModules();

		for (a = 0; a < Modules.size(); a++) {
			CString sModName = Modules[a]->GetModName();
			CString sArgs = Modules[a]->GetArgs();
			CString sModRet;

			try {
				if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet)) {
					DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "] [" << sModRet << "]" << endl);
				}
			} catch (...) {
				DEBUG_ONLY(cerr << "Unable to load module [" << sModName << "]" << endl);
			}
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

	if (IsAdmin()) {
		pNewUser->SetDenyLoadMod(GetParam("denyloadmod").ToBool());
	} else if (pUser) {
		pNewUser->SetDenyLoadMod(pUser->DenyLoadMod());
	}

	GetParam("channels").Split("\n", vsArgs);

	for (a = 0; a < vsArgs.size(); a++) {
		pNewUser->AddChan(vsArgs[a].TrimRight_n("\r"), true);
	}

	return pNewUser;
}

GLOBALMODULEDEFS(CWebAdminMod, "Dynamic configuration of users/settings through a web browser")
