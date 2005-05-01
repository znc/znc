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
	m_uListenPort = 0;
	m_bISpoofLocked = false;
}

CZNC::~CZNC() {
	m_Manager.Cleanup();
	DeleteUsers();
}

bool CZNC::OnBoot() {
	for (map<string,CUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
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

		string sSockName = "IRC::" + m_itUserIter->first;
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
					File.Write(pUser->GetIdent() + "\r\n");
					File.Close();
				}

				m_bISpoofLocked = true;
			}

			DEBUG_ONLY( cout << "User [" << pUser->GetUserName() << "] is connecting to [" << pServer->GetName() << ":" << pServer->GetPort() << "] ..." << endl);
			CUserSock* pUserSock = pUser->GetUserSock();

			if (pUserSock) {
				pUserSock->PutStatus("Attempting to connect to [" + pServer->GetName() + ":" + CUtils::ToString(pServer->GetPort()) + "] ...");
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
			File.Write(CUtils::ToString(iPid) + "\n");
			File.Close();
			CUtils::PrintStatus(true);
			return true;
		}

		CUtils::PrintStatus(false);
	}

	return false;
}

void CZNC::DeleteUsers() {
	for (map<string,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		delete a->second;
	}

	m_msUsers.clear();
	m_itUserIter = m_msUsers.end();
}

CUser* CZNC::GetUser(const string& sUser) {
	// Todo: make this case insensitive
	map<string,CUser*>::iterator it = m_msUsers.find(sUser);
	return (it == m_msUsers.end()) ? NULL : it->second;
}

Csock* CZNC::FindSockByName(const string& sSockName) {
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

bool CZNC::IsHostAllowed(const string& sHostMask) {
	for (map<string,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		if (a->second->IsHostAllowed(sHostMask)) {
			return true;
		}
	}

	return false;
}

void CZNC::InitDirs(const string& sArgvPath) {
	char buf[PATH_MAX];
	getcwd(buf, PATH_MAX);

	// If the bin was not ran from the current directory, we need to add that dir onto our cwd
	string::size_type uPos = sArgvPath.rfind('/');
	m_sCurPath = (uPos == string::npos) ? string(buf) : CUtils::ChangeDir(buf, sArgvPath.substr(0, uPos), "");

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


string CZNC::GetConfigPath(const string& sConfigFile) {
	string sRetPath;

	if (sConfigFile.empty()) {
		sRetPath = GetZNCPath() + "/znc.conf";
	} else {
		if (CUtils::Left(sConfigFile, 2) == "./" || CUtils::Left(sConfigFile, 3) == "../") {
			sRetPath = GetCurPath() + "/" + sConfigFile;
		} else if (CUtils::Left(sConfigFile, 1) != "/") {
			sRetPath = GetZNCPath() + "/" + sConfigFile;
		} else {
			sRetPath = sConfigFile;
		}
	}

	return sRetPath;
}

bool CZNC::WriteNewConfig(const string& sConfig) {
	string sAnswer;
	vector<string> vsLines;
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

	vsLines.push_back("ListenPort = " + string((bAnswer) ? "+" : "") + CUtils::ToString(uPort));
	// !ListenPort

	// User
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Now we need to setup a user...");
	CUtils::PrintMessage("");

	do {
		vsLines.push_back("");
		string sUser, sNick;
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

		CUtils::GetNumInput("Number of lines to buffer per channel", uBufferCount, 0, ~0, 50);	if (uBufferCount) { vsLines.push_back("\tBuffer     = " + CUtils::ToString(uBufferCount)); }
		if (CUtils::GetBoolInput("Would you like your buffer to be sticky?", true)) {
			vsLines.push_back("\tKeepBuffer = true");
		} else {
			vsLines.push_back("\tKeepBuffer = false");
		}

		CUtils::GetInput("Default channel modes", sAnswer, "+stn");
		if (!sAnswer.empty()) {
			vsLines.push_back("\tChanModes  = " + sAnswer);
		}

		vsLines.push_back("");
		CUtils::PrintMessage("");
		CUtils::PrintMessage("Now we need to setup some servers for this user to connect to...");
		CUtils::PrintMessage("");

		do {
			string sHost, sPass;
			bool bSSL = false;
			unsigned int uPort = 0;

			while(!CUtils::GetInput("IRC server", sHost, "", "host only") || !CServer::IsValidHostName(sHost));
			while(!CUtils::GetNumInput("[" + sHost + "] Port", uPort, 1, 65535, 6667));
			CUtils::GetInput("[" + sHost + "] Password (probably empty)", sPass);

#ifdef HAVE_LIBSSL
			bSSL = CUtils::GetBoolInput("Does this server use SSL? (probably no)", false);
#endif

			vsLines.push_back("\tServer     = " + sHost + ((bSSL) ? " +" : " ") + CUtils::ToString(uPort) + " " + sPass);
		} while (CUtils::GetBoolInput("Would you like to add another server?", false));

		vsLines.push_back("");
		CUtils::PrintMessage("");
		CUtils::PrintMessage("Now we need to setup some channels that this user will join once connected...");
		CUtils::PrintMessage("");

		string sArg = "a";
		string sPost = " for ZNC to automatically join?";
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

	string sConfigFile = GetConfigPath(sConfig);
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
	return true;
}

bool CZNC::ParseConfig(const string& sConfig) {
	string sStatusPrefix;
	string sConfigFile = GetConfigPath(sConfig);

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

	string sLine;
	bool bCommented = false;	// support for /**/ style comments
	bool bAutoCycle = true;
	CUser* pUser = NULL;	// Used to keep track of which user block we are in
	CChan* pChan = NULL;	// Used to keep track of which chan block we are in

	while (File.ReadLine(sLine)) {
		while ((CUtils::Right(sLine, 1) == "\r") || (CUtils::Right(sLine, 1) == "\n")) {
			CUtils::Trim(sLine);
		}

		if ((sLine.empty()) || (sLine[0] == '#') || (CUtils::Left(sLine, 2) == "//")) {
			continue;
		}

		if (CUtils::Left(sLine, 2) == "/*") {
			if (CUtils::Right(sLine, 2) != "*/") {
				bCommented = true;
			}

			continue;
		}

		if (bCommented) {
			if (CUtils::Right(sLine, 2) == "*/") {
				bCommented = false;
			}

			continue;
		}

		if ((CUtils::Left(sLine, 1) == "<") && (CUtils::Right(sLine, 1) == ">")) {
			CUtils::LeftChomp(sLine);
			CUtils::RightChomp(sLine);
			CUtils::Trim(sLine);

			string sTag = sLine.substr(0, sLine.find_first_of(" \t\r\n"));
			string sValue = (sTag.size() < sLine.size()) ? sLine.substr(sTag.size() +1) : "";

			CUtils::Trim(sTag);
			CUtils::Trim(sValue);

			if (CUtils::Left(sLine, 1) == "/") {
				string sTag = sLine.substr(1);

				if (pUser) {
					if (pChan) {
						if (strcasecmp(sTag.c_str(), "Chan") == 0) {
							pUser->AddChan(pChan);
							pChan = NULL;
							continue;
						}
					} else if (strcasecmp(sTag.c_str(), "User") == 0) {
						string sErr;

						if (!pUser->IsValid(sErr)) {
							CUtils::PrintError("Invalid user [" + pUser->GetUserName() + "] " + sErr);
							return false;
						}

						m_msUsers[pUser->GetUserName()] = pUser;

						pUser = NULL;
						continue;
					}
				}
			} else if (strcasecmp(sTag.c_str(), "User") == 0) {
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
				bAutoCycle = true;

				if (!sStatusPrefix.empty()) {
					if (!pUser->SetStatusPrefix(sStatusPrefix)) {
						CUtils::PrintError("Invalid StatusPrefix [" + sStatusPrefix + "] Must be 1-5 chars, no spaces.");
						return false;
					}
				}

				continue;
			} else if (strcasecmp(sTag.c_str(), "Chan") == 0) {
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
		string sName = CUtils::Token(sLine, 0, false, '=');
		string sValue = CUtils::Token(sLine, 1, true, '=');
		CUtils::Trim(sName);
		CUtils::Trim(sValue);

		if ((!sName.empty()) && (!sValue.empty())) {
			if (pUser) {
				if (pChan) {
					if (strcasecmp(sName.c_str(), "Buffer") == 0) {
						pChan->SetBufferCount(strtoul(sValue.c_str(), NULL, 10));
						continue;
					} else if (strcasecmp(sName.c_str(), "KeepBuffer") == 0) {
						pChan->SetKeepBuffer((strcasecmp(sValue.c_str(), "true") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "Detached") == 0) {
						pChan->SetDetached((strcasecmp(sValue.c_str(), "true") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "AutoCycle") == 0) {
						pChan->SetAutoCycle((strcasecmp(sValue.c_str(), "true") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "Key") == 0) {
						pChan->SetKey(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "Modes") == 0) {
						pChan->SetDefaultModes(sValue);
						continue;
					}
				} else {
					if (strcasecmp(sName.c_str(), "Buffer") == 0) {
						pUser->SetBufferCount(strtoul(sValue.c_str(), NULL, 10));
						continue;
					} else if (strcasecmp(sName.c_str(), "KeepBuffer") == 0) {
						pUser->SetKeepBuffer((strcasecmp(sValue.c_str(), "true") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "Nick") == 0) {
						pUser->SetNick(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "VersionReply") == 0) {
						pUser->SetVersionReply(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "QuitMsg") == 0) {
						pUser->SetQuitMsg(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "AltNick") == 0) {
						pUser->SetAltNick(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "Pass") == 0) {
						if (CUtils::Right(sValue, 1) == "-") {
							CUtils::RightChomp(sValue);
							CUtils::Trim(sValue);
							pUser->SetPass(sValue, true);
						} else {
							pUser->SetPass(sValue, false);
						}

						continue;
					} else if (strcasecmp(sName.c_str(), "AutoCycle") == 0) {
						bAutoCycle = (strcasecmp(sValue.c_str(), "true") == 0);
						continue;
					} else if (strcasecmp(sName.c_str(), "Ident") == 0) {
						pUser->SetIdent(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "DenyLoadMod") == 0) {
						pUser->SetDenyLoadMod((strcasecmp(sValue.c_str(), "TRUE") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "StatusPrefix") == 0) {
						if (!pUser->SetStatusPrefix(sValue)) {
							CUtils::PrintError("Invalid StatusPrefix [" + sValue + "] Must be 1-5 chars, no spaces.");
							return false;
						}
						continue;
					} else if (strcasecmp(sName.c_str(), "DCCLookupMethod") == 0) {
						pUser->SetUseClientIP((strcasecmp(sValue.c_str(), "Client") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "RealName") == 0) {
						pUser->SetRealName(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "KeepNick") == 0) {
						pUser->SetKeepNick((strcasecmp(sValue.c_str(), "true") == 0));
						continue;
					} else if (strcasecmp(sName.c_str(), "ChanModes") == 0) {
						pUser->SetDefaultChanModes(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "VHost") == 0) {
						pUser->SetVHost(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "Allow") == 0) {
						pUser->AddAllowedHost(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "Server") == 0) {
						CUtils::PrintAction("Adding Server [" + sValue + "]");
						CUtils::PrintStatus(pUser->AddServer(sValue));
						continue;
					} else if (strcasecmp(sName.c_str(), "Chan") == 0) {
						pUser->AddChan(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "LoadModule") == 0) {
						string sModName = CUtils::Token(sValue, 0);
						CUtils::PrintAction("Loading Module [" + sModName + "]");
#ifdef _MODULES
						string sModRet;
						string sArgs = CUtils::Token(sValue, 1, true);

						try {
							bool bModRet = pUser->GetModules().LoadModule(sModName, sArgs, pUser, sModRet);
							CUtils::PrintStatus(bModRet, (bModRet) ? "" : sModRet);
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
				if (strcasecmp(sName.c_str(), "ListenPort") == 0) {
					m_bSSL = false;
					string sPort = sValue;
					if (CUtils::Left(sPort, 1) == "+") {
						CUtils::LeftChomp(sPort);
						m_bSSL = true;
					}

					m_uListenPort = strtol(sPort.c_str(), NULL, 10);
					CUtils::PrintAction("Binding to port [" + string((m_bSSL) ? "+" : "") + CUtils::ToString(m_uListenPort) + "]");

#ifndef HAVE_LIBSSL
					if (m_bSSL) {
						CUtils::PrintStatus(false, "SSL is not enabled");
						return false;
					}
#else
					string sPemFile = GetPemLocation();

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

						CUtils::PrintAction("Binding to port [" + string((m_bSSL) ? "+" : "") + CUtils::ToString(m_uListenPort) + "]");
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
				} else if (strcasecmp(sName.c_str(), "ISpoofFile") == 0) {
					m_sISpoofFile = sValue;
					continue;
				} else if (strcasecmp(sName.c_str(), "PidFile") == 0) {
					if (!sValue.empty() && sValue[0] != '/') {
						m_sPidFile = GetZNCPath() + "/" + sValue;
					} else {
						m_sPidFile = sValue;
					}

					continue;
				} else if (strcasecmp(sName.c_str(), "StatusPrefix") == 0) {
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
