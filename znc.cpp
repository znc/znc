/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "Chan.h"
#include "IRCSock.h"
#include "Server.h"
#include "User.h"
#include <pwd.h>
#include <list>

namespace
{ // private namespace for local things
	struct CGlobalModuleConfigLine
	{
		CString	m_sName;
		CString	m_sValue;
		CUser	*m_pUser;
		CChan	*m_pChan;
	};
};

CZNC::CZNC() {
#ifdef _MODULES
	m_pModules = new CGlobalModules();
#endif
	m_pISpoofLockFile = NULL;
	m_uiConnectDelay = 30;
	SetISpoofFormat(""); // Set ISpoofFormat to default
	m_uBytesRead = 0;
	m_uBytesWritten = 0;
	m_pConnectUserTimer = NULL;
	m_bNeedRehash = false;
	m_TimeStarted = time(NULL);
}

CZNC::~CZNC() {
	if (m_pISpoofLockFile)
		ReleaseISpoof();

#ifdef _MODULES
	m_pModules->UnloadAll();

	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		a->second->GetModules().UnloadAll();
	}
#endif

	for (size_t b = 0; b < m_vpListeners.size(); b++) {
		delete m_vpListeners[b];
	}

	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		a->second->SetBeingDeleted(true);
	}

	m_pConnectUserTimer = NULL;
	// This deletes m_pConnectUserTimer
	m_Manager.Cleanup();
	DeleteUsers();

#ifdef _MODULES
	delete m_pModules;
#endif

	DeletePidFile();
}

CString CZNC::GetVersion() {
	char szBuf[128];

	snprintf(szBuf, sizeof(szBuf), "%1.3f"VERSION_EXTRA, VERSION);
	// If snprintf overflows (which I doubt), we want to be on the safe side
	szBuf[sizeof(szBuf) - 1] = '\0';

	return szBuf;
}

CString CZNC::GetTag(bool bIncludeVersion) {
	if (!bIncludeVersion) {
		return "ZNC - http://znc.sourceforge.net";
	}

	char szBuf[128];
	snprintf(szBuf, sizeof(szBuf), "ZNC %1.3f"VERSION_EXTRA" - http://znc.sourceforge.net", VERSION);
	// If snprintf overflows (which I doubt), we want to be on the safe side
	szBuf[sizeof(szBuf) - 1] = '\0';

	return szBuf;
}

CString CZNC::GetUptime() const {
	time_t now = time(NULL);
	return CString::ToTimeStr(now - TimeStarted());
}

bool CZNC::OnBoot() {
#ifdef _MODULES
	if (!GetModules().OnBoot()) {
		return false;
	}

	for (map<CString,CUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
		if (!it->second->GetModules().OnBoot()) {
			return false;
		}
	}
#endif

	return true;
}

bool CZNC::ConnectUser(CUser *pUser) {
	CString sSockName = "IRC::" + pUser->GetUserName();
	// Don't use pUser->GetIRCSock(), as that only returns something if the
	// CIRCSock is already connected, not when it's still connecting!
	CIRCSock* pIRCSock = (CIRCSock*) m_Manager.FindSockByName(sSockName);

	if (m_pISpoofLockFile != NULL) {
		return false;
	}

	if (!pUser->GetIRCConnectEnabled())
		return false;

	if (pIRCSock || !pUser->HasServers())
		return false;

	if (pUser->ConnectPaused())
		return false;

	CServer* pServer = pUser->GetNextServer();

	if (!pServer)
		return false;

	if (!WriteISpoof(pUser)) {
		DEBUG_ONLY(cout << "ISpoof could not be written" << endl);
		pUser->PutStatus("ISpoof could not be written, retrying...");
		return true;
	}

	DEBUG_ONLY(cout << "User [" << pUser->GetUserName() << "] is connecting to [" << pServer->GetName() << ":" << pServer->GetPort() << "] ..." << endl);
	pUser->PutStatus("Attempting to connect to [" + pServer->GetName() + ":" + CString(pServer->GetPort()) + "] ...");

	pIRCSock = new CIRCSock(pUser);
	pIRCSock->SetPass(pServer->GetPass());

	bool bSSL = false;
#ifdef HAVE_LIBSSL
	if (pServer->IsSSL()) {
		bSSL = true;
	}
#endif

	if (!m_Manager.Connect(pServer->GetName(), pServer->GetPort(), sSockName, 120, bSSL, pUser->GetVHost(), pIRCSock)) {
		ReleaseISpoof();
		pUser->PutStatus("Unable to connect. (Bad host?)");
	}

	return true;
}

bool CZNC::HandleUserDeletion()
{
	map<CString, CUser*>::iterator it;
	map<CString, CUser*>::iterator end;

	if (m_msDelUsers.size() == 0)
		return false;

	end = m_msDelUsers.end();
	for (it = m_msDelUsers.begin(); it != end; it++) {
		CUser* pUser = it->second;
		pUser->SetBeingDeleted(true);

#ifdef _MODULES
		if (GetModules().OnDeleteUser(*pUser)) {
			pUser->SetBeingDeleted(false);
			continue;
		}
#endif
		m_msUsers.erase(pUser->GetUserName());

		CIRCSock* pIRCSock = pUser->GetIRCSock();

		if (pIRCSock) {
			m_Manager.DelSockByAddr(pIRCSock);
		}

		pUser->DelClients();
#ifdef _MODULES
		pUser->DelModules();
#endif
		AddBytesRead(pUser->BytesRead());
		AddBytesWritten(pUser->BytesWritten());
		delete pUser;
	}

	m_msDelUsers.clear();

	return true;
}

int CZNC::Loop() {
	while (true) {
		CString sError;

		if (GetNeedRehash()) {
			SetNeedRehash(false);

			if (RehashConfig(sError)) {
				Broadcast("Rehashing succeeded", true);
			} else {
				Broadcast("Rehashing failed: " + sError, true);
				Broadcast("ZNC is in some possibly inconsistent state!", true);
			}
		}

		// Check for users that need to be deleted
		if (HandleUserDeletion()) {
			// Also remove those user(s) from the config file
			WriteConfig();
		}

		// Csocket wants micro seconds
		// 500 msec to 600 sec
		m_Manager.DynamicSelectLoop(500 * 1000, 600 * 1000 * 1000);
	}

	return 0;
}

bool CZNC::WriteISpoof(CUser* pUser) {
	if (m_pISpoofLockFile != NULL)
		return false;

	if (!m_sISpoofFile.empty()) {
		m_pISpoofLockFile = new CLockFile;
		if (!m_pISpoofLockFile->TryExLock(m_sISpoofFile, true)) {
			delete m_pISpoofLockFile;
			m_pISpoofLockFile = NULL;
			return false;
		}

		CFile File(m_pISpoofLockFile->GetFD(), m_pISpoofLockFile->GetFileName());

		char buf[1024];
		memset((char*) buf, 0, 1024);
		File.Read(buf, 1023);
		m_sOrigISpoof = buf;

		if (!File.Seek(0) || !File.Truncate()) {
			delete m_pISpoofLockFile;
			m_pISpoofLockFile = NULL;
			return false;
		}

		CString sData = m_sISpoofFormat.Token(0, false, "%") + pUser->GetIdent() + m_sISpoofFormat.Token(1, true, "%");
		File.Write(sData + "\n");
	}
	return true;
}

void CZNC::ReleaseISpoof() {
	if (m_pISpoofLockFile == NULL)
		return;

	if (!m_sISpoofFile.empty()) {
		CFile File(m_pISpoofLockFile->GetFD(), m_pISpoofLockFile->GetFileName());

		if (File.Seek(0) && File.Truncate()) {
			File.Write(m_sOrigISpoof);
		}

		m_sOrigISpoof = "";
	}

	delete m_pISpoofLockFile;
	m_pISpoofLockFile = NULL;
}

bool CZNC::WritePidFile(int iPid) {
	if (!m_sPidFile.empty()) {
		CString sFile;

		// absolute path or relative to the data dir?
		if (m_sPidFile[0] != '/')
			sFile = GetZNCPath() + "/" + m_sPidFile;
		else
			sFile = m_sPidFile;

		CFile File(sFile);
		CUtils::PrintAction("Writing pid file [" + sFile + "]");

		if (File.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
			File.Write(CString(iPid) + "\n");
			File.Close();
			CUtils::PrintStatus(true);
			return true;
		}

		CUtils::PrintStatus(false);
	}

	return false;
}

bool CZNC::DeletePidFile() {
	if (!m_sPidFile.empty()) {
		CString sFile;
		// absolute path or relative to the data dir?
		if (m_sPidFile[0] != '/')
			sFile = GetZNCPath() + "/" + m_sPidFile;
		else
			sFile = m_sPidFile;

		CFile File(sFile);
		CUtils::PrintAction("Deleting pid file [" + sFile + "]");
		if (File.Delete()) {
			CUtils::PrintStatus(true);
			return true;
		}
		CUtils::PrintStatus(false);
	}
	return false;
}

bool CZNC::WritePemFile(bool bEncPem) {
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

	while (!CUtils::GetInput("hostname of your shell", sHost, sHost, "including the '.com' portion")) ;

	CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");
	FILE *f = fopen(sPemFile.c_str(), "w");

	if (!f) {
		CUtils::PrintStatus(false, "Unable to open");
		return false;
	}

	CUtils::GenerateCert(f, bEncPem, sHost);
	fclose(f);

	CUtils::PrintStatus(true);
	return true;
#endif
}

void CZNC::DeleteUsers() {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		a->second->SetBeingDeleted(true);
		delete a->second;
	}

	m_msUsers.clear();
	DisableConnectUser();
}

CUser* CZNC::GetUser(const CString& sUser) {
	// Todo: make this case insensitive
	map<CString,CUser*>::iterator it = m_msUsers.find(sUser);
	return (it == m_msUsers.end()) ? NULL : it->second;
}

Csock* CZNC::FindSockByName(const CString& sSockName) {
	return m_Manager.FindSockByName(sSockName);
}

bool CZNC::IsHostAllowed(const CString& sHostMask) const {
	for (map<CString,CUser*>::const_iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		if (a->second->IsHostAllowed(sHostMask)) {
			return true;
		}
	}

	return false;
}

void CZNC::InitDirs(const CString& sArgvPath, const CString& sDataDir) {
	char buf[PATH_MAX];
	char *home;

	if (getcwd(buf, PATH_MAX) == NULL) {
		CUtils::PrintError("getcwd() failed, can't read my current dir");
		exit(-1);
	}

	// If the bin was not ran from the current directory, we need to add that dir onto our cwd
	CString::size_type uPos = sArgvPath.rfind('/');
	m_sCurPath = (uPos == CString::npos) ? CString(buf) : CDir::ChangeDir(buf, sArgvPath.substr(0, uPos), "");

	// Try to set the user's home dir, default to binpath on failure
	home = getenv("HOME");

	m_sHomePath.clear();
	if (home) {
		m_sHomePath = home;
	}

	if (m_sHomePath.empty()) {
		struct passwd* pUserInfo = getpwuid(getuid());

		if (pUserInfo) {
			m_sHomePath = pUserInfo->pw_dir;
		}
	}

	if (m_sHomePath.empty()) {
		m_sHomePath = m_sCurPath;
	}

	if (sDataDir.empty()) {
		m_sZNCPath = m_sHomePath + "/.znc";
	} else {
		m_sZNCPath = sDataDir;
	}

	// Other dirs that we use
	m_sConfPath = m_sZNCPath + "/configs";
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

bool CZNC::BackupConfig() const {
	CString sBackup = GetConfigFile() + "-backup";

	// Create a new backup overwriting an old one we might have
	if (CFile::Copy(m_sConfigFile, sBackup, true))
		return true;

	// Don't abort if no config file exists
	if (!CFile::Exists(m_sConfigFile))
		// No backup because we got nothing to backup
		return true;

	return false;
}

bool CZNC::WriteConfig() {
	CFile File(GetConfigFile());

	if (!BackupConfig()) {
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

		File.Write("Listen" + s6 + "      = " + sHostPortion + CString((pListener->IsSSL()) ? "+" : "") + CString(pListener->GetPort()) + "\n");
	}

	File.Write("ConnectDelay = " + CString(m_uiConnectDelay) + "\n");

	if (!m_sISpoofFile.empty()) {
		File.Write("ISpoofFile   = " + m_sISpoofFile + "\n");
		if (!m_sISpoofFormat.empty()) { File.Write("ISpoofFormat = " + m_sISpoofFormat + "\n"); }
	}

	if (!m_sPidFile.empty()) { File.Write("PidFile      = " + m_sPidFile + "\n"); }
	if (!m_sStatusPrefix.empty()) { File.Write("StatusPrefix = " + m_sStatusPrefix + "\n"); }

	for (unsigned int m = 0; m < m_vsMotd.size(); m++) {
		File.Write("Motd         = " + m_vsMotd[m] + "\n");
	}

	for (unsigned int v = 0; v < m_vsVHosts.size(); v++) {
		File.Write("VHost        = " + m_vsVHosts[v] + "\n");
	}

#ifdef _MODULES
	CGlobalModules& Mods = GetModules();

	for (unsigned int a = 0; a < Mods.size(); a++) {
		CString sArgs = Mods[a]->GetArgs();

		if (!sArgs.empty()) {
			sArgs = " " + sArgs;
		}

		File.Write("LoadModule   = " + Mods[a]->GetModName() + sArgs + "\n");
	}
#endif

	for (map<CString,CUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
		CString sErr;

		if (!it->second->IsValid(sErr)) {
			DEBUG_ONLY(cerr << "** Error writing config for user [" << it->first << "] [" << sErr << "]" << endl);
			continue;
		}

		File.Write("\n");

		if (!it->second->WriteConfig(File)) {
			DEBUG_ONLY(cerr << "** Error writing config for user [" << it->first << "]" << endl);
		}
	}

	File.Close();

	return true;
}

bool CZNC::WriteNewConfig(CString& sConfigFile) {
	CString sAnswer, sUser;
	VCString vsLines;

	sConfigFile = ExpandConfigPath((sConfigFile.empty()) ? "znc.conf" : sConfigFile);
	CUtils::PrintMessage("Building new config");

	CUtils::PrintMessage("");
	CUtils::PrintMessage("First lets start with some global settings...");
	CUtils::PrintMessage("");

	// Listen
	unsigned int uListenPort = 0;
	while (!CUtils::GetNumInput("What port would you like ZNC to listen on?", uListenPort, 1, 65535)) ;

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

	vsLines.push_back("Listen" + s6 + "    = " + sListenHost + sSSL + CString(uListenPort));
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

				if (sName.Right(3).Equals(".so")) {
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
		CString sSalt;
		sAnswer = CUtils::GetSaltedHashPass(sSalt);
		vsLines.push_back("\tPass       = md5#" + sAnswer + "#" + sSalt + "#");

		if (CUtils::GetBoolInput("Would you like this user to be an admin?", bFirstUser)) {
			vsLines.push_back("\tAdmin      = true");
		} else {
			vsLines.push_back("\tAdmin      = false");
		}

		CUtils::GetInput("Nick", sNick, CUser::MakeCleanUserName(sUser));
		vsLines.push_back("\tNick       = " + sNick);
		CUtils::GetInput("Alt Nick", sAnswer, sNick + "_");
		if (!sAnswer.empty()) {
			vsLines.push_back("\tAltNick    = " + sAnswer);
		}
		CUtils::GetInput("Ident", sAnswer, sNick);
		vsLines.push_back("\tIdent      = " + sAnswer);
		CUtils::GetInput("Real Name", sAnswer, "Got ZNC?");
		vsLines.push_back("\tRealName   = " + sAnswer);
		CUtils::GetInput("VHost", sAnswer, "", "optional");
		if (!sAnswer.empty()) {
			vsLines.push_back("\tVHost      = " + sAnswer);
		}
		// todo: Possibly add motd

		unsigned int uBufferCount = 0;

		CUtils::GetNumInput("Number of lines to buffer per channel", uBufferCount, 0, ~0, 50);
		if (uBufferCount) {
			vsLines.push_back("\tBuffer     = " + CString(uBufferCount));
		}
		if (CUtils::GetBoolInput("Would you like to keep buffers after replay?", false)) {
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
			unsigned int uServerPort = 0;

			while (!CUtils::GetInput("IRC server", sHost, "", "host only") || !CServer::IsValidHostName(sHost)) ;
			while (!CUtils::GetNumInput("[" + sHost + "] Port", uServerPort, 1, 65535, 6667)) ;
			CUtils::GetInput("[" + sHost + "] Password (probably empty)", sPass);

#ifdef HAVE_LIBSSL
			bSSL = CUtils::GetBoolInput("Does this server use SSL? (probably no)", false);
#endif

			vsLines.push_back("\tServer     = " + sHost + ((bSSL) ? " +" : " ") + CString(uServerPort) + " " + sPass);

			CUtils::PrintMessage("");
		} while (CUtils::GetBoolInput("Would you like to add another server?", false));

		vsLines.push_back("");
		CUtils::PrintMessage("");
		CUtils::PrintMessage("-- Channels --");
		CUtils::PrintMessage("");

		CString sArg = "a";
		CString sPost = " for ZNC to automatically join?";
		bool bDefault = true;

		while (CUtils::GetBoolInput("Would you like to add " + sArg + " channel" + sPost, bDefault)) {
			while (!CUtils::GetInput("Channel name", sAnswer)) ;
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

	CFile File;
	bool bFileOK, bFileOpen = false;
	do {
		CUtils::PrintAction("Writing config [" + sConfigFile + "]");

		bFileOK = true;
		if (CFile::Exists(sConfigFile)) {
			if (!m_LockFile.TryExLock(sConfigFile)) {
				CUtils::PrintStatus(false, "ZNC is currently running on this config.");
				bFileOK = false;
			} else {
				m_LockFile.Close();
				CUtils::PrintStatus(false, "This config already exists.");
				if (CUtils::GetBoolInput("Would you like to overwrite it?", false))
					CUtils::PrintAction("Overwriting config [" + sConfigFile + "]");
				else
					bFileOK = false;
			}
		}

		if (bFileOK) {
			File.SetFileName(sConfigFile);
			if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
				bFileOpen = true;
			} else {
				CUtils::PrintStatus(false, "Unable to open file");
				bFileOK = false;
			}
		}
		if (!bFileOK) {
			CUtils::GetInput("Please specify an alternate location (or \"stdout\" for displaying the config)", sConfigFile, sConfigFile);
			if (sConfigFile.Equals("stdout"))
				bFileOK = true;
			else
				sConfigFile = ExpandConfigPath(sConfigFile);
		}
	} while (!bFileOK);

	if (!bFileOpen) {
		CUtils::PrintMessage("");
		CUtils::PrintMessage("Printing the new config to stdout:");
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
	CUtils::PrintMessage("as the irc server password like so... user:pass.", true);
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Try something like this in your IRC client...", true);
	CUtils::PrintMessage("/server <znc_server_ip> " + CString(uListenPort) + " " + sUser + ":<pass>", true);
	CUtils::PrintMessage("");

	m_LockFile.UnLock();
	return bFileOpen && CUtils::GetBoolInput("Launch znc now?", true);
}

bool CZNC::ParseConfig(const CString& sConfig)
{
	CString s;

	m_sConfigFile = ExpandConfigPath(sConfig);

	return DoRehash(s);
}

bool CZNC::RehashConfig(CString& sError)
{
#ifdef _MODULES
	GetModules().OnPreRehash();
	for (map<CString, CUser*>::iterator itb = m_msUsers.begin();
			itb != m_msUsers.end(); itb++) {
		itb->second->GetModules().OnPreRehash();
	}
#endif

	// This clears m_msDelUsers
	HandleUserDeletion();

	// Mark all users as going-to-be deleted
	m_msDelUsers = m_msUsers;
	m_msUsers.clear();

	if (DoRehash(sError)) {
#ifdef _MODULES
		GetModules().OnPostRehash();
		for (map<CString, CUser*>::iterator it = m_msUsers.begin();
				it != m_msUsers.end(); it++) {
			it->second->GetModules().OnPostRehash();
		}
#endif

		return true;
	}

	// Rehashing failed, try to recover
	CString s;
	while (m_msDelUsers.size()) {
		AddUser(m_msDelUsers.begin()->second, s);
		m_msDelUsers.erase(m_msDelUsers.begin());
	}

	return false;
}

bool CZNC::DoRehash(CString& sError)
{
	sError.clear();

	CUtils::PrintAction("Opening Config [" + m_sConfigFile + "]");

	if (!CFile::Exists(m_sConfigFile)) {
		sError = "No such file";
		CUtils::PrintStatus(false, sError);
		CUtils::PrintMessage("Restart znc with the --makeconf option if you wish to create this config.");
		return false;
	}

	if (!CFile::IsReg(m_sConfigFile)) {
		sError = "Not a file";
		CUtils::PrintStatus(false, sError);
		return false;
	}

	if (!m_LockFile.Open(m_sConfigFile)) {
		sError = "Can not open config file";
		CUtils::PrintStatus(false, sError);
		return false;
	}

	if (!m_LockFile.TryExLock()) {
		sError = "ZNC is already running on this config.";
		CUtils::PrintStatus(false, sError);
		return false;
	}

	CFile File(m_LockFile.GetFD(), m_sConfigFile);

	// This fd is re-used for rehashing, so we must seek back to the beginning!
	if (!File.Seek(0)) {
		sError = "Could not seek to the beginning of the config.";
		CUtils::PrintStatus(false, sError);
		return false;
	}

	CUtils::PrintStatus(true);

	m_vsVHosts.clear();
	m_vsMotd.clear();

	// Delete all listeners
	while (m_vpListeners.size()) {
		delete m_vpListeners[0];
		m_vpListeners.erase(m_vpListeners.begin());
	}

	CString sLine;
	bool bCommented = false;	// support for /**/ style comments
	CUser* pUser = NULL;	// Used to keep track of which user block we are in
	CUser* pRealUser = NULL;	// If we rehash a user, this is the real one
	CChan* pChan = NULL;	// Used to keep track of which chan block we are in
	unsigned int uLineNum = 0;
#ifdef _MODULES
	MCString msModules;	// Modules are queued for later loading
#endif

	std::list<CGlobalModuleConfigLine> lGlobalModuleConfigLine;

	while (File.ReadLine(sLine)) {
		uLineNum++;

		// Remove all leading / trailing spaces and line endings
		sLine.Trim();

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
				sTag = sTag.substr(1);

				if (pUser) {
					if (pChan) {
						if (sTag.Equals("Chan")) {
							// Save the channel name, because AddChan
							// deletes the CChannel*, if adding fails
							sError = pChan->GetName();
							if (!pUser->AddChan(pChan)) {
								sError = "Channel [" + sError + "] defined more than once";
								CUtils::PrintError(sError);
								return false;
							}
							sError.clear();
							pChan = NULL;
							continue;
						}
					} else if (sTag.Equals("User")) {
						CString sErr;

						if (pRealUser) {
							if (!pRealUser->Clone(*pUser, sErr)
									|| !AddUser(pRealUser, sErr)) {
								sError = "Invalid user [" + pUser->GetUserName() + "] " + sErr;
								DEBUG_ONLY(cout << "CUser::Clone() failed in rehash" << endl);
							}
							pUser->SetBeingDeleted(true);
							delete pUser;
							pUser = NULL;
						} else if (!AddUser(pUser, sErr)) {
							sError = "Invalid user [" + pUser->GetUserName() + "] " + sErr;
						}

						if (!sError.empty()) {
							CUtils::PrintError(sError);
							if (pUser) {
								pUser->SetBeingDeleted(true);
								delete pUser;
								pUser = NULL;
							}
							return false;
						}

						pUser = NULL;
						pRealUser = NULL;
						continue;
					}
				}
			} else if (sTag.Equals("User")) {
				if (pUser) {
					sError = "You may not nest <User> tags inside of other <User> tags.";
					CUtils::PrintError(sError);
					return false;
				}

				if (sValue.empty()) {
					sError = "You must supply a username in the <User> tag.";
					CUtils::PrintError(sError);
					return false;
				}

				if (m_msUsers.find(sValue) != m_msUsers.end()) {
					sError = "User [" + sValue + "] defined more than once.";
					CUtils::PrintError(sError);
					return false;
				}

				CUtils::PrintMessage("Loading user [" + sValue + "]");

				// Either create a CUser* or use an existing one
				map<CString, CUser*>::iterator it = m_msDelUsers.find(sValue);

				if (it != m_msDelUsers.end()) {
					pRealUser = it->second;
					m_msDelUsers.erase(it);
				} else
					pRealUser = NULL;

				pUser = new CUser(sValue);

				if (!m_sStatusPrefix.empty()) {
					if (!pUser->SetStatusPrefix(m_sStatusPrefix)) {
						sError = "Invalid StatusPrefix [" + m_sStatusPrefix + "] Must be 1-5 chars, no spaces.";
						CUtils::PrintError(sError);
						return false;
					}
				}

				continue;
			} else if (sTag.Equals("Chan")) {
				if (!pUser) {
					sError = "<Chan> tags must be nested inside of a <User> tag.";
					CUtils::PrintError(sError);
					return false;
				}

				if (pChan) {
					sError = "You may not nest <Chan> tags inside of other <Chan> tags.";
					CUtils::PrintError(sError);
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

		if ((!sName.empty()) && (!sValue.empty())) {
			if (pUser) {
				if (pChan) {
					if (sName.Equals("Buffer")) {
						pChan->SetBufferCount(strtoul(sValue.c_str(), NULL, 10));
						continue;
					} else if (sName.Equals("KeepBuffer")) {
						pChan->SetKeepBuffer(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("Detached")) {
						pChan->SetDetached(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("AutoCycle")) {
						if (sValue.Equals("true")) {
							CUtils::PrintError("WARNING: AutoCycle has been removed, instead try -> LoadModule = autocycle " + pChan->GetName());
						}
						continue;
					} else if (sName.Equals("Key")) {
						pChan->SetKey(sValue);
						continue;
					} else if (sName.Equals("Modes")) {
						pChan->SetDefaultModes(sValue);
						continue;
					}
				} else {
					if (sName.Equals("Buffer")) {
						pUser->SetBufferCount(strtoul(sValue.c_str(), NULL, 10));
						continue;
					} else if (sName.Equals("KeepBuffer")) {
						pUser->SetKeepBuffer(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("Nick")) {
						pUser->SetNick(sValue);
						continue;
					} else if (sName.Equals("CTCPReply")) {
						pUser->AddCTCPReply(sValue.Token(0), sValue.Token(1, true));
						continue;
					} else if (sName.Equals("QuitMsg")) {
						pUser->SetQuitMsg(sValue);
						continue;
					} else if (sName.Equals("AltNick")) {
						pUser->SetAltNick(sValue);
						continue;
					} else if (sName.Equals("AwaySuffix")) {
						CUtils::PrintMessage("WARNING: AwaySuffix has been depricated, instead try -> LoadModule = awaynick %nick%_" + sValue);
						continue;
					} else if (sName.Equals("AutoCycle")) {
						if (sValue.Equals("true")) {
							CUtils::PrintError("WARNING: AutoCycle has been removed, instead try -> LoadModule = autocycle");
						}
						continue;
					} else if (sName.Equals("Pass")) {
						// There are different formats for this available:
						// Pass = <plain text>
						// Pass = <md5 hash> -
						// Pass = plain#<plain text>
						// Pass = md5#<md5 hash>
						// Pass = md5#<salted md5 hash>#<salt>#
						// The last one is the md5 hash of 'password' + 'salt'
						if (sValue.Right(1) == "-") {
							sValue.RightChomp();
							sValue.Trim();
							pUser->SetPass(sValue, true);
						} else {
							CString sMethod = sValue.Token(0, false, "#");
							CString sPass = sValue.Token(1, true, "#");
							if (sMethod == "md5") {
								CString sSalt = sPass.Token(1, false, "#");
								sPass = sPass.Token(0, false, "#");
								pUser->SetPass(sPass, true, sSalt);
							} else if (sMethod == "plain") {
								pUser->SetPass(sPass, false);
							} else {
								pUser->SetPass(sValue, false);
							}
						}

						continue;
					} else if (sName.Equals("MultiClients")) {
						pUser->SetMultiClients(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("BounceDCCs")) {
						pUser->SetBounceDCCs(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("Ident")) {
						pUser->SetIdent(sValue);
						continue;
					} else if (sName.Equals("DenyLoadMod")) {
						pUser->SetDenyLoadMod(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("Admin")) {
						pUser->SetAdmin(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("DenySetVHost")) {
						pUser->SetDenySetVHost(sValue.Equals("true"));
						continue;
					} else if (sName.Equals("StatusPrefix")) {
						if (!pUser->SetStatusPrefix(sValue)) {
							sError = "Invalid StatusPrefix [" + sValue + "] Must be 1-5 chars, no spaces.";
							CUtils::PrintError(sError);
							return false;
						}
						continue;
					} else if (sName.Equals("DCCLookupMethod")) {
						pUser->SetUseClientIP(sValue.Equals("Client"));
						continue;
					} else if (sName.Equals("RealName")) {
						pUser->SetRealName(sValue);
						continue;
					} else if (sName.Equals("KeepNick")) {
						if (sValue.Equals("true")) {
							CUtils::PrintError("WARNING: KeepNick has been deprecated, instead try -> LoadModule = keepnick");
						}
						continue;
					} else if (sName.Equals("ChanModes")) {
						pUser->SetDefaultChanModes(sValue);
						continue;
					} else if (sName.Equals("VHost")) {
						pUser->SetVHost(sValue);
						continue;
					} else if (sName.Equals("Allow")) {
						pUser->AddAllowedHost(sValue);
						continue;
					} else if (sName.Equals("Server")) {
						CUtils::PrintAction("Adding Server [" + sValue + "]");
						CUtils::PrintStatus(pUser->AddServer(sValue));
						continue;
					} else if (sName.Equals("Chan")) {
						pUser->AddChan(sValue, true);
						continue;
					} else if (sName.Equals("TimestampFormat")) {
						pUser->SetTimestampFormat(sValue);
						continue;
					} else if (sName.Equals("AppendTimestamp")) {
						pUser->SetTimestampAppend(sValue.ToBool());
						continue;
					} else if (sName.Equals("PrependTimestamp")) {
						pUser->SetTimestampPrepend(sValue.ToBool());
						continue;
					} else if (sName.Equals("Timestamp")) {
						if (!sValue.Trim_n().Equals("true")) {
							if (sValue.Trim_n().Equals("append")) {
								pUser->SetTimestampAppend(true);
								pUser->SetTimestampPrepend(false);
							} else if (sValue.Trim_n().Equals("prepend")) {
								pUser->SetTimestampAppend(false);
								pUser->SetTimestampPrepend(true);
							} else if (sValue.Trim_n().Equals("false")) {
								pUser->SetTimestampAppend(false);
								pUser->SetTimestampPrepend(false);
							} else {
								pUser->SetTimestampFormat(sValue);
							}
						}
						continue;
					} else if (sName.Equals("TimezoneOffset")) {
						pUser->SetTimezoneOffset(sValue.ToDouble()); // there is no ToFloat()
						continue;
					} else if (sName.Equals("JoinTries")) {
						pUser->SetJoinTries(sValue.ToUInt());
						continue;
					} else if (sName.Equals("MaxJoins")) {
						pUser->SetMaxJoins(sValue.ToUInt());
						continue;
					} else if (sName.Equals("LoadModule")) {
						CString sModName = sValue.Token(0);
						CUtils::PrintAction("Loading Module [" + sModName + "]");
#ifdef _MODULES
						CString sModRet;
						CString sArgs = sValue.Token(1, true);

						bool bModRet = pUser->GetModules().LoadModule(sModName, sArgs, pUser, sModRet);

						// If the module was loaded, sModRet contains
						// "Loaded Module [name] ..." and we strip away this beginning.
						if (bModRet)
							sModRet = sModRet.Token(1, true, sModName + "] ");

						CUtils::PrintStatus(bModRet, sModRet);
						if (!bModRet) {
							sError = sModRet;
							return false;
						}
#else
						sError = "Modules are not enabled.";
						CUtils::PrintStatus(false, sError);
#endif
						continue;
					}
				}
			} else {
				if (sName.Equals("Listen") || sName.Equals("ListenPort") || sName.Equals("Listen6")) {
					bool bSSL = false;
					bool bIPV6 = sName.Equals("Listen6");
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
					CUtils::PrintAction("Binding to port [" + CString((bSSL) ? "+" : "") + CString(uPort) + "]" + sHostComment + sIPV6Comment);

#ifndef HAVE_IPV6
					if (bIPV6) {
						sError = "IPV6 is not enabled";
						CUtils::PrintStatus(false, sError);
						return false;
					}
#endif

#ifndef HAVE_LIBSSL
					if (bSSL) {
						sError = "SSL is not enabled";
						CUtils::PrintStatus(false, sError);
						return false;
					}
#else
					CString sPemFile = GetPemLocation();

					if (bSSL && !CFile::Exists(sPemFile)) {
						sError = "Unable to locate pem file: [" + sPemFile + "]";
						CUtils::PrintStatus(false, sError);

						// If stdin is e.g. /dev/null and we call GetBoolInput(),
						// we are stuck in an endless loop!
						if (isatty(0) && CUtils::GetBoolInput("Would you like to create a new pem file?", true)) {
							sError.clear();
							WritePemFile();
						} else {
							return false;
						}

						CUtils::PrintAction("Binding to port [+" + CString(uPort) + "]" + sHostComment + sIPV6Comment);
					}
#endif
					if (!uPort) {
						sError = "Invalid port";
						CUtils::PrintStatus(false, sError);
						return false;
					}

					CListener* pListener = new CListener(uPort, sBindHost, bSSL, bIPV6);

					if (!pListener->Listen()) {
						sError = "Unable to bind";
						CUtils::PrintStatus(false, sError);
						delete pListener;
						return false;
					}

					m_vpListeners.push_back(pListener);
					CUtils::PrintStatus(true);

					continue;
				} else if (sName.Equals("LoadModule")) {
#ifdef _MODULES
					CString sModName = sValue.Token(0);
					CString sArgs = sValue.Token(1, true);

					if (msModules.find(sModName) != msModules.end()) {
						sError = "Module [" + sModName +
							"] already loaded";
						CUtils::PrintError(sError);
						return false;
					}
					msModules[sModName] = sArgs;
#else
					CUtils::PrintError("Modules are not enabled.");
#endif
					continue;
				} else if (sName.Equals("ISpoofFormat")) {
					m_sISpoofFormat = sValue;
					continue;
				} else if (sName.Equals("ISpoofFile")) {
					if (sValue.Left(2) == "~/") {
						sValue.LeftChomp(2);
						sValue = GetHomePath() + "/" + sValue;
					}
					m_sISpoofFile = sValue;
					continue;
				} else if (sName.Equals("MOTD")) {
					AddMotd(sValue);
					continue;
				} else if (sName.Equals("VHost")) {
					AddVHost(sValue);
					continue;
				} else if (sName.Equals("PidFile")) {
					m_sPidFile = sValue;
					continue;
				} else if (sName.Equals("StatusPrefix")) {
					m_sStatusPrefix = sValue;
					continue;
				} else if (sName.Equals("ConnectDelay")) {
					m_uiConnectDelay = sValue.ToUInt();
					continue;
				}
			}

		}

		if (sName.Equals("GM:", false, 3))
		{ // GM: prefix is a pass through to config lines for global modules
			CGlobalModuleConfigLine cTmp;
			cTmp.m_sName = sName.substr(3, CString::npos);
			cTmp.m_sValue = sValue;
			cTmp.m_pChan = pChan;
			cTmp.m_pUser = pUser;
			lGlobalModuleConfigLine.push_back(cTmp);
		}
		else
		{
			sError = "Unhandled line " + CString(uLineNum) + " in config: [" + sLine + "]";
			CUtils::PrintError(sError);
			return false;
		}
	}

#ifdef _MODULES
	// First step: Load and reload new modules or modules with new arguments
	for (MCString::iterator it = msModules.begin(); it != msModules.end(); it++) {
		CString sModName = it->first;
		CString sArgs = it->second;
		CString sModRet;
		CModule *pOldMod;

		pOldMod = GetModules().FindModule(sModName);
		if (!pOldMod) {
			CUtils::PrintAction("Loading Global Module [" + sModName + "]");

			bool bModRet = GetModules().LoadModule(sModName, sArgs, NULL, sModRet);

			// If the module was loaded, sModRet contains
			// "Loaded Module [name] ..." and we strip away this beginning.
			if (bModRet)
				sModRet = sModRet.Token(1, true, sModName + "] ");

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}
		} else if (pOldMod->GetArgs() != sArgs) {
			CUtils::PrintAction("Reloading Global Module [" + sModName + "]");

			bool bModRet = GetModules().ReloadModule(sModName, sArgs, NULL, sModRet);

			// If the module was loaded, sModRet contains
			// "Loaded Module [name] ..." and we strip away this beginning.
			if (bModRet)
				sModRet = sModRet.Token(1, true, sModName + "] ");

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}
		} else
			CUtils::PrintMessage("Module [" + sModName + "] already loaded.");
	}

	// Second step: Unload modules which are no longer in the config
	set<CString> ssUnload;
	for (size_t i = 0; i < GetModules().size(); i++) {
		CModule *pCurMod = GetModules()[i];

		if (msModules.find(pCurMod->GetModName()) == msModules.end())
			ssUnload.insert(pCurMod->GetModName());
	}

	for (set<CString>::iterator it = ssUnload.begin(); it != ssUnload.end(); it++) {
		if (GetModules().UnloadModule(*it))
			CUtils::PrintMessage("Unloaded Global Module [" + *it + "]");
		else
			CUtils::PrintMessage("Could not unload [" + *it + "]");
	}

	// last step, throw unhandled config items at global config
	for (std::list<CGlobalModuleConfigLine>::iterator it = lGlobalModuleConfigLine.begin(); it != lGlobalModuleConfigLine.end(); it++)
	{
		if ((pChan && pChan == it->m_pChan) || (pUser && pUser == it->m_pUser))
			continue; // skip unclosed user or chan
		if (!GetModules().OnConfigLine(it->m_sName, it->m_sValue, it->m_pUser, it->m_pChan))
		{
			CUtils::PrintMessage("unhandled global module config line [GM:" + it->m_sName + "] = [" + it->m_sValue + "]");
		}
	}
#endif

	if (pChan) {
		// TODO last <Chan> not closed
		delete pChan;
	}

	if (pUser) {
		// TODO last <User> not closed
		delete pUser;
	}

	File.Close();

	if (m_msUsers.size() == 0) {
		sError = "You must define at least one user in your config.";
		CUtils::PrintError(sError);
		return false;
	}

	if (m_vpListeners.size() == 0) {
		sError = "You must supply at least one Listen port in your config.";
		CUtils::PrintError(sError);
		return false;
	}

	// Make sure that users that want to connect do so
	EnableConnectUser();

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
		if (m_vsVHosts[a].Equals(sHost)) {
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

void CZNC::Broadcast(const CString& sMessage, bool bAdminOnly,
		CUser* pSkipUser, CClient *pSkipClient) {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); a++) {
		if (bAdminOnly && !a->second->IsAdmin())
			continue;

		if (a->second != pSkipUser) {
			CString sMsg = sMessage;

			MODULECALL(OnBroadcast(sMsg), a->second, NULL, continue);
			a->second->PutStatusNotice("*** " + sMsg, NULL, pSkipClient);
		}
	}
}

bool CZNC::FindModPath(const CString& sModule, CString& sModPath,
		CString& sDataPath) const {
	CString sMod = sModule;
	CString sDir = sMod;
	if (sModule.find(".") == CString::npos)
		sMod += ".so";

	sDataPath = GetCurPath() + "/modules/";
	sModPath = sDataPath + sMod;

	if (!CFile::Exists(sModPath)) {
		sDataPath = GetModPath() + "/";
		sModPath = sDataPath + sMod;

		if (!CFile::Exists(sModPath)) {
			sDataPath = _DATADIR_ + CString("/");
			sModPath = _MODDIR_ + CString("/") + sMod;

			if (!CFile::Exists(sModPath)) {
				return false;
			}
		}
	}

	sDataPath += sDir;

	return true;
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

	m_msDelUsers[pUser->GetUserName()] = pUser;
	return true;
}

bool CZNC::AddUser(CUser* pUser, CString& sErrorRet) {
	if (pUser->IsValid(sErrorRet)) {
		m_msUsers[pUser->GetUserName()] = pUser;
		return true;
	}

	DEBUG_ONLY(cout << "Invalid user [" << pUser->GetUserName() << "] - [" << sErrorRet << "]" << endl);
	return false;
}

CZNC& CZNC::Get() {
	static CZNC* pZNC = new CZNC;
	return *pZNC;
}

void CZNC::UpdateTrafficStats() {
	CSockManager* p = &m_Manager;
	for (unsigned int a = 0; a < p->size(); a++) {
		if ((*p)[a]->GetSockName().Left(5) == "IRC::") {
			CIRCSock *i = (CIRCSock *)(*p)[a];
			i->GetUser()->AddBytesRead((*p)[a]->GetBytesRead());
			(*p)[a]->ResetBytesRead();
			i->GetUser()->AddBytesWritten((*p)[a]->GetBytesWritten());
			(*p)[a]->ResetBytesWritten();
		} else if ((*p)[a]->GetSockName().Left(5) == "USR::") {
			CClient *c = (CClient *)(*p)[a];
			c->GetUser()->AddBytesRead((*p)[a]->GetBytesRead());
			(*p)[a]->ResetBytesRead();
			c->GetUser()->AddBytesWritten((*p)[a]->GetBytesWritten());
			(*p)[a]->ResetBytesWritten();
		}
	}
}

void CZNC::AuthUser(CSmartPtr<CAuthBase> AuthClass) {
#ifdef _MODULES
	// TODO unless the auth module calls it, CUser::IsHostAllowed() is not honoured
	if (GetModules().OnLoginAttempt(AuthClass)) {
		return;
	}
#endif

	CUser* pUser = GetUser(AuthClass->GetUsername());

	if (!pUser || !pUser->CheckPass(AuthClass->GetPassword())) {
		if (pUser) {
			pUser->PutStatus("Another client attempted to login as you, with a bad password.");
		}

		AuthClass->RefuseLogin("Invalid Password");
		return;
	}

	CString sHost = AuthClass->GetRemoteIP();

	if (!pUser->IsHostAllowed(sHost)) {
		AuthClass->RefuseLogin("Your host [" + sHost + "] is not allowed");
		return;
	}

	AuthClass->AcceptLogin(*pUser);
}

class CConnectUserTimer : public CCron {
public:
	CConnectUserTimer(int iSecs) : CCron() {
		SetName("Connect users");
		Start(iSecs);
		m_uiPosNextUser = 0;
		// Don't wait iSecs seconds for first timer run
		m_bRunOnNextCall = true;
	}
	virtual ~CConnectUserTimer() {}

protected:
	virtual void RunJob() {
		unsigned int uiUserCount;
		bool bUsersLeft = false;
		const map<CString,CUser*>& mUsers = CZNC::Get().GetUserMap();
		map<CString,CUser*>::const_iterator it = mUsers.begin();

		uiUserCount = CZNC::Get().GetUserMap().size();

		if (m_uiPosNextUser >= uiUserCount) {
			m_uiPosNextUser = 0;
		}

		for (unsigned int i = 0; i < m_uiPosNextUser; i++) {
			it++;
		}

		// Try to connect each user, if this doesnt work, abort
		for (unsigned int i = 0; i < uiUserCount; i++) {
			if (it == mUsers.end())
				it = mUsers.begin();

			CUser* pUser = it->second;
			it++;
			m_uiPosNextUser = (m_uiPosNextUser + 1) % uiUserCount;

			// Is this user disconnected?
			if (pUser->GetIRCSock() != NULL)
				continue;

			// Does this user want to connect?
			if (!pUser->GetIRCConnectEnabled())
				continue;

			// Does this user have any servers?
			if (!pUser->HasServers())
				continue;

			// The timer runs until it once didn't find any users to connect
			bUsersLeft = true;

			DEBUG_ONLY(cout << "Connecting user [" << pUser->GetUserName()
					<< "]" << endl);

			if (CZNC::Get().ConnectUser(pUser))
				// User connecting, wait until next time timer fires
				return;
		}

		if (bUsersLeft == false) {
			DEBUG_ONLY(cout << "ConnectUserTimer done" << endl);
			CZNC::Get().DisableConnectUser();
		}
	}

private:
	size_t	m_uiPosNextUser;
};

void CZNC::EnableConnectUser() {
	if (m_pConnectUserTimer != NULL)
		return;

	m_pConnectUserTimer = new CConnectUserTimer(m_uiConnectDelay);
	GetManager().AddCron(m_pConnectUserTimer);
}

void CZNC::DisableConnectUser() {
	if (m_pConnectUserTimer == NULL)
		return;

	// This will kill the cron
	m_pConnectUserTimer->Stop();
	m_pConnectUserTimer = NULL;
}
