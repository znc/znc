#include "znc.h"
#include "User.h"
#include "Server.h"
#include "UserSock.h"
#include "IRCSock.h"
#include "Utils.h"

#include <pwd.h>
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
				pIRCSock->SetPemLocation(GetBinPath() + "/znc.pem");
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

		if (File.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
			File.Write(CUtils::ToString(iPid) + "\n");
			File.Close();
			return true;
		}
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
		pUserSock->SetPemLocation(GetBinPath() + "/znc.pem");
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
	unsigned int uPos = sArgvPath.rfind('/');
	m_sBinPath = (uPos == string::npos) ? string(buf) : CUtils::ChangeDir(buf, sArgvPath.substr(0, uPos), "");

	// Try to set the user's home dir, default to binpath on failure
	struct passwd* pUserInfo = getpwuid(getuid());

	if (pUserInfo) {
		m_sHomePath = pUserInfo->pw_dir;
	}

	if (m_sHomePath.empty()) {
		m_sHomePath = m_sBinPath;
	}

	// Other dirs that we use
	m_sDLPath = m_sBinPath + "/downloads";
	m_sModPath = m_sBinPath + "/modules";
}

bool CZNC::ParseConfig(const string& sConfigFile) {
	string sStatusPrefix;

	string sFilePath = sConfigFile;
	if (CUtils::Left(sConfigFile, 1) != "/") {
		sFilePath = m_sBinPath + "/" + sFilePath;
	}

	CFile File(sFilePath);

	if (!File.Open(O_RDONLY)) {
		cerr << "Could not open config [" << sFilePath << "]" << endl;
		return false;
	}

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
						pUser = NULL;
						continue;
					}
				}
			} else if (strcasecmp(sTag.c_str(), "User") == 0) {
				if (pUser) {
					cerr << "You may not nest <User> tags inside of other <User> tags." << endl;
					return false;
				}

				if (sValue.empty()) {
					cerr << "You must supply a username in the <User> tag." << endl;
					return false;
				}

				if (m_msUsers.find(sValue) != m_msUsers.end()) {
					cerr << "User [" << sValue << "] defined more than once." << endl;
					return false;
				}

				pUser = new CUser(sValue, this);
				bAutoCycle = true;

				if (!sStatusPrefix.empty()) {
					if (!pUser->SetStatusPrefix(sStatusPrefix)) {
						cerr << "Invalid StatusPrefix [" + sStatusPrefix + "] Must be 1-5 chars, no spaces." << endl;
					}
				}
				m_msUsers[sValue] = pUser;
				continue;
			} else if (strcasecmp(sTag.c_str(), "Chan") == 0) {
				if (!pUser) {
					cerr << "<Chan> tags must be nested inside of a <User> tag." << endl;
					return false;
				}

				if (pChan) {
					cerr << "You may not nest <Chan> tags inside of other <Chan> tags." << endl;
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
					if (strcasecmp(sName.c_str(), "Nick") == 0) {
						pUser->SetNick(sValue);
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
							cerr << "Invalid StatusPrefix [" + sValue + "] Must be 1-5 chars, no spaces." << endl;
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
						if (!pUser->AddServer(sValue)) {
							cerr << "Unable to add server [" << sValue << "]" << endl;
						}
						continue;
					} else if (strcasecmp(sName.c_str(), "Chan") == 0) {
						pUser->AddChan(sValue);
						continue;
					} else if (strcasecmp(sName.c_str(), "LoadModule") == 0) {
#ifdef _MODULES
						string sModRet;
						string sModName = CUtils::Token(sValue, 0);
						string sArgs = CUtils::Token(sValue, 1, true);

						pUser->GetModules().LoadModule(sModName, sArgs, pUser, m_sBinPath, sModRet);
						cout << sModRet << endl;
#else
						cerr << "Unable to load [" << sValue << "] Modules are not enabled." << endl;
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

#ifndef HAVE_LIBSSL
					if (m_bSSL) {
						cerr << "SSL is not enabled, could not bind to ssl port [" << m_uListenPort << "]" << endl;
						return false;
					}
#endif

					if ((m_bSSL) && (!CFile::Exists(GetBinPath() + "/znc.pem"))) {
						cerr << "Unable to locate pem file: [" << GetBinPath() << "/znc.pem]" << endl;
						return false;
					}

					if (!Listen()) {
						cerr << "Could not bind to port [" << m_uListenPort << "]" << endl;
						return false;
					}

					continue;
				} else if (strcasecmp(sName.c_str(), "ISpoofFile") == 0) {
					m_sISpoofFile = sValue;
					continue;
				} else if (strcasecmp(sName.c_str(), "PidFile") == 0) {
					m_sPidFile = sValue;
					continue;
				} else if (strcasecmp(sName.c_str(), "StatusPrefix") == 0) {
					sStatusPrefix = sValue;
					continue;
				}
			}
		}

		cerr << "Unhandled line in config: [" << sLine << "]" << endl;
	}

	if (pChan) {
		delete pChan;
	}

	File.Close();

	if (!m_msUsers.size()) {
		cerr << "You must define at least one user in your config." << endl;
		return false;
	}

	return true;
}
