#include "znc.h"
#include "User.h"
#include "Server.h"
#include "UserSock.h"
#include "IRCSock.h"
#include "Utils.h"

#include <pwd.h>
#include <signal.h>
#include <sys/types.h>

#ifdef _MODULES
#include "Modules.h"
#endif

CZNC::CZNC() {
	m_pModules = new CGlobalModules(this);
	m_uListenPort = 0;
	m_bISpoofLocked = false;
	m_sISpoofFormat = "global { reply \"%\" }";
}

CZNC::~CZNC() {
#ifdef _MODULES
	delete m_pModules;

	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		a->second->GetModules().UnloadAll();
	}
#endif

	m_Manager.Cleanup();
	DeleteUsers();
}

CString CZNC::GetTag(bool bIncludeVersion) {
	if (!bIncludeVersion) {
		return "ZNC - by prozac@gmail.com";
	}

	char szBuf[32];
	memset(szBuf, 0, 32);
	snprintf(szBuf, 32, "ZNC %1.3f - by prozac@gmail.com", VERSION);

	return szBuf;
}

bool CZNC::OnBoot() {
	if (!GetModules().OnBoot()) {
		return false;
	}

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
		m_Manager.Loop();

		if (m_bISpoofLocked) {
			continue;
		}

		if (m_itUserIter == m_msUsers.end()) {
			m_itUserIter = m_msUsers.begin();
		}

		CString sSockName = "IRC::" + m_itUserIter->first;
		CUser* pUser = m_itUserIter->second;

		m_itUserIter++;

		CIRCSock* pIRCSock = (CIRCSock*) m_Manager.FindSockByName(sSockName);

		if (!pIRCSock) {
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
					CString sData = m_sISpoofFormat.Token(0, false, '%') + pUser->GetIdent() + m_sISpoofFormat.Token(1, true, '%');
					File.Write(sData + "\n");
					File.Close();
				}

				m_bISpoofLocked = true;
			}

			DEBUG_ONLY( cout << "User [" << pUser->GetUserName() << "] is connecting to [" << pServer->GetName() << ":" << pServer->GetPort() << "] ..." << endl);
			CUserSock* pUserSock = pUser->GetUserSock();

			if (pUserSock) {
				pUserSock->PutStatus("Attempting to connect to [" + pServer->GetName() + ":" + CString::ToString(pServer->GetPort()) + "] ...");
			}

			pIRCSock = new CIRCSock(this, pUser);
			pIRCSock->SetPass(pServer->GetPass());

			bool bSSL = false;
#ifdef HAVE_LIBSSL
			if (pServer->IsSSL()) {
				bSSL = true;
				pIRCSock->SetPemLocation(GetPemLocation());
			}
#endif
			if (!m_Manager.Connect(pServer->GetName(), pServer->GetPort(), sSockName, 20, bSSL, pUser->GetVHost(), pIRCSock)) {
				ReleaseISpoof();

				if (pUserSock) {
					pUserSock->PutStatus("Unable to connect. (Bad host?)");
				}
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

void CZNC::DeleteUsers() {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		delete a->second;
	}

	m_msUsers.clear();
	m_itUserIter = m_msUsers.end();
}

CUser* CZNC::GetUser(const CString& sUser) {
	// Todo: make this case insensitive
	map<CString,CUser*>::iterator it = m_msUsers.find(sUser);
	return (it == m_msUsers.end()) ? NULL : it->second;
}

Csock* CZNC::FindSockByName(const CString& sSockName) {
	return m_Manager.FindSockByName(sSockName);
}

bool CZNC::Listen() {
	if (!m_uListenPort) {
		return false;
	}

	CUserSock* pUserSock = new CUserSock;
	pUserSock->SetZNC(this);

	bool bSSL = false;
#ifdef HAVE_LIBSSL
	if (IsSSL()) {
		bSSL = true;
		pUserSock->SetPemLocation(GetPemLocation());
	}
#endif
	return m_Manager.ListenAll(m_uListenPort, "_LISTENER", bSSL, SOMAXCONN, pUserSock);
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
	m_sDLPath = m_sZNCPath + "/downloads";
	m_sModPath = m_sZNCPath + "/modules";
	m_sDataPath = m_sZNCPath + "/data";
}


CString CZNC::GetConfigPath(const CString& sConfigFile) {
	CString sRetPath;

	if (sConfigFile.empty()) {
		sRetPath = GetZNCPath() + "/znc.conf";
	} else {
		if (sConfigFile.Left(2) == "./" || sConfigFile.Left(3) == "../") {
			sRetPath = GetCurPath() + "/" + sConfigFile;
		} else if (sConfigFile.Left(1) != "/") {
			sRetPath = GetZNCPath() + "/" + sConfigFile;
		} else {
			sRetPath = sConfigFile;
		}
	}

	return sRetPath;
}

bool CZNC::WriteNewConfig(const CString& sConfig) {
	CString sAnswer, sUser;
	vector<CString> vsLines;
	bool bAnswer = false;

	CUtils::PrintMessage("");
	CUtils::PrintMessage("First lets start with some global settings...");
	CUtils::PrintMessage("");

	// ListenPort
	unsigned int uPort = 0;
	while(!CUtils::GetNumInput("What port would you like ZNC to listen on?", uPort, 1, 65535));
#ifdef HAVE_LIBSSL
	bAnswer = CUtils::GetBoolInput("Would you like ZNC to listen using SSL?", false);
#endif

	vsLines.push_back("ListenPort = " + CString((bAnswer) ? "+" : "") + CString::ToString(uPort));
	// !ListenPort

	// User
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Now we need to setup a user...");
	CUtils::PrintMessage("");

	do {
		vsLines.push_back("");
		CString sNick;
		do {
			CUtils::GetInput("Username", sUser, "", "AlphaNumeric");
		} while (!CUser::IsValidUserName(sUser));

		vsLines.push_back("<User " + sUser + ">");
		sAnswer = CUtils::GetHashPass();					vsLines.push_back("\tPass       = " + sAnswer + " -");
		CUtils::GetInput("Nick", sNick, sUser);			vsLines.push_back("\tNick       = " + sNick);
		CUtils::GetInput("Alt Nick", sAnswer, sNick + "_");	if (!sAnswer.empty()) { vsLines.push_back("\tAltNick    = " + sAnswer); }
		CUtils::GetInput("Ident", sAnswer, sNick);			vsLines.push_back("\tIdent      = " + sAnswer);
		CUtils::GetInput("Real Name", sAnswer, "Got ZNC?");	vsLines.push_back("\tRealName   = " + sAnswer);
		CUtils::GetInput("VHost", sAnswer, "", "optional");	if (!sAnswer.empty()) { vsLines.push_back("\tVHost      = " + sAnswer); }

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
		set<CModInfo> ssMods;
		CModules::GetAvailableMods(ssMods, this);

		if (ssMods.size()) {
			vsLines.push_back("");
			CUtils::PrintMessage("");
			CUtils::PrintMessage("-- Modules --");
			CUtils::PrintMessage("");

			if (CUtils::GetBoolInput("Do you want to automatically load any modules at all?")) {
				for (set<CModInfo>::iterator it = ssMods.begin(); it != ssMods.end(); it++) {
					const CModInfo& Info = *it;
					CString sName = Info.GetName();

					if (sName.Right(3).CaseCmp(".so") == 0) {
						sName.RightChomp(3);
					}

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
	} while (CUtils::GetBoolInput("Would you like to setup another user?", false));
	// !User

	CString sConfigFile = GetConfigPath(sConfig);
	CUtils::PrintAction("Writing config [" + sConfigFile + "]");
	CFile File(sConfigFile);

	if (!File.Open(O_WRONLY | O_CREAT)) {
		CUtils::PrintStatus(false, "Unable to open file");
		return false;
	}

	for (unsigned int a = 0; a < vsLines.size(); a++) {
		File.Write(vsLines[a] + "\n");
	}

	File.Close();
	CUtils::PrintStatus(true);

	CUtils::PrintMessage("");
	CUtils::PrintMessage("To connect to this znc you need to connect to it as your irc server", true);
	CUtils::PrintMessage("using the port that you supplied.  You have to supply your login info", true);
	CUtils::PrintMessage("as the irc server password like so.. user:pass.", true);
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Try something like this in your IRC client...", true);
	CUtils::PrintMessage("/server <znc_server_ip> " + CString::ToString(uPort) + " " + sUser + ":<pass>", true);
	CUtils::PrintMessage("");

	return true;
}

bool CZNC::ParseConfig(const CString& sConfig) {
	CString sStatusPrefix;
	CString sConfigFile = GetConfigPath(sConfig);

	CUtils::PrintAction("Opening Config [" + sConfigFile + "]");

	if (!CFile::Exists(sConfigFile)) {
		CUtils::PrintStatus(false, "No such file");
		if (!CUtils::GetBoolInput("Would you like to create this config now?", true)) {
			return false;
		}

		WriteNewConfig(sConfigFile);
		CUtils::PrintAction("Opening Config [" + sConfigFile + "]");
	}

	if (!CFile::IsReg(sConfigFile)) {
		CUtils::PrintStatus(false, "Not a file");
		return false;
	}

	if (!m_LockFile.TryExLock(sConfigFile, 50)) {
		CUtils::PrintStatus(false, "ZNC is already running on this config.");
		return false;
	}

	CFile File(sConfigFile);

	if (!File.Open(O_RDONLY)) {
		CUtils::PrintStatus(false);
		return false;
	}

	CUtils::PrintStatus(true);

	CString sLine;
	bool bCommented = false;	// support for /**/ style comments
	bool bAutoCycle = true;
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

						m_msUsers[pUser->GetUserName()] = pUser;

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

				pUser = new CUser(sValue, this);
				CUtils::PrintMessage("Loading user [" + sValue + "]");
				bAutoCycle = true;

				if (!sStatusPrefix.empty()) {
					if (!pUser->SetStatusPrefix(sStatusPrefix)) {
						CUtils::PrintError("Invalid StatusPrefix [" + sStatusPrefix + "] Must be 1-5 chars, no spaces.");
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

				pChan = new CChan(sValue, pUser);
				pChan->SetAutoCycle(bAutoCycle);
				continue;
			}
		}

		// If we have a regular line, figure out where it goes
		CString sName = sLine.Token(0, false, '=');
		CString sValue = sLine.Token(1, true, '=');
		sName.Trim();
		sValue.Trim();

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
					} else if (sName.CaseCmp("Nick") == 0) {
						pUser->SetNick(sValue);
						continue;
					} else if (sName.CaseCmp("VersionReply") == 0) {
						pUser->SetVersionReply(sValue);
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
					} else if (sName.CaseCmp("AutoCycle") == 0) {
						bAutoCycle = (sValue.CaseCmp("true") == 0);
						continue;
					} else if (sName.CaseCmp("Ident") == 0) {
						pUser->SetIdent(sValue);
						continue;
					} else if (sName.CaseCmp("DenyLoadMod") == 0) {
						pUser->SetDenyLoadMod((sValue.CaseCmp("TRUE") == 0));
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
						pUser->AddChan(sValue);
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
				if (sName.CaseCmp("ListenPort") == 0) {
					m_bSSL = false;
					CString sPort = sValue;
					if (sPort.Left(1) == "+") {
						sPort.LeftChomp();
						m_bSSL = true;
					}

					m_uListenPort = strtol(sPort.c_str(), NULL, 10);
					CUtils::PrintAction("Binding to port [" + CString((m_bSSL) ? "+" : "") + CString::ToString(m_uListenPort) + "]");

#ifndef HAVE_LIBSSL
					if (m_bSSL) {
						CUtils::PrintStatus(false, "SSL is not enabled");
						return false;
					}
#else
					CString sPemFile = GetPemLocation();

					if ((m_bSSL) && (!CFile::Exists(sPemFile))) {
						CUtils::PrintStatus(false, "Unable to locate pem file: [" + sPemFile + "]");

						if (CUtils::GetBoolInput("Would you like to create a new pem file?", true)) {
							CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");

							if (CFile::Exists(sPemFile)) {
								CUtils::PrintStatus(false, "File already exists");
								return false;
							}

							FILE *f = fopen(sPemFile.c_str(), "w");

							if (!f) {
								CUtils::PrintStatus(false, "Unable to open");
								return false;
							}

							CUtils::GenerateCert(f, false);
							fclose(f);

							CUtils::PrintStatus(true);
						} else {
							return false;
						}

						CUtils::PrintAction("Binding to port [" + CString((m_bSSL) ? "+" : "") + CString::ToString(m_uListenPort) + "]");
					}
#endif
					if (!m_uListenPort) {
						CUtils::PrintStatus(false, "Invalid port");
						return false;
					}

					if (!Listen()) {
						CUtils::PrintStatus(false, "Unable to bind");
						return false;
					}

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
				} else if (sName.CaseCmp("PidFile") == 0) {
					if (!sValue.empty() && sValue[0] != '/') {
						m_sPidFile = GetZNCPath() + "/" + sValue;
					} else {
						m_sPidFile = sValue;
					}

					continue;
				} else if (sName.CaseCmp("StatusPrefix") == 0) {
					sStatusPrefix = sValue;
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

CString CZNC::FindModPath(const CString& sModule) const {
	CString sModPath = GetCurPath() + "/modules/" + sModule;
	sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

	if (!CFile::Exists(sModPath)) {
		DEBUG_ONLY(cout << "[" << sModPath << "] Not found..." << endl);
		sModPath = GetModPath() + "/" + sModule;
		sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

		if (!CFile::Exists(sModPath)) {
			DEBUG_ONLY(cout << "[" << sModPath << "] Not found..." << endl);
			sModPath = _MODDIR_ + CString("/") + sModule;
			sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

			if (!CFile::Exists(sModPath)) {
				DEBUG_ONLY(cout << "[" << sModPath << "] Not found... giving up!" << endl);
				return "";
			}
		}
	}

	return sModPath;
}
