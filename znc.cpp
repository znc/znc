#include "znc.h"
#include "User.h"
#include "Server.h"
#include "Client.h"
#include "IRCSock.h"
#include "Utils.h"

#include <pwd.h>
#include <signal.h>
#include <sys/types.h>

#ifdef _MODULES
#include "Modules.h"
#endif

CZNC::CZNC() {
#ifdef _MODULES
	m_pModules = new CGlobalModules();
#endif
	m_bISpoofLocked = false;
	SetISpoofFormat(""); // Set ISpoofFormat to default
}

CZNC::~CZNC() {
#ifdef _MODULES
	m_pModules->UnloadAll();
	delete m_pModules;

	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		a->second->GetModules().UnloadAll();
	}
#endif

	for (size_t b = 0; b < m_vpListeners.size(); b++) {
		delete m_vpListeners[b];
	}

	m_Manager.Cleanup();
	DeleteUsers();
}

CString CZNC::GetTag(bool bIncludeVersion) {
	if (!bIncludeVersion) {
		return "ZNC - by prozac@rottenboy.com";
	}

	char szBuf[128];
	memset(szBuf, 0, 128);
	snprintf(szBuf, 127, "ZNC %1.3f - by prozac@rottenboy.com", VERSION);

	return szBuf;
}

bool CZNC::OnBoot() {
#ifdef _MODULES
	if (!GetModules().OnBoot()) {
		return false;
	}
#endif

	for (map<CString,CUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
		if (!it->second->OnBoot()) {
			return false;
		}
	}

	return true;
}

int CZNC::Loop() {
	m_Manager.SetSelectTimeout(10000);
	m_itUserIter = m_msUsers.begin();

	while (true) {
		// Check for users that need to be deleted
		if (m_ssDelUsers.size()) {
			for (set<CUser*>::iterator it = m_ssDelUsers.begin(); it != m_ssDelUsers.end(); it++) {
				CUser* pUser = *it;
				m_msUsers.erase(pUser->GetUserName());

				CIRCSock* pIRCSock = pUser->GetIRCSock();

				if (pIRCSock) {
					m_Manager.DelSockByAddr(pIRCSock);
				}

				pUser->DelClients();
#ifdef _MODULES
				pUser->DelModules();
#endif
				delete pUser;
			}

			m_ssDelUsers.clear();
			m_itUserIter = m_msUsers.begin();
			WriteConfig();
		}

		m_Manager.Loop();

		if (m_bISpoofLocked) {
			continue;
		}

		if (m_itUserIter == m_msUsers.end()) {
			m_itUserIter = m_msUsers.begin();
		}

		if (m_msUsers.empty()) {
			usleep(10000);
			continue;
		}

		CString sSockName = "IRC::" + m_itUserIter->first;
		CUser* pUser = m_itUserIter->second;

		m_itUserIter++;

		CIRCSock* pIRCSock = (CIRCSock*) m_Manager.FindSockByName(sSockName);

		if (!pIRCSock) {
			if (pUser->ConnectPaused() && pUser->IsLastServer()) {
				continue;
			}

			CServer* pServer = pUser->GetNextServer();

			if (!pServer) {
				continue;
			}

			if (!m_sISpoofFile.empty()) {
				CFile File(m_sISpoofFile);

				if (File.Open(O_RDONLY)) {
					char buf[1024];
					memset((char*) buf, 0, 1024);
					File.Read(buf, 1023);
					File.Close();
					m_sOrigISpoof = buf;
				}

				if (File.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
					CString sData = m_sISpoofFormat.Token(0, false, "%") + pUser->GetIdent() + m_sISpoofFormat.Token(1, true, "%");
					File.Write(sData + "\n");
					File.Close();
				}

				m_bISpoofLocked = true;
			}

			DEBUG_ONLY(cout << "User [" << pUser->GetUserName() << "] is connecting to [" << pServer->GetName() << ":" << pServer->GetPort() << "] ..." << endl);
			pUser->PutStatus("Attempting to connect to [" + pServer->GetName() + ":" + CString::ToString(pServer->GetPort()) + "] ...");

			pIRCSock = new CIRCSock(pUser);
			pIRCSock->SetPass(pServer->GetPass());

			bool bSSL = false;
			bool bIPV6 = false;
#ifdef HAVE_LIBSSL
			if (pServer->IsSSL()) {
				bSSL = true;
				pIRCSock->SetPemLocation(GetPemLocation());
			}
#endif
#ifdef HAVE_IPV6
			if (pServer->IsIPV6()) {
				bIPV6 = true;
			}
#endif
			if (!m_Manager.Connect(pServer->GetName(), pServer->GetPort(), sSockName, 20, bSSL, pUser->GetVHost(), pIRCSock, bIPV6)) {
				ReleaseISpoof();
				pUser->PutStatus("Unable to connect. (Bad host?)");
			}
		}
	}

	return 0;
}

void CZNC::ReleaseISpoof() {
	if (!m_sISpoofFile.empty()) {
		CFile File(m_sISpoofFile);

		if (File.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
			File.Write(m_sOrigISpoof);
			File.Close();
		}

		m_sOrigISpoof = "";
	}

	m_bISpoofLocked = false;
}

bool CZNC::WritePidFile(int iPid) {
	if (!m_sPidFile.empty()) {
		CFile File(m_sPidFile);
		CUtils::PrintAction("Writing pid file [" + m_sPidFile + "]");

		if (File.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
			File.Write(CString::ToString(iPid) + "\n");
			File.Close();
			CUtils::PrintStatus(true);
			return true;
		}

		CUtils::PrintStatus(false);
	}

	return false;
}

bool CZNC::WritePemFile() {
#ifndef HAVE_LIBSSL
	CUtils::PrintError("ZNC was not compiled with ssl support.");
	return false; 
#else
	CString sPemFile = GetPemLocation();
	const char* pHostName = getenv("HOSTNAME");
	CString sHost;

	if (pHostName) {
		sHost = pHostName;
	}

	if (CFile::Exists(sPemFile)) {
		CUtils::PrintError("Pem file [" + sPemFile + "] already exists");
		return false;
	}

	while (!CUtils::GetInput("hostname of your shell", sHost, sHost, "including the '.com' portion"));

	CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");
	FILE *f = fopen(sPemFile.c_str(), "w");

	if (!f) {
		CUtils::PrintStatus(false, "Unable to open");
		return false;
	}

	CUtils::GenerateCert(f, false, sHost);
	fclose(f);

	CUtils::PrintStatus(true);
	return true;
#endif
}

void CZNC::DeleteUsers() {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		delete a->second;
	}

	m_msUsers.clear();
	m_itUserIter = m_msUsers.begin();
}

CUser* CZNC::GetUser(const CString& sUser) {
	// Todo: make this case insensitive
	map<CString,CUser*>::iterator it = m_msUsers.find(sUser);
	return (it == m_msUsers.end()) ? NULL : it->second;
}

Csock* CZNC::FindSockByName(const CString& sSockName) {
	return m_Manager.FindSockByName(sSockName);
}

bool CZNC::IsHostAllowed(const CString& sHostMask) {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		if (a->second->IsHostAllowed(sHostMask)) {
			return true;
		}
	}

	return false;
}

void CZNC::InitDirs(const CString& sArgvPath) {
	char buf[PATH_MAX];
	getcwd(buf, PATH_MAX);

	// If the bin was not ran from the current directory, we need to add that dir onto our cwd
	CString::size_type uPos = sArgvPath.rfind('/');
	m_sCurPath = (uPos == CString::npos) ? CString(buf) : CUtils::ChangeDir(buf, sArgvPath.substr(0, uPos), "");

	// Try to set the user's home dir, default to binpath on failure
	struct passwd* pUserInfo = getpwuid(getuid());

	if (pUserInfo) {
		m_sHomePath = pUserInfo->pw_dir;
	}

	if (m_sHomePath.empty()) {
		m_sHomePath = m_sCurPath;
	}

	m_sZNCPath = m_sHomePath + "/.znc";

	// Other dirs that we use
	m_sConfPath = m_sZNCPath + "/configs";
	m_sConfBackupPath = m_sConfPath + "/backups";
	m_sModPath = m_sZNCPath + "/modules";
	m_sUserPath = m_sZNCPath + "/users";
}


CString CZNC::ExpandConfigPath(const CString& sConfigFile) {
	CString sRetPath;

	if (sConfigFile.empty()) {
		sRetPath = GetConfPath() + "/znc.conf";
	} else {
		if (sConfigFile.Left(2) == "./" || sConfigFile.Left(3) == "../") {
			sRetPath = GetCurPath() + "/" + sConfigFile;
		} else if (sConfigFile.Left(1) != "/") {
			sRetPath = GetConfPath() + "/" + sConfigFile;
		} else {
			sRetPath = sConfigFile;
		}
	}

	return sRetPath;
}

bool CZNC::WriteConfig() {
	CFile File(m_sConfigFile);

	if (!File.Copy(GetConfBackupPath() + "/" + File.GetShortName() + "-" + CString::ToString(time(NULL)))) {
		return false;
	}

	if (m_sConfigFile.empty() || !File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
		return false;
	}

	for (size_t l = 0; l < m_vpListeners.size(); l++) {
		CListener* pListener = m_vpListeners[l];
		CString sHostPortion = pListener->GetBindHost();

		if (!sHostPortion.empty()) {
			sHostPortion += " ";
		}

		CString s6 = (pListener->IsIPV6()) ? "6" : " ";

		File.Write("Listen" + s6 + "      = " + sHostPortion + CString((pListener->IsSSL()) ? "+" : "") + CString::ToString(pListener->GetPort()) + "\r\n");
	}

	if (!m_sISpoofFile.empty()) {
		File.Write("ISpoofFile   = " + m_sISpoofFile + "\r\n");
		if (!m_sISpoofFormat.empty()) { File.Write("ISpoofFormat = " + m_sISpoofFormat + "\r\n"); }
	}

	if (!m_sPidFile.empty()) { File.Write("PidFile      = " + m_sPidFile + "\r\n"); }
	if (!m_sStatusPrefix.empty()) { File.Write("StatusPrefix = " + m_sStatusPrefix + "\r\n"); }

	for (unsigned int m = 0; m < m_vsMotd.size(); m++) {
		File.Write("Motd         = " + m_vsMotd[m] + "\r\n");
	}

	for (unsigned int v = 0; v < m_vsVHosts.size(); v++) {
		File.Write("VHost        = " + m_vsVHosts[v] + "\r\n");
	}

#ifdef _MODULES
	CGlobalModules& Mods = GetModules();

	for (unsigned int a = 0; a < Mods.size(); a++) {
		CString sArgs = Mods[a]->GetArgs();

		if (!sArgs.empty()) {
			sArgs = " " + sArgs;
		}

		File.Write("LoadModule   = " + Mods[a]->GetModName() + sArgs + "\r\n");
	}
#endif

	for (map<CString,CUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
		CString sErr;

		if (!it->second->IsValid(sErr)) {
			DEBUG_ONLY(cerr << "** Error writing config for user [" << it->first << "] [" << sErr << "]" << endl);
			continue;
		}

		File.Write("\r\n");

		if (!it->second->WriteConfig(File)) {
			DEBUG_ONLY(cerr << "** Error writing config for user [" << it->first << "]" << endl);
		}
	}

	File.Close();

	return true;
}

bool CZNC::WriteNewConfig(const CString& sConfig) {
	CString sConfigFile = ExpandConfigPath((sConfig.empty()) ? "znc.conf" : sConfig);
	CString sAnswer, sUser;
	vector<CString> vsLines;

	if (CFile::Exists(sConfigFile)) {
		if (!m_LockFile.TryExLock(sConfigFile, 50)) {
			CUtils::PrintError("ZNC is currently running on this config.");
			return false;
		}

		if (!CUtils::GetBoolInput("This config already exists.  Would you like to overwrite it?", false)) {
			m_LockFile.UnLock();
			return false;
		}
	}

	CUtils::PrintMessage("Writing new config [" + sConfigFile + "]");

	CUtils::PrintMessage("");
	CUtils::PrintMessage("First lets start with some global settings...");
	CUtils::PrintMessage("");

	// Listen
	unsigned int uPort = 0;
	while(!CUtils::GetNumInput("What port would you like ZNC to listen on?", uPort, 1, 65535));

	CString sSSL;
#ifdef HAVE_LIBSSL
	if (CUtils::GetBoolInput("Would you like ZNC to listen using SSL?", false)) {
		sSSL = "+";
	}
#endif

	CString s6 = " ";
#ifdef HAVE_IPV6
	if (CUtils::GetBoolInput("Would you like ZNC to listen using ipv6?", false)) {
		s6 = "6";
	}
#endif

	CString sListenHost;
	CUtils::GetInput("Listen Host", sListenHost, "", "Blank for all ips");

	if (!sListenHost.empty()) {
		sListenHost += " ";
	}

	vsLines.push_back("Listen" + s6 + "    = " + sListenHost + sSSL + CString::ToString(uPort));
	// !Listen

#ifdef _MODULES
	set<CModInfo> ssGlobalMods;
	GetModules().GetAvailableMods(ssGlobalMods, true);

	if (ssGlobalMods.size()) {
		CUtils::PrintMessage("");
		CUtils::PrintMessage("-- Global Modules --");
		CUtils::PrintMessage("");

		if (CUtils::GetBoolInput("Do you want to load any global modules?")) {
			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");
			set<CModInfo>::iterator it;

			for (it = ssGlobalMods.begin(); it != ssGlobalMods.end(); it++) {
				const CModInfo& Info = *it;
				Table.AddRow();
				Table.SetCell("Name", Info.GetName());
				Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
			}

			unsigned int uTableIdx = 0; CString sLine;
			while (Table.GetLine(uTableIdx++, sLine)) {
				CUtils::PrintMessage(sLine);
			}

			CUtils::PrintMessage("");

			for (it = ssGlobalMods.begin(); it != ssGlobalMods.end(); it++) {
				const CModInfo& Info = *it;
				CString sName = Info.GetName();

				if (sName.Right(3).CaseCmp(".so") == 0) {
					sName.RightChomp(3);
				}

				if (CUtils::GetBoolInput("Load global module <\033[1m" + sName + "\033[22m>?", false)) {
					vsLines.push_back("LoadModule = " + sName);
				}
			}
		}
	}
#endif

	// User
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Now we need to setup a user...");
	CUtils::PrintMessage("");

	bool bFirstUser = true;

	do {
		vsLines.push_back("");
		CString sNick;
		do {
			CUtils::GetInput("Username", sUser, "", "AlphaNumeric");
		} while (!CUser::IsValidUserName(sUser));

		vsLines.push_back("<User " + sUser + ">");
		sAnswer = CUtils::GetHashPass();					vsLines.push_back("\tPass       = " + sAnswer + " -");

		if (CUtils::GetBoolInput("Would you like this user to be an admin?", bFirstUser)) {
			vsLines.push_back("\tAdmin      = true");
		} else {
			vsLines.push_back("\tAdmin      = false");
		}

		CUtils::GetInput("Nick", sNick, sUser);			vsLines.push_back("\tNick       = " + sNick);
		CUtils::GetInput("Alt Nick", sAnswer, sNick + "_");	if (!sAnswer.empty()) { vsLines.push_back("\tAltNick    = " + sAnswer); }
		CUtils::GetInput("Ident", sAnswer, sNick);			vsLines.push_back("\tIdent      = " + sAnswer);
		CUtils::GetInput("Real Name", sAnswer, "Got ZNC?");	vsLines.push_back("\tRealName   = " + sAnswer);
		CUtils::GetInput("VHost", sAnswer, "", "optional");	if (!sAnswer.empty()) { vsLines.push_back("\tVHost      = " + sAnswer); }
		// todo: Possibly add motd

		if (CUtils::GetBoolInput("Would you like ZNC to keep trying for your primary nick?", true)) {
			vsLines.push_back("\tKeepNick   = true");
		} else {
			vsLines.push_back("\tKeepNick   = false");
		}

		unsigned int uBufferCount = 0;

		CUtils::GetNumInput("Number of lines to buffer per channel", uBufferCount, 0, ~0, 50);	if (uBufferCount) { vsLines.push_back("\tBuffer     = " + CString::ToString(uBufferCount)); }
		if (CUtils::GetBoolInput("Would you like your buffer to be sticky?", true)) {
			vsLines.push_back("\tKeepBuffer = true");
		} else {
			vsLines.push_back("\tKeepBuffer = false");
		}

		CUtils::GetInput("Default channel modes", sAnswer, "+stn");
		if (!sAnswer.empty()) {
			vsLines.push_back("\tChanModes  = " + sAnswer);
		}

#ifdef _MODULES
		set<CModInfo> ssUserMods;
		GetModules().GetAvailableMods(ssUserMods);

		if (ssUserMods.size()) {
			vsLines.push_back("");
			CUtils::PrintMessage("");
			CUtils::PrintMessage("-- User Modules --");
			CUtils::PrintMessage("");

			if (CUtils::GetBoolInput("Do you want to automatically load any user modules for this user?")) {
				CTable Table;
				Table.AddColumn("Name");
				Table.AddColumn("Description");
				set<CModInfo>::iterator it;

				for (it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
					const CModInfo& Info = *it;
					Table.AddRow();
					Table.SetCell("Name", Info.GetName());
					Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
				}

				unsigned int uTableIdx = 0; CString sLine;
				while (Table.GetLine(uTableIdx++, sLine)) {
					CUtils::PrintMessage(sLine);
				}

				CUtils::PrintMessage("");

				for (it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
					const CModInfo& Info = *it;
					CString sName = Info.GetName();

					if (CUtils::GetBoolInput("Load " + CString((Info.IsSystem()) ? "system" : "local") + " module <\033[1m" + sName + "\033[22m>?", false)) {
						vsLines.push_back("\tLoadModule = " + sName);
					}
				}
			}
		}
#endif

		vsLines.push_back("");
		CUtils::PrintMessage("");
		CUtils::PrintMessage("-- IRC Servers --");
		CUtils::PrintMessage("");

		do {
			CString sHost, sPass;
			bool bSSL = false;
			unsigned int uPort = 0;

			while(!CUtils::GetInput("IRC server", sHost, "", "host only") || !CServer::IsValidHostName(sHost));
			while(!CUtils::GetNumInput("[" + sHost + "] Port", uPort, 1, 65535, 6667));
			CUtils::GetInput("[" + sHost + "] Password (probably empty)", sPass);

#ifdef HAVE_LIBSSL
			bSSL = CUtils::GetBoolInput("Does this server use SSL? (probably no)", false);
#endif

			vsLines.push_back("\tServer     = " + sHost + ((bSSL) ? " +" : " ") + CString::ToString(uPort) + " " + sPass);
		} while (CUtils::GetBoolInput("Would you like to add another server?", false));

		vsLines.push_back("");
		CUtils::PrintMessage("");
		CUtils::PrintMessage("-- Channels --");
		CUtils::PrintMessage("");

		CString sArg = "a";
		CString sPost = " for ZNC to automatically join?";
		bool bDefault = true;

		while (CUtils::GetBoolInput("Would you like to add " + sArg + " channel" + sPost, bDefault)) {
			while (!CUtils::GetInput("Channel name", sAnswer));
			vsLines.push_back("\t<Chan " + sAnswer + ">");
			vsLines.push_back("\t</Chan>");
			sArg = "another";
			sPost = "?";
			bDefault = false;
		}

		vsLines.push_back("</User>");

		CUtils::PrintMessage("");
		bFirstUser = false;
	} while (CUtils::GetBoolInput("Would you like to setup another user?", false));
	// !User

	CUtils::PrintAction("Writing config [" + sConfigFile + "]");
	CFile File(sConfigFile);

	bool bFileOpen = false;

	if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
		bFileOpen = true;
	} else {
		CUtils::PrintStatus(false, "Unable to open file");
		CUtils::GetInput("Alternate location", sConfigFile, "/tmp/" + sConfig);

		if (!CFile::Exists(sConfigFile) || CUtils::GetBoolInput("Would you like to overwrite the existing alt file", false)) {
			CUtils::PrintAction("Writing to alt location [" + sConfigFile + "]");
			File.SetFileName(sConfigFile);

			if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
				bFileOpen = true;
			} else {
				CUtils::PrintStatus(false, "Unable to open alt file");
			}
		}
	}

	if (!bFileOpen) {
		CUtils::PrintMessage("");
		CUtils::PrintMessage("Printing new config to stdout since we were unable to open a file");
		CUtils::PrintMessage("");
		cout << endl << "----------------------------------------------------------------------------" << endl << endl;
	}

	for (unsigned int a = 0; a < vsLines.size(); a++) {
		if (bFileOpen) {
			File.Write(vsLines[a] + "\n");
		} else {
			cout << vsLines[a] << endl;
		}
	}

	if (bFileOpen) {
		File.Close();

		CUtils::PrintStatus(true);
	} else {
		cout << endl << "----------------------------------------------------------------------------" << endl << endl;
	}

	CUtils::PrintMessage("");
	CUtils::PrintMessage("To connect to this znc you need to connect to it as your irc server", true);
	CUtils::PrintMessage("using the port that you supplied.  You have to supply your login info", true);
	CUtils::PrintMessage("as the irc server password like so.. user:pass.", true);
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Try something like this in your IRC client...", true);
	CUtils::PrintMessage("/server <znc_server_ip> " + CString::ToString(uPort) + " " + sUser + ":<pass>", true);
	CUtils::PrintMessage("");

	m_LockFile.UnLock();
	return CUtils::GetBoolInput("Launch znc now?", true);
}

bool CZNC::ParseConfig(const CString& sConfig) {
	m_sConfigFile = ExpandConfigPath(sConfig);

	CUtils::PrintAction("Opening Config [" + m_sConfigFile + "]");

	if (!CFile::Exists(m_sConfigFile)) {
		CUtils::PrintStatus(false, "No such file");
		CUtils::PrintMessage("Restart znc with the --makeconf option if you wish to create this config.");
		return false;
	}

	if (!CFile::IsReg(m_sConfigFile)) {
		CUtils::PrintStatus(false, "Not a file");
		return false;
	}

	if (!m_LockFile.TryExLock(m_sConfigFile, 50)) {
		CUtils::PrintStatus(false, "ZNC is already running on this config.");
		return false;
	}

	CFile File(m_sConfigFile);

	if (!File.Open(O_RDONLY)) {
		CUtils::PrintStatus(false);
		return false;
	}

	CUtils::PrintStatus(true);

	CString sLine;
	bool bCommented = false;	// support for /**/ style comments
	CUser* pUser = NULL;	// Used to keep track of which user block we are in
	CChan* pChan = NULL;	// Used to keep track of which chan block we are in

	while (File.ReadLine(sLine)) {
		while ((sLine.Right(1) == "\r") || (sLine.Right(1) == "\n")) {
			sLine.Trim();
		}

		if ((sLine.empty()) || (sLine[0] == '#') || (sLine.Left(2) == "//")) {
			continue;
		}

		if (sLine.Left(2) == "/*") {
			if (sLine.Right(2) != "*/") {
				bCommented = true;
			}

			continue;
		}

		if (bCommented) {
			if (sLine.Right(2) == "*/") {
				bCommented = false;
			}

			continue;
		}

		if ((sLine.Left(1) == "<") && (sLine.Right(1) == ">")) {
			sLine.LeftChomp();
			sLine.RightChomp();
			sLine.Trim();

			CString sTag = sLine.substr(0, sLine.find_first_of(" \t\r\n"));
			CString sValue = (sTag.size() < sLine.size()) ? sLine.substr(sTag.size() +1) : "";

			sTag.Trim();
			sValue.Trim();

			if (sLine.Left(1) == "/") {
				CString sTag = sLine.substr(1);

				if (pUser) {
					if (pChan) {
						if (sTag.CaseCmp("Chan") == 0) {
							pUser->AddChan(pChan);
							pChan = NULL;
							continue;
						}
					} else if (sTag.CaseCmp("User") == 0) {
						CString sErr;

						if (!pUser->IsValid(sErr)) {
							CUtils::PrintError("Invalid user [" + pUser->GetUserName() + "] " + sErr);
							return false;
						}

						AddUser(pUser);

						pUser = NULL;
						continue;
					}
				}
			} else if (sTag.CaseCmp("User") == 0) {
				if (pUser) {
					CUtils::PrintError("You may not nest <User> tags inside of other <User> tags.");
					return false;
				}

				if (sValue.empty()) {
					CUtils::PrintError("You must supply a username in the <User> tag.");
					return false;
				}

				if (m_msUsers.find(sValue) != m_msUsers.end()) {
					CUtils::PrintError("User [" + sValue + "] defined more than once.");
					return false;
				}

				pUser = new CUser(sValue);
				CUtils::PrintMessage("Loading user [" + sValue + "]");

				if (!m_sStatusPrefix.empty()) {
					if (!pUser->SetStatusPrefix(m_sStatusPrefix)) {
						CUtils::PrintError("Invalid StatusPrefix [" + m_sStatusPrefix + "] Must be 1-5 chars, no spaces.");
						return false;
					}
				}

				continue;
			} else if (sTag.CaseCmp("Chan") == 0) {
				if (!pUser) {
					CUtils::PrintError("<Chan> tags must be nested inside of a <User> tag.");
					return false;
				}

				if (pChan) {
					CUtils::PrintError("You may not nest <Chan> tags inside of other <Chan> tags.");
					return false;
				}

				pChan = new CChan(sValue, pUser, true);
				continue;
			}
		}

		// If we have a regular line, figure out where it goes
		CString sName = sLine.Token(0, false, "=");
		CString sValue = sLine.Token(1, true, "=");
		sName.Trim();
		sValue.Trim();

#ifdef _MODULES
		if (GetModules().OnConfigLine(sName, sValue, pUser, pChan)) {
			continue;
		}
#endif

		if ((!sName.empty()) && (!sValue.empty())) {
			if (pUser) {
				if (pChan) {
					if (sName.CaseCmp("Buffer") == 0) {
						pChan->SetBufferCount(strtoul(sValue.c_str(), NULL, 10));
						continue;
					} else if (sName.CaseCmp("KeepBuffer") == 0) {
						pChan->SetKeepBuffer((sValue.CaseCmp("true") == 0));
						continue;
					} else if (sName.CaseCmp("Detached") == 0) {
						pChan->SetDetached((sValue.CaseCmp("true") == 0));
						continue;
					} else if (sName.CaseCmp("AutoCycle") == 0) {
						pChan->SetAutoCycle((sValue.CaseCmp("true") == 0));
						continue;
					} else if (sName.CaseCmp("Key") == 0) {
						pChan->SetKey(sValue);
						continue;
					} else if (sName.CaseCmp("Modes") == 0) {
						pChan->SetDefaultModes(sValue);
						continue;
					}
				} else {
					if (sName.CaseCmp("Buffer") == 0) {
						pUser->SetBufferCount(strtoul(sValue.c_str(), NULL, 10));
						continue;
					} else if (sName.CaseCmp("KeepBuffer") == 0) {
						pUser->SetKeepBuffer((sValue.CaseCmp("true") == 0));
						continue;
					} else if (sName.CaseCmp("AutoCycle") == 0) {
						pUser->SetAutoCycle((sValue.CaseCmp("true") == 0));
						continue;
					} else if (sName.CaseCmp("Nick") == 0) {
						pUser->SetNick(sValue);
						continue;
					} else if (sName.CaseCmp("CTCPReply") == 0) {
						pUser->AddCTCPReply(sValue.Token(0), sValue.Token(1, true));
						continue;
					} else if (sName.CaseCmp("QuitMsg") == 0) {
						pUser->SetQuitMsg(sValue);
						continue;
					} else if (sName.CaseCmp("AltNick") == 0) {
						pUser->SetAltNick(sValue);
						continue;
					} else if (sName.CaseCmp("AwaySuffix") == 0) {
						pUser->SetAwaySuffix(sValue);
						continue;
					} else if (sName.CaseCmp("Pass") == 0) {
						if (sValue.Right(1) == "-") {
							sValue.RightChomp();
							sValue.Trim();
							pUser->SetPass(sValue, true);
						} else {
							pUser->SetPass(sValue, false);
						}

						continue;
					} else if (sName.CaseCmp("MultiClients") == 0) {
						pUser->SetMultiClients(sValue.CaseCmp("true") == 0);
						continue;
					} else if (sName.CaseCmp("BounceDCCs") == 0) {
						pUser->SetBounceDCCs(sValue.CaseCmp("true") == 0);
						continue;
					} else if (sName.CaseCmp("Ident") == 0) {
						pUser->SetIdent(sValue);
						continue;
					} else if (sName.CaseCmp("DenyLoadMod") == 0) {
						pUser->SetDenyLoadMod((sValue.CaseCmp("TRUE") == 0));
						continue;
					} else if (sName.CaseCmp("Admin") == 0) {
						pUser->SetAdmin((sValue.CaseCmp("TRUE") == 0));
						continue;
					} else if (sName.CaseCmp("StatusPrefix") == 0) {
						if (!pUser->SetStatusPrefix(sValue)) {
							CUtils::PrintError("Invalid StatusPrefix [" + sValue + "] Must be 1-5 chars, no spaces.");
							return false;
						}
						continue;
					} else if (sName.CaseCmp("DCCLookupMethod") == 0) {
						pUser->SetUseClientIP((sValue.CaseCmp("Client") == 0));
						continue;
					} else if (sName.CaseCmp("RealName") == 0) {
						pUser->SetRealName(sValue);
						continue;
					} else if (sName.CaseCmp("KeepNick") == 0) {
						pUser->SetKeepNick((sValue.CaseCmp("true") == 0));
						continue;
					} else if (sName.CaseCmp("ChanModes") == 0) {
						pUser->SetDefaultChanModes(sValue);
						continue;
					} else if (sName.CaseCmp("VHost") == 0) {
						pUser->SetVHost(sValue);
						continue;
					} else if (sName.CaseCmp("Allow") == 0) {
						pUser->AddAllowedHost(sValue);
						continue;
					} else if (sName.CaseCmp("Server") == 0) {
						CUtils::PrintAction("Adding Server [" + sValue + "]");
						CUtils::PrintStatus(pUser->AddServer(sValue));
						continue;
					} else if (sName.CaseCmp("Chan") == 0) {
						pUser->AddChan(sValue, true);
						continue;
					} else if (sName.CaseCmp("LoadModule") == 0) {
						CString sModName = sValue.Token(0);
						CUtils::PrintAction("Loading Module [" + sModName + "]");
#ifdef _MODULES
						CString sModRet;
						CString sArgs = sValue.Token(1, true);

						try {
							bool bModRet = pUser->GetModules().LoadModule(sModName, sArgs, pUser, sModRet);
							CUtils::PrintStatus(bModRet, (bModRet) ? "" : sModRet);
							if (!bModRet) {
								return false;
							}
						} catch (CException e) {
							CUtils::PrintStatus(false, sModRet);
							return false;
						}
#else
						CUtils::PrintStatus(false, "Modules are not enabled.");
#endif
						continue;
					}
				}
			} else {
				if (sName.CaseCmp("Listen") == 0 || sName.CaseCmp("ListenPort") == 0 || sName.CaseCmp("Listen6") == 0) {
					bool bSSL = false;
					bool bIPV6 = (sName.CaseCmp("Listen6") == 0);
					CString sPort;

					CString sBindHost;

					if (!bIPV6) {
						sValue.Replace(":", " ");
					}

					if (sValue.find(" ") != CString::npos) {
						sBindHost = sValue.Token(0, false, " ");
						sPort = sValue.Token(1, true, " ");
					} else {
						sPort = sValue;
					}

					if (sPort.Left(1) == "+") {
						sPort.LeftChomp();
						bSSL = true;
					}

					CString sHostComment;

					if (!sBindHost.empty()) {
						sHostComment = " on host [" + sBindHost + "]";
					}

					CString sIPV6Comment;

					if (bIPV6) {
						sIPV6Comment = " using ipv6";
					}

					unsigned short uPort = strtol(sPort.c_str(), NULL, 10);
					CUtils::PrintAction("Binding to port [" + CString((bSSL) ? "+" : "") + CString::ToString(uPort) + "]" + sHostComment + sIPV6Comment);

#ifndef HAVE_IPV6
					if (bIPV6) {
						CUtils::PrintStatus(false, "IPV6 is not enabled");
						return false;
					}
#endif

#ifndef HAVE_LIBSSL
					if (bSSL) {
						CUtils::PrintStatus(false, "SSL is not enabled");
						return false;
					}
#else
					CString sPemFile = GetPemLocation();

					if (bSSL && !CFile::Exists(sPemFile)) {
						CUtils::PrintStatus(false, "Unable to locate pem file: [" + sPemFile + "]");

						if (CUtils::GetBoolInput("Would you like to create a new pem file?", true)) {
							WritePemFile();
						} else {
							return false;
						}

						CUtils::PrintAction("Binding to port [+" + CString::ToString(uPort) + "]" + sHostComment + sIPV6Comment);
					}
#endif
					if (!uPort) {
						CUtils::PrintStatus(false, "Invalid port");
						return false;
					}

					CListener* pListener = new CListener(uPort, sBindHost, bSSL, bIPV6);

					if (!pListener->Listen()) {
						CUtils::PrintStatus(false, "Unable to bind");
						delete pListener;
						return false;
					}

					m_vpListeners.push_back(pListener);
					CUtils::PrintStatus(true);

					continue;
				} else if (sName.CaseCmp("LoadModule") == 0) {
					CString sModName = sValue.Token(0);
					CUtils::PrintAction("Loading Global Module [" + sModName + "]");
#ifdef _MODULES
					CString sModRet;
					CString sArgs = sValue.Token(1, true);

					try {
						bool bModRet = GetModules().LoadModule(sModName, sArgs, NULL, sModRet);
						CUtils::PrintStatus(bModRet, (bModRet) ? "" : sModRet);
						if (!bModRet) {
							return false;
						}
					} catch (CException e) {
						CUtils::PrintStatus(false, sModRet);
						return false;
					}
#else
					CUtils::PrintStatus(false, "Modules are not enabled.");
#endif
					continue;
				} else if (sName.CaseCmp("ISpoofFormat") == 0) {
					m_sISpoofFormat = sValue;
					continue;
				} else if (sName.CaseCmp("ISpoofFile") == 0) {
					m_sISpoofFile = sValue;
					continue;
				} else if (sName.CaseCmp("MOTD") == 0) {
					AddMotd(sValue);
					continue;
				} else if (sName.CaseCmp("VHost") == 0) {
					AddVHost(sValue);
					continue;
				} else if (sName.CaseCmp("PidFile") == 0) {
					if (!sValue.empty() && sValue[0] != '/') {
						m_sPidFile = GetZNCPath() + "/" + sValue;
					} else {
						m_sPidFile = sValue;
					}

					continue;
				} else if (sName.CaseCmp("StatusPrefix") == 0) {
					m_sStatusPrefix = sValue;
					continue;
				}
			}
		}

		CUtils::PrintError("Unhandled line in config: [" + sLine + "]");
		return false;
	}

	if (pChan) {
		delete pChan;
	}

	File.Close();

	if (!m_msUsers.size()) {
		CUtils::PrintError("You must define at least one user in your config.");
		return false;
	}

	return true;
}

void CZNC::ClearVHosts() {
	m_vsVHosts.clear();
}

bool CZNC::AddVHost(const CString& sHost) {
	if (sHost.empty()) {
		return false;
	}

	for (unsigned int a = 0; a < m_vsVHosts.size(); a++) {
		if (m_vsVHosts[a].CaseCmp(sHost) == 0) {
			return false;
		}
	}

	m_vsVHosts.push_back(sHost);
	return true;
}

bool CZNC::RemVHost(const CString& sHost) {
	// @todo
	return true;
}

void CZNC::Broadcast(const CString& sMessage, CUser* pUser) {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		if (a->second != pUser) {
			CString sMsg = sMessage;
#ifdef _MODULES
			CUser* m_pUser = a->second;	// This is a semi-hack because MODULECALLCONT below expects the user to be stored in m_pUser
			MODULECALLCONT(OnBroadcast(sMsg));
#endif
			a->second->PutStatusNotice("*** " + sMsg);
		}
	}
}

CString CZNC::FindModPath(const CString& sModule) const {
	CString sModPath = GetCurPath() + "/modules/" + sModule;
	sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

	if (!CFile::Exists(sModPath)) {
		//DEBUG_ONLY(cout << "[" << sModPath << "] Not found..." << endl);
		sModPath = GetModPath() + "/" + sModule;
		sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

		if (!CFile::Exists(sModPath)) {
			//DEBUG_ONLY(cout << "[" << sModPath << "] Not found..." << endl);
			sModPath = _MODDIR_ + CString("/") + sModule;
			sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

			if (!CFile::Exists(sModPath)) {
				//DEBUG_ONLY(cout << "[" << sModPath << "] Not found... giving up!" << endl);
				return "";
			}
		}
	}

	return sModPath;
}

CUser* CZNC::FindUser(const CString& sUsername) {
	map<CString,CUser*>::iterator it = m_msUsers.find(sUsername);

	if (it != m_msUsers.end()) {
		return it->second;
	}

	return NULL;
}

bool CZNC::DeleteUser(const CString& sUsername) {
	CUser* pUser = FindUser(sUsername);

	if (!pUser) {
		return false;
	}

	m_ssDelUsers.insert(pUser);
	return true;
}

bool CZNC::AddUser(CUser* pUser) {
	CString sErr;

	if (pUser->IsValid(sErr)) {
		m_msUsers[pUser->GetUserName()] = pUser;
		return true;
	}

	DEBUG_ONLY(cout << "Invalid user [" << pUser->GetUserName() << "] - [" << sErr << "]" << endl);
	return false;
}

CZNC& CZNC::Get() {
	static CZNC* pZNC = new CZNC;
	return *pZNC;
}

