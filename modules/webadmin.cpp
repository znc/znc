#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "znc.h"
#include "HTTPSock.h"
#include "Server.h"
#include "Template.h"

class CWebAdminMod;

class CWebAdminSock : public CHTTPSock {
public:
	CWebAdminSock(CWebAdminMod* pModule);
	CWebAdminSock(CWebAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CWebAdminSock();

	virtual bool OnPageRequest(const CString& sURI, CString& sPageRet);
	virtual bool OnLogin(const CString& sUser, const CString& sPass);

	CString GetSkinDir();
	void PrintPage(CString& sPageRet, const CString& sTmplName);

	void GetErrorPage(CString& sPageRet, const CString& sError) {
		m_Template["Title"] = "Error";
		m_Template["Error"] = sError;

		PrintPage(sPageRet, "Error.tmpl");
	}

	void ListUsersPage(CString& sPageRet);
	bool SettingsPage(CString& sPageRet);
	bool ChanPage(CString& sPageRet, CChan* = NULL);
	bool DelChan(CString& sPageRet);
	bool UserPage(CString& sPageRet, CUser* pUser = NULL);
	CUser* GetNewUser(CString& sPageRet, CUser* pUser);

	CString GetModArgs(const CString& sModName, bool bGlobal = false) {
		if (!bGlobal && !m_pUser) {
			return "";
		}

		CModules& Modules = (bGlobal) ? CZNC::Get().GetModules() : m_pUser->GetModules();

		for (unsigned int a = 0; a < Modules.size(); a++) {
			CModule* pModule = Modules[a];

			if (pModule->GetModName() == sModName) {
				return pModule->GetArgs();
			}
		}

		return "";
	}

	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	bool IsAdmin(bool bAllowUserAdmin = true) const { return m_bAdmin; }

private:
protected:
	CWebAdminMod*	m_pModule;
	CUser*			m_pUser;
	bool			m_bAdmin;
	CTemplate		m_Template;
};

class CWebAdminMod : public CGlobalModule {
public:
	CWebAdminMod(void *pDLL, const CString& sModName) : CGlobalModule(pDLL, sModName) {
		m_uPort = 8080;
	}

	virtual ~CWebAdminMod() {
		while (m_spSocks.size()) {							// Loop through the sockets that we have created
			m_pManager->DelSockByAddr(*m_spSocks.begin());	// Delete each one which will call SockDestroyed() and erase it from the set
		}													// This way we don't want to erase it ourselves, that's why we're using the funky while loop

		m_spSocks.clear();
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

		if (!sPort.empty()) {
			m_uPort = sPort.ToUInt();
		}

		CWebAdminSock* pListenSock = new CWebAdminSock(this);
#ifdef HAVE_LIBSSL
		if (bSSL) {
			pListenSock->SetPemLocation(CZNC::Get().GetPemLocation());
		}
#endif

		return m_pManager->ListenHost(m_uPort, "WebAdmin::Listener", CZNC::Get().GetListenHost(), bSSL, SOMAXCONN, pListenSock);
	}

	void AddSock(CWebAdminSock* pSock) {
		m_spSocks.insert(pSock);
	}

	void SockDestroyed(CWebAdminSock* pSock) {
		m_spSocks.erase(pSock);
	}

private:
	unsigned int		m_uPort;
	set<CWebAdminSock*>	m_spSocks;
};
 
CString CWebAdminSock::GetSkinDir() {
	CString sModPath = CZNC::Get().FindModPath(m_pModule->GetModName());	// @todo store the path to the module at load time and store it in a member var which can be used here

	while (!sModPath.empty() && sModPath.Right(1) != "/") {
		sModPath.RightChomp();
	}

	return sModPath + "/" + m_pModule->GetModName() + "/skins/default/";
}

void CWebAdminSock::PrintPage(CString& sPageRet, const CString& sTmplName) {
	sPageRet.clear();
	// @todo possibly standardize the location of meta files such as these skins
	// @todo give an option for changing the current skin from 'default'
	if (!m_Template.SetFile(GetSkinDir() + sTmplName)) {
		return;
	}

	stringstream oStr;
	m_Template.Print(oStr);

	sPageRet = oStr.str();
}

bool CWebAdminSock::OnLogin(const CString& sUser, const CString& sPass) {
	CUser* pUser = CZNC::Get().FindUser(GetUser());

	if (pUser) {
		CString sHost = GetRemoteIP();

		if (pUser->IsHostAllowed(sHost) && pUser->CheckPass(GetPass())) {
			if (pUser->IsAdmin()) {
				m_bAdmin = true;
			} else {
				m_pUser = pUser;
			}

			return true;
		}
	}

	return false;
}

void CWebAdminSock::ListUsersPage(CString& sPageRet) {
	const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
	m_Template["Title"] = "List Users";

	unsigned int a = 0;

	for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++, a++) {
		CServer* pServer = it->second->GetCurrentServer();
		CTemplate& l = m_Template.AddRow("UserLoop");

		l["Username"] = it->second->GetUserName();

		if (pServer) {
			l["Server"] = pServer->GetName();
		}
	}

	PrintPage(sPageRet, "ListUsers.tmpl");
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
	SetDocRoot(GetSkinDir());
}

CWebAdminSock::CWebAdminSock(CWebAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout) : CHTTPSock(sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	m_pUser = NULL;
	m_bAdmin = false;
	m_pModule->AddSock(this);
	SetDocRoot(GetSkinDir());
}

CWebAdminSock::~CWebAdminSock() {
	m_pModule->SockDestroyed(this);
}

bool CWebAdminSock::OnPageRequest(const CString& sURI, CString& sPageRet) {
	/*if (!ForceLogin()) {
		return false;
	}*/

	m_Template["SessionUser"] = GetUser();
	m_Template["SessionIP"] = GetRemoteIP();
	m_Template["Tag"] = CZNC::GetTag();

	if (IsAdmin()) {
		m_Template["IsAdmin"] = "true";
	}

	if (sURI == "/") {
		if (!IsAdmin()) {
			Redirect("/edituser");
			return false;
		}

		m_Template["Title"] = "Main Page";
		PrintPage(sPageRet, "Main.tmpl");
	} else if (sURI.Left(5).CaseCmp("/css/") == 0) {
		PrintFile(GetSkinDir() + sURI, "text/css");
		return false;
	} else if (sURI == "/settings") {
		if (!IsAdmin()) {
			return false;
		}

		if (!SettingsPage(sPageRet)) {
			return false;
		}
	} else if (sURI == "/adduser") {
		if (!IsAdmin()) {
			return false;
		}

		if (!UserPage(sPageRet)) {
			return false;
		}
	} else if (sURI == "/edituser") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!UserPage(sPageRet, m_pUser)) {
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/editchan") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (!m_pUser) {
			GetErrorPage(sPageRet, "No such username");
			return true;
		}

		CChan* pChan = m_pUser->FindChan(GetParam("chan"));
		if (!pChan) {
			GetErrorPage(sPageRet, "No such channel");
			return true;
		}

		if (!ChanPage(sPageRet, pChan)) {
			return false;
		}
	} else if (sURI == "/addchan") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!ChanPage(sPageRet)) {
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/delchan") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!DelChan(sPageRet)) {
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/favicon.ico") {
		CString sIcon = "AAABAAIAICAAAAAAAACoCAAAJgAAABAQAAAAAAAAaAUAAM4IAAAoAAAAIAAAAEAAAAABAAgAAAAAAAAEAAAAAAAAAAAAAAABAAAAAQAAAAAAAAAAgAAAgAAAAICAAIAAAACAAIAAgIAAAMDA"
			"wADA3MAA8MqmAAQEBAAICAgADAwMABEREQAWFhYAHBwcACIiIgApKSkAVVVVAE1NTQBCQkIAOTk5AIB8/wBQUP8AkwDWAP/szADG1u8A1ufnAJCprQAAADMAAABmAAAAmQAAAMwAADMAAAAz"
			"MwAAM2YAADOZAAAzzAAAM/8AAGYAAABmMwAAZmYAAGaZAABmzAAAZv8AAJkAAACZMwAAmWYAAJmZAACZzAAAmf8AAMwAAADMMwAAzGYAAMyZAADMzAAAzP8AAP9mAAD/mQAA/8wAMwAAADMA"
			"MwAzAGYAMwCZADMAzAAzAP8AMzMAADMzMwAzM2YAMzOZADMzzAAzM/8AM2YAADNmMwAzZmYAM2aZADNmzAAzZv8AM5kAADOZMwAzmWYAM5mZADOZzAAzmf8AM8wAADPMMwAzzGYAM8yZADPM"
			"zAAzzP8AM/8zADP/ZgAz/5kAM//MADP//wBmAAAAZgAzAGYAZgBmAJkAZgDMAGYA/wBmMwAAZjMzAGYzZgBmM5kAZjPMAGYz/wBmZgAAZmYzAGZmZgBmZpkAZmbMAGaZAABmmTMAZplmAGaZ"
			"mQBmmcwAZpn/AGbMAABmzDMAZsyZAGbMzABmzP8AZv8AAGb/MwBm/5kAZv/MAMwA/wD/AMwAmZkAAJkzmQCZAJkAmQDMAJkAAACZMzMAmQBmAJkzzACZAP8AmWYAAJlmMwCZM2YAmWaZAJlm"
			"zACZM/8AmZkzAJmZZgCZmZkAmZnMAJmZ/wCZzAAAmcwzAGbMZgCZzJkAmczMAJnM/wCZ/wAAmf8zAJnMZgCZ/5kAmf/MAJn//wDMAAAAmQAzAMwAZgDMAJkAzADMAJkzAADMMzMAzDNmAMwz"
			"mQDMM8wAzDP/AMxmAADMZjMAmWZmAMxmmQDMZswAmWb/AMyZAADMmTMAzJlmAMyZmQDMmcwAzJn/AMzMAADMzDMAzMxmAMzMmQDMzMwAzMz/AMz/AADM/zMAmf9mAMz/mQDM/8wAzP//AMwA"
			"MwD/AGYA/wCZAMwzAAD/MzMA/zNmAP8zmQD/M8wA/zP/AP9mAAD/ZjMAzGZmAP9mmQD/ZswAzGb/AP+ZAAD/mTMA/5lmAP+ZmQD/mcwA/5n/AP/MAAD/zDMA/8xmAP/MmQD/zMwA/8z/AP//"
			"MwDM/2YA//+ZAP//zABmZv8AZv9mAGb//wD/ZmYA/2b/AP//ZgAhAKUAX19fAHd3dwCGhoYAlpaWAMvLywCysrIA19fXAN3d3QDj4+MA6urqAPHx8QD4+PgA8Pv/AKSgoACAgIAAAAD/AAD/"
			"AAAA//8A/wAAAP8A/wD//wAA////AP///////////////////////////////////////////////////////////////////////////////////////////wAAAAAAAAAAAAAAAAAAAAAA"
			"AAAAAAD///////////8AeXl5eXl5eXl5eXl5eXl5eXl5eXl5eQD/////////AHkKCgoKCgoKCgoKCgoKCgoKCgoKCgoKeQD//////wB5CgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKeQD/////"
			"AHkKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgp5AP////8AeQoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCnkA//////8AeQoKCgoKCgoKCgoKCgoKCgoKCgoKCgp5AP////////8AeQoKCgoKCgp5"
			"eXl5eXl5eXl5eXl5eQD///////////8AeQoKCgoKCgp5AAAAAAAAAAAAAAAA//////////////8AeQoKCgoKCgp5AP////////////////////////////8AeQoKCgoKCgp5AP//////////"
			"//////////////////8AeQoKCgoKCgp5AP////////////////////////////8AeQoKCgoKCgp5AP////////////////////////////8AeQoKCgoKCgp5AP//////////////////////"
			"//////8AeQoKCgoKCgp5AP////////////////////////////8AeQoKCgoKCgp5AP////////////////////////////8AeQoKCgoKCgp5AP////////////////////////////8AeQoK"
			"CgoKCgp5AP////////////////////////////8AeQoKCgoKCgp5AP//////////////AAAAAAAAAAAAAAAAeQoKCgoKCgp5AP///////////wB5eXl5eXl5eXl5eXl5eQoKCgoKCgp5AP//"
			"//////8AeQoKCgoKCgoKCgoKCgoKCgoKCgoKCgp5AP//////AHkKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgp5AP////8AeQoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCnkA/////wB5CgoKCgoK"
			"CgoKCgoKCgoKCgoKCgoKCgoKeQD//////wB5CgoKCgoKCgoKCgoKCgoKCgoKCgoKCnkA/////////wB5eXl5eXl5eXl5eXl5eXl5eXl5eXl5AP///////////wAAAAAAAAAAAAAAAAAAAAAA"
			"AAAAAAD///////////////////////////////////////////////////////////////////////////////////////////8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACgAAAAQAAAAIAAAAAEA"
			"CAAAAAAAAAEAAAAAAAAAAAAAAAEAAAABAAAAAAAAAACAAACAAAAAgIAAgAAAAIAAgACAgAAAwMDAAMDcwADwyqYABAQEAAgICAAMDAwAERERABYWFgAcHBwAIiIiACkpKQBVVVUATU1NAEJC"
			"QgA5OTkAgHz/AFBQ/wCTANYA/+zMAMbW7wDW5+cAkKmtAAAAMwAAAGYAAACZAAAAzAAAMwAAADMzAAAzZgAAM5kAADPMAAAz/wAAZgAAAGYzAABmZgAAZpkAAGbMAABm/wAAmQAAAJkzAACZ"
			"ZgAAmZkAAJnMAACZ/wAAzAAAAMwzAADMZgAAzJkAAMzMAADM/wAA/2YAAP+ZAAD/zAAzAAAAMwAzADMAZgAzAJkAMwDMADMA/wAzMwAAMzMzADMzZgAzM5kAMzPMADMz/wAzZgAAM2YzADNm"
			"ZgAzZpkAM2bMADNm/wAzmQAAM5kzADOZZgAzmZkAM5nMADOZ/wAzzAAAM8wzADPMZgAzzJkAM8zMADPM/wAz/zMAM/9mADP/mQAz/8wAM///AGYAAABmADMAZgBmAGYAmQBmAMwAZgD/AGYz"
			"AABmMzMAZjNmAGYzmQBmM8wAZjP/AGZmAABmZjMAZmZmAGZmmQBmZswAZpkAAGaZMwBmmWYAZpmZAGaZzABmmf8AZswAAGbMMwBmzJkAZszMAGbM/wBm/wAAZv8zAGb/mQBm/8wAzAD/AP8A"
			"zACZmQAAmTOZAJkAmQCZAMwAmQAAAJkzMwCZAGYAmTPMAJkA/wCZZgAAmWYzAJkzZgCZZpkAmWbMAJkz/wCZmTMAmZlmAJmZmQCZmcwAmZn/AJnMAACZzDMAZsxmAJnMmQCZzMwAmcz/AJn/"
			"AACZ/zMAmcxmAJn/mQCZ/8wAmf//AMwAAACZADMAzABmAMwAmQDMAMwAmTMAAMwzMwDMM2YAzDOZAMwzzADMM/8AzGYAAMxmMwCZZmYAzGaZAMxmzACZZv8AzJkAAMyZMwDMmWYAzJmZAMyZ"
			"zADMmf8AzMwAAMzMMwDMzGYAzMyZAMzMzADMzP8AzP8AAMz/MwCZ/2YAzP+ZAMz/zADM//8AzAAzAP8AZgD/AJkAzDMAAP8zMwD/M2YA/zOZAP8zzAD/M/8A/2YAAP9mMwDMZmYA/2aZAP9m"
			"zADMZv8A/5kAAP+ZMwD/mWYA/5mZAP+ZzAD/mf8A/8wAAP/MMwD/zGYA/8yZAP/MzAD/zP8A//8zAMz/ZgD//5kA///MAGZm/wBm/2YAZv//AP9mZgD/Zv8A//9mACEApQBfX18Ad3d3AIaG"
			"hgCWlpYAy8vLALKysgDX19cA3d3dAOPj4wDq6uoA8fHxAPj4+ADw+/8ApKCgAICAgAAAAP8AAP8AAAD//wD/AAAA/wD/AP//AAD///8ADw8PDw8PDw8PDw8PDw8PDw95eXl5eXl5eXl5eXl5"
			"eQ8PeQoKCgoKCgoKCgoKCnkPD3kKCgoKCgoKCgoKCgp5Dw95CgoKCgoKCgoKCgoKeQ8AD3kKCgoKCnl5eXl5eXkPAAAPeQoKCgoKeQ8PDw8PDwAAAA95CgoKCgp5DwAAAAAAAAAAD3kKCgoK"
			"CnkPAAAAAAAADw8PeQoKCgoKeQ8AAA95eXl5eXl5CgoKCgp5DwAPeQoKCgoKCgoKCgoKCnkPD3kKCgoKCgoKCgoKCgp5Dw95CgoKCgoKCgoKCgoKeQ8PeXl5eXl5eXl5eXl5eXkPDw8PDw8P"
			"Dw8PDw8PDw8PDwAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAAMAAAADgDwAA8AcAAAADAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

		SetContentType("image/x-icon");
		sIcon.Base64Decode(sPageRet);
		return true;
	} else if (sURI == "/listusers") {
		if (!IsAdmin()) {
			return false;
		}

		ListUsersPage(sPageRet);
	} else if (sURI == "/deluser") {
		if (!IsAdmin()) {
			return false;
		}

		if (CZNC::Get().DeleteUser(GetParam("user"))) {
			Redirect("/listusers");
			return false;
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else {
		return false;
	}

	return true;
}

bool CWebAdminSock::SettingsPage(CString& sPageRet) {
	if (!GetParam("submitted").ToUInt()) {
		CString sVHosts, sMotd;
		m_Template["Title"] = "Settings";
		m_Template["StatusPrefix"] = CZNC::Get().GetStatusPrefix();
		m_Template["ISpoofFile"] = CZNC::Get().GetISpoofFile();
		m_Template["ISpoofFormat"] = CZNC::Get().GetISpoofFormat();

		const VCString& vsVHosts = CZNC::Get().GetVHosts();
		for (unsigned int a = 0; a < vsVHosts.size(); a++) {
			CTemplate& l = m_Template.AddRow("VHostLoop");
			l["VHost"] = vsVHosts[a];
		}

		const VCString& vsMotd = CZNC::Get().GetMotd();
		for (unsigned int b = 0; b < vsMotd.size(); b++) {
			CTemplate& l = m_Template.AddRow("MOTDLoop");
			l["Line"] = vsMotd[b];
		}

		set<CModInfo> ssGlobalMods;
		CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, true);

		for (set<CModInfo>::iterator it = ssGlobalMods.begin(); it != ssGlobalMods.end(); it++) {
			const CModInfo& Info = *it;
			CTemplate& l = m_Template.AddRow("ModuleLoop");

			if (CZNC::Get().GetModules().FindModule(Info.GetName())) {
				l["Checked"] = "true";
			}

			if (Info.GetName() == m_pModule->GetModName()) {
				l["Disabled"] = "true";
			}

			l["Name"] = Info.GetName();
			l["Description"] = Info.GetDescription();
			l["Args"] = GetModArgs(Info.GetName(), true);
		}

		PrintPage(sPageRet, "Settings.tmpl");
		return true;
	}

	CString sArg;
	sArg = GetParam("statusprefix"); CZNC::Get().SetStatusPrefix(sArg);
	sArg = GetParam("ispooffile"); CZNC::Get().SetISpoofFile(sArg);
	sArg = GetParam("ispoofformat"); CZNC::Get().SetISpoofFormat(sArg);
	//sArg = GetParam(""); if (!sArg.empty()) { CZNC::Get().Set(sArg); }

	VCString vsArgs;
	GetParam("motd").Split("\n", vsArgs);
	CZNC::Get().ClearMotd();

	unsigned int a = 0;
	for (a = 0; a < vsArgs.size(); a++) {
		CZNC::Get().AddMotd(vsArgs[a].TrimRight_n());
	}

	GetParam("vhosts").Split("\n", vsArgs);
	CZNC::Get().ClearVHosts();

	for (a = 0; a < vsArgs.size(); a++) {
		CZNC::Get().AddVHost(vsArgs[a].Trim_n());
	}

	set<CString> ssArgs;
	GetParamValues("loadmod", ssArgs);

	for (set<CString>::iterator it = ssArgs.begin(); it != ssArgs.end(); it++) {
		CString sModRet;
		CString sModName = (*it).TrimRight_n("\r");

		if (!sModName.empty()) {
			CString sArgs = GetParam("modargs_" + sModName);

			try {
				if (!CZNC::Get().GetModules().FindModule(sModName)) {
					if (!CZNC::Get().GetModules().LoadModule(sModName, sArgs, NULL, sModRet)) {
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

	const CModules& vCurMods = CZNC::Get().GetModules();
	set<CString> ssUnloadMods;

	for (a = 0; a < vCurMods.size(); a++) {
		CModule* pCurMod = vCurMods[a];

		if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() && pCurMod->GetModName() != m_pModule->GetModName()) {
			ssUnloadMods.insert(pCurMod->GetModName());
		}
	}

	for (set<CString>::iterator it2 = ssUnloadMods.begin(); it2 != ssUnloadMods.end(); it2++) {
		CZNC::Get().GetModules().UnloadModule(*it2);
	}

	if (!CZNC::Get().WriteConfig()) {
		GetErrorPage(sPageRet, "Settings changed, but config was not written");
		return true;
	}

	Redirect("/");
	return false;
}

bool CWebAdminSock::ChanPage(CString& sPageRet, CChan* pChan) {
	if (!m_pUser) {
		GetErrorPage(sPageRet, "That user doesn't exist");
		return true;
	}

	if (!GetParam("submitted").ToUInt()) {
		m_Template["User"] = m_pUser->GetUserName();

		if (pChan) {
			m_Template["Edit"] = "true";
			m_Template["Title"] = "Edit Channel" + CString(" [" + pChan->GetName() + "]");
			m_Template["ChanName"] = pChan->GetName();
			m_Template["BufferCount"] = CString::ToString(pChan->GetBufferCount());
			m_Template["DefModes"] = pChan->GetDefaultModes();

			if (pChan->InConfig()) {
				m_Template["InConfig"] = "true";
			}
		} else {
			m_Template["Title"] = "Add Channel" + CString(" for User [" + m_pUser->GetUserName() + "]");
			m_Template["BufferCount"] = "50";
			m_Template["DefModes"] = "+stn";
			m_Template["InConfig"] = "true";
		}

		CTemplate& o1 = m_Template.AddRow("OptionLoop");
		o1["Name"] = "autocycle";
		o1["DisplayName"] = "Auto Cycle";
		if (!pChan || pChan->AutoCycle()) { o1["Checked"] = "true"; }

		CTemplate& o2 = m_Template.AddRow("OptionLoop");
		o2["Name"] = "keepbuffer";
		o2["DisplayName"] = "Keep Buffer";
		if (!pChan || pChan->KeepBuffer()) { o2["Checked"] = "true"; }

		CTemplate& o3 = m_Template.AddRow("OptionLoop");
		o3["Name"] = "detached";
		o3["DisplayName"] = "Detached";
		if (pChan && pChan->IsDetached()) { o3["Checked"] = "true"; }

		PrintPage(sPageRet, "Channel.tmpl");
		return true;
	}

	CString sChanName = GetParam("name").Trim_n();

	if (!pChan) {
		if (sChanName.empty()) {
			GetErrorPage(sPageRet, "Channel name is a required argument");
			return true;
		}

		if (m_pUser->FindChan(sChanName.Token(0))) {
			GetErrorPage(sPageRet, "Channel [" + sChanName.Token(0) + "] already exists");
			return true;
		}

		pChan = new CChan(sChanName, m_pUser, true);
		m_pUser->AddChan(pChan);
	}

	pChan->SetDefaultModes(GetParam("defmodes"));
	pChan->SetBufferCount(GetParam("buffercount").ToUInt());
	pChan->SetInConfig(GetParam("save").ToBool());
	pChan->SetAutoCycle(GetParam("autocycle").ToBool());
	pChan->SetKeepBuffer(GetParam("keepbuffer").ToBool());

	bool bDetached = GetParam("detached").ToBool();

	if (pChan->IsDetached() != bDetached) {
		if (bDetached) {
			pChan->DetachUser();
		} else {
			pChan->AttachUser();
		}
	}

	m_pUser->JoinChans();

	if (!CZNC::Get().WriteConfig()) {
		GetErrorPage(sPageRet, "Channel added/modified, but config was not written");
		return true;
	}

	Redirect("/edituser?user=" + m_pUser->GetUserName().Escape_n(CString::EURL));
	return false;
}

bool CWebAdminSock::DelChan(CString& sPageRet) {
	CString sChan = GetParam("chan");

	if (!m_pUser) {
		GetErrorPage(sPageRet, "That user doesn't exist");
		return true;
	}

	if (sChan.empty()) {
		GetErrorPage(sPageRet, "That channel doesn't exist for this user");
		return true;
	}

	m_pUser->DelChan(sChan);
	m_pUser->PutIRC("PART " + sChan);

	if (!CZNC::Get().WriteConfig()) {
		GetErrorPage(sPageRet, "Channel deleted, but config was not written");
		return true;
	}

	Redirect("/edituser?user=" + m_pUser->GetUserName().Escape_n(CString::EURL));
	return false;
}

bool CWebAdminSock::UserPage(CString& sPageRet, CUser* pUser) {
	if (!GetParam("submitted").ToUInt()) {
		CString sAllowedHosts, sServers, sChans, sCTCPReplies;

		if (pUser) {
			m_Template["Title"] = "Edit User [" + pUser->GetUserName() + "]";
			m_Template["Edit"] = "true";
			m_Template["Username"] = pUser->GetUserName();
			m_Template["Nick"] = pUser->GetNick();
			m_Template["AltNick"] = pUser->GetAltNick();
			m_Template["AwaySuffix"] = pUser->GetAwaySuffix();
			m_Template["StatusPrefix"] = pUser->GetStatusPrefix();
			m_Template["Ident"] = pUser->GetIdent();
			m_Template["RealName"] = pUser->GetRealName();
			m_Template["QuitMsg"] = pUser->GetQuitMsg();
			m_Template["DefaultChanModes"] = pUser->GetDefaultChanModes();
			m_Template["BufferCount"] = CString::ToString(pUser->GetBufferCount());

			const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
			for (set<CString>::const_iterator it = ssAllowedHosts.begin(); it != ssAllowedHosts.end(); it++) {
				CTemplate& l = m_Template.AddRow("AllowedHostLoop");
				l["Host"] = *it;
			}

			const vector<CServer*>& vServers = pUser->GetServers();
			for (unsigned int a = 0; a < vServers.size(); a++) {
				CTemplate& l = m_Template.AddRow("ServerLoop");
				l["Server"] = vServers[a]->GetString();
			}

			const MCString& msCTCPReplies = pUser->GetCTCPReplies();
			for (MCString::const_iterator it2 = msCTCPReplies.begin(); it2 != msCTCPReplies.end(); it2++) {
				CTemplate& l = m_Template.AddRow("CTCPLoop");
				l["CTCP"] = it2->first + " " + it2->second;
			}

			if (pUser == CZNC::Get().FindUser(GetUser())) {
				CString sIP = GetRemoteIP();

				if (!sIP.empty()) {
					m_Template["OwnIP"] = sIP.Token(0, false, ".") + "." + sIP.Token(1, false, ".") + "." + sIP.Token(2, false, ".") + ".*";
				}
			}

			const VCString& vsVHosts = CZNC::Get().GetVHosts();
			for (unsigned int b = 0; b < vsVHosts.size(); b++) {
				const CString& sVHost = vsVHosts[b];
				CTemplate& l = m_Template.AddRow("VHostLoop");

				l["VHost"] = sVHost;

				if (pUser && pUser->GetVHost() == sVHost) {
					l["Checked"] = "true";
				}
			}

			const vector<CChan*>& Channels = pUser->GetChans();
			for (unsigned int c = 0; c < Channels.size(); c++) {
				CChan* pChan = Channels[c];
				CTemplate& l = m_Template.AddRow("ChannelLoop");

				l["Username"] = pUser->GetUserName();
				l["Name"] = pChan->GetName();
				l["Perms"] = pChan->GetPermStr();
				l["CurModes"] = pChan->GetModeString();
				l["DefModes"] = pChan->GetDefaultModes();
				l["BufferCount"] = CString::ToString(pChan->GetBufferCount());
				l["Options"] = pChan->GetOptions();

				if (pChan->InConfig()) {
					l["InConfig"] = "true";
				}
			}
		} else {
			m_Template["Title"] = "Add User";
			m_Template["StatusPrefix"] = "*";
		}

		set<CModInfo> ssUserMods;
		CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

		for (set<CModInfo>::iterator it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
			const CModInfo& Info = *it;
			CTemplate& l = m_Template.AddRow("ModuleLoop");

			l["Name"] = Info.GetName();
			l["Description"] = Info.GetDescription();
			l["Args"] = GetModArgs(Info.GetName());

			if (pUser && pUser->GetModules().FindModule(Info.GetName())) {
				l["Checked"] = "true";
			}

			if (!IsAdmin() && pUser && pUser->DenyLoadMod()) {
				l["Disabled"] = "true";
			}
		}

		CTemplate& o1 = m_Template.AddRow("OptionLoop");
		o1["Name"] = "keepbuffer";
		o1["DisplayName"] = "Keep Buffer";
		if (!pUser || pUser->KeepBuffer()) { o1["Checked"] = "true"; }

		CTemplate& o2 = m_Template.AddRow("OptionLoop");
		o2["Name"] = "autocycle";
		o2["DisplayName"] = "Auto Cycle";
		if (!pUser || pUser->AutoCycle()) { o2["Checked"] = "true"; }

		CTemplate& o3 = m_Template.AddRow("OptionLoop");
		o3["Name"] = "keepnick";
		o3["DisplayName"] = "Keep Nick";
		if (!pUser || pUser->GetKeepNick()) { o3["Checked"] = "true"; }

		CTemplate& o4 = m_Template.AddRow("OptionLoop");
		o4["Name"] = "multiclients";
		o4["DisplayName"] = "Multi Clients";
		if (!pUser || pUser->MultiClients()) { o4["Checked"] = "true"; }

		CTemplate& o5 = m_Template.AddRow("OptionLoop");
		o5["Name"] = "bouncedccs";
		o5["DisplayName"] = "Bounce DCCs";
		if (!pUser || pUser->BounceDCCs()) { o5["Checked"] = "true"; }

		CTemplate& o6 = m_Template.AddRow("OptionLoop");
		o6["Name"] = "useclientip";
		o6["DisplayName"] = "Use Client IP";
		if (pUser && pUser->UseClientIP()) { o6["Checked"] = "true"; }

		if (IsAdmin()) {
			CTemplate& o7 = m_Template.AddRow("OptionLoop");
			o7["Name"] = "denyloadmod";
			o7["DisplayName"] = "Deny LoadMod";
			if (pUser && pUser->DenyLoadMod()) { o7["Checked"] = "true"; }

			CTemplate& o8 = m_Template.AddRow("OptionLoop");
			o8["Name"] = "isadmin";
			o8["DisplayName"] = "Admin";
			if (pUser && pUser->IsAdmin()) { o8["Checked"] = "true"; }
			if (pUser && pUser == CZNC::Get().FindUser(GetUser())) { o8["Disabled"] = "true"; }
		}

		PrintPage(sPageRet, "UserPage.tmpl");
		return true;
	}

	CString sUsername = GetParam("user");
	if (!pUser && CZNC::Get().FindUser(sUsername)) {
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

		CZNC::Get().AddUser(pNewUser);
		if (!CZNC::Get().WriteConfig()) {
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
		if (!CZNC::Get().WriteConfig()) {
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

	CUser* pNewUser = new CUser(sUsername);

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
	if (vsArgs.size()) {
		for (a = 0; a < vsArgs.size(); a++) {
			pNewUser->AddAllowedHost(vsArgs[a].Trim_n());
		}
	} else {
		pNewUser->AddAllowedHost("*");
	}

	if (HasParam("ownip")) {
		pNewUser->AddAllowedHost(GetParam("ownip"));
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
					if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet, (pUser != NULL))) {
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
				if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet, (pUser != NULL))) {
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
	pNewUser->SetMultiClients(GetParam("multiclients").ToBool());
	pNewUser->SetBounceDCCs(GetParam("bouncedccs").ToBool());
	pNewUser->SetAutoCycle(GetParam("autocycle").ToBool());
	pNewUser->SetKeepNick(GetParam("keepnick").ToBool());
	pNewUser->SetUseClientIP(GetParam("useclientip").ToBool());

	if (IsAdmin()) {
		pNewUser->SetDenyLoadMod(GetParam("denyloadmod").ToBool());
	} else if (pUser) {
		pNewUser->SetDenyLoadMod(pUser->DenyLoadMod());
	}

	if (pUser && pUser != CZNC::Get().FindUser(GetUser())) {
		pNewUser->SetAdmin(GetParam("isadmin").ToBool());
	} else if (pUser) {
		pNewUser->SetAdmin(pUser->IsAdmin());
	}

	GetParamValues("channel", vsArgs);
	for (a = 0; a < vsArgs.size(); a++) {
		const CString& sChan = vsArgs[a];
		pNewUser->AddChan(sChan.TrimRight_n("\r"), GetParam("save_" + sChan).ToBool());
	}

	return pNewUser;
}

GLOBALMODULEDEFS(CWebAdminMod, "Dynamic configuration of users/settings through a web browser")
