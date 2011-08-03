/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "Chan.h"
#include "FileUtils.h"
#include "IRCSock.h"
#include "Server.h"
#include "User.h"
#include "Listener.h"
#include "Config.h"
#include <list>

static inline CString FormatBindError() {
	CString sError = (errno == 0 ? CString("unknown error, check the host name") : CString(strerror(errno)));
	return "Unable to bind [" + sError + "]";
}

CZNC::CZNC() {
	if (!InitCsocket()) {
		CUtils::PrintError("Could not initialize Csocket!");
		exit(-1);
	}

	m_pModules = new CGlobalModules();
	m_uiConnectDelay = 5;
	m_uiAnonIPLimit = 10;
	m_uBytesRead = 0;
	m_uBytesWritten = 0;
	m_uiMaxBufferSize = 500;
	m_pConnectUserTimer = NULL;
	m_eConfigState = ECONFIG_NOTHING;
	m_TimeStarted = time(NULL);
	m_sConnectThrottle.SetTTL(30000);
	m_pLockFile = NULL;
	m_bProtectWebSessions = true;
}

CZNC::~CZNC() {
	m_pModules->UnloadAll();

	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); ++a) {
		a->second->GetModules().UnloadAll();
	}

	for (size_t b = 0; b < m_vpListeners.size(); b++) {
		delete m_vpListeners[b];
	}

	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); ++a) {
		a->second->SetBeingDeleted(true);
	}

	m_pConnectUserTimer = NULL;
	// This deletes m_pConnectUserTimer
	m_Manager.Cleanup();
	DeleteUsers();

	delete m_pModules;
	delete m_pLockFile;

	ShutdownCsocket();
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
		return "ZNC - http://znc.in";
	}

	char szBuf[128];
	snprintf(szBuf, sizeof(szBuf), "ZNC %1.3f"VERSION_EXTRA" - http://znc.in", VERSION);
	// If snprintf overflows (which I doubt), we want to be on the safe side
	szBuf[sizeof(szBuf) - 1] = '\0';

	return szBuf;
}

CString CZNC::GetUptime() const {
	time_t now = time(NULL);
	return CString::ToTimeStr(now - TimeStarted());
}

bool CZNC::OnBoot() {
	ALLMODULECALL(OnBoot(), return false);

	return true;
}

bool CZNC::ConnectUser(CUser *pUser) {
	CString sSockName = "IRC::" + pUser->GetUserName();
	CIRCSock* pIRCSock = pUser->GetIRCSock();

	if (!pUser->GetIRCConnectEnabled())
		return false;

	if (pIRCSock || !pUser->HasServers())
		return false;

	CServer* pServer = pUser->GetNextServer();

	if (!pServer)
		return false;

	if (m_sConnectThrottle.GetItem(pServer->GetName()))
		return false;

	m_sConnectThrottle.AddItem(pServer->GetName());

	DEBUG("User [" << pUser->GetUserName() << "] is connecting to [" << pServer->GetString(false) << "] ...");
	pUser->PutStatus("Attempting to connect to [" + pServer->GetString(false) + "] ...");

	pIRCSock = new CIRCSock(pUser);
	pIRCSock->SetPass(pServer->GetPass());

	bool bSSL = false;
#ifdef HAVE_LIBSSL
	if (pServer->IsSSL()) {
		bSSL = true;
	}
#endif

	MODULECALL(OnIRCConnecting(pIRCSock), pUser, NULL,
		DEBUG("Some module aborted the connection attempt");
		pUser->PutStatus("Some module aborted the connection attempt");
		delete pIRCSock;
		return false;
	);

	if (!m_Manager.Connect(pServer->GetName(), pServer->GetPort(), sSockName, 120, bSSL, pUser->GetBindHost(), pIRCSock)) {
		pUser->PutStatus("Unable to connect. (Bad host?)");
	}

	return true;
}

bool CZNC::HandleUserDeletion()
{
	map<CString, CUser*>::iterator it;
	map<CString, CUser*>::iterator end;

	if (m_msDelUsers.empty())
		return false;

	end = m_msDelUsers.end();
	for (it = m_msDelUsers.begin(); it != end; ++it) {
		CUser* pUser = it->second;
		pUser->SetBeingDeleted(true);

		if (GetModules().OnDeleteUser(*pUser)) {
			pUser->SetBeingDeleted(false);
			continue;
		}
		m_msUsers.erase(pUser->GetUserName());

		CIRCSock* pIRCSock = pUser->GetIRCSock();

		if (pIRCSock) {
			m_Manager.DelSockByAddr(pIRCSock);
		}

		pUser->DelClients();
		pUser->DelModules();
		CWebSock::FinishUserSessions(*pUser);
		AddBytesRead(pUser->BytesRead());
		AddBytesWritten(pUser->BytesWritten());
		delete pUser;
	}

	m_msDelUsers.clear();

	return true;
}

void CZNC::Loop() {
	while (true) {
		CString sError;

		switch (GetConfigState()) {
		case ECONFIG_NEED_REHASH:
			SetConfigState(ECONFIG_NOTHING);

			if (RehashConfig(sError)) {
				Broadcast("Rehashing succeeded", true);
			} else {
				Broadcast("Rehashing failed: " + sError, true);
				Broadcast("ZNC is in some possibly inconsistent state!", true);
			}
			break;
		case ECONFIG_NEED_WRITE:
			SetConfigState(ECONFIG_NOTHING);

			if (WriteConfig()) {
				Broadcast("Writing the config succeeded", true);
			} else {
				Broadcast("Writing the config file failed", true);
			}
			break;
		case ECONFIG_NOTHING:
			break;
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
}

CFile* CZNC::InitPidFile() {
	if (!m_sPidFile.empty()) {
		CString sFile;

		// absolute path or relative to the data dir?
		if (m_sPidFile[0] != '/')
			sFile = GetZNCPath() + "/" + m_sPidFile;
		else
			sFile = m_sPidFile;

		return new CFile(sFile);
	}

	return NULL;
}

bool CZNC::WritePidFile(int iPid) {
	CFile* File = InitPidFile();
	if (File == NULL)
		return false;

	CUtils::PrintAction("Writing pid file [" + File->GetLongName() + "]");

	bool bRet = false;
	if (File->Open(O_WRONLY | O_TRUNC | O_CREAT)) {
		File->Write(CString(iPid) + "\n");
		File->Close();
		bRet = true;
	}

	delete File;
	CUtils::PrintStatus(bRet);
	return bRet;
}

bool CZNC::DeletePidFile() {
	CFile* File = InitPidFile();
	if (File == NULL)
		return false;

	CUtils::PrintAction("Deleting pid file [" + File->GetLongName() + "]");

	bool bRet = File->Delete();

	delete File;
	CUtils::PrintStatus(bRet);
	return bRet;
}

bool CZNC::WritePemFile() {
#ifndef HAVE_LIBSSL
	CUtils::PrintError("ZNC was not compiled with ssl support.");
	return false;
#else
	CString sPemFile = GetPemLocation();

	CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");
	FILE *f = fopen(sPemFile.c_str(), "w");

	if (!f) {
		CUtils::PrintStatus(false, "Unable to open");
		return false;
	}

	CUtils::GenerateCert(f, "");
	fclose(f);

	CUtils::PrintStatus(true);
	return true;
#endif
}

void CZNC::DeleteUsers() {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); ++a) {
		a->second->SetBeingDeleted(true);
		delete a->second;
	}

	m_msUsers.clear();
	DisableConnectUser();
}

bool CZNC::IsHostAllowed(const CString& sHostMask) const {
	for (map<CString,CUser*>::const_iterator a = m_msUsers.begin(); a != m_msUsers.end(); ++a) {
		if (a->second->IsHostAllowed(sHostMask)) {
			return true;
		}
	}

	return false;
}

bool CZNC::AllowConnectionFrom(const CString& sIP) const {
	if (m_uiAnonIPLimit == 0)
		return true;
	return (GetManager().GetAnonConnectionCount(sIP) < m_uiAnonIPLimit);
}

void CZNC::InitDirs(const CString& sArgvPath, const CString& sDataDir) {
	// If the bin was not ran from the current directory, we need to add that dir onto our cwd
	CString::size_type uPos = sArgvPath.rfind('/');
	if (uPos == CString::npos)
		m_sCurPath = "./";
	else
		m_sCurPath = CDir::ChangeDir("./", sArgvPath.Left(uPos), "");

	// Try to set the user's home dir, default to binpath on failure
	CFile::InitHomePath(m_sCurPath);

	if (sDataDir.empty()) {
		m_sZNCPath = CFile::GetHomePath() + "/.znc";
	} else {
		m_sZNCPath = sDataDir;
	}

	m_sSSLCertFile = m_sZNCPath + "/znc.pem";
}

CString CZNC::GetConfPath(bool bAllowMkDir) const {
	CString sConfPath = m_sZNCPath + "/configs";
	if (bAllowMkDir && !CFile::Exists(sConfPath)) {
		CDir::MakeDir(sConfPath);
	}

	return sConfPath;
}

CString CZNC::GetUserPath() const {
	CString sUserPath = m_sZNCPath + "/users";
	if (!CFile::Exists(sUserPath)) {
		CDir::MakeDir(sUserPath);
	}

	return sUserPath;
}

CString CZNC::GetModPath() const {
	CString sModPath = m_sZNCPath + "/modules";

	if (!CFile::Exists(sModPath)) {
		CDir::MakeDir(sModPath);
	}

	return sModPath;
}

const CString& CZNC::GetCurPath() const {
	if (!CFile::Exists(m_sCurPath)) {
		CDir::MakeDir(m_sCurPath);
	}
	return m_sCurPath;
}

const CString& CZNC::GetHomePath() const {
	return CFile::GetHomePath();
}

const CString& CZNC::GetZNCPath() const {
	if (!CFile::Exists(m_sZNCPath)) {
		CDir::MakeDir(m_sZNCPath);
	}
	return m_sZNCPath;
}

CString CZNC::GetPemLocation() const {
	return CDir::ChangeDir("", m_sSSLCertFile);
}

CString CZNC::ExpandConfigPath(const CString& sConfigFile, bool bAllowMkDir) {
	CString sRetPath;

	if (sConfigFile.empty()) {
		sRetPath = GetConfPath(bAllowMkDir) + "/znc.conf";
	} else {
		if (sConfigFile.Left(2) == "./" || sConfigFile.Left(3) == "../") {
			sRetPath = GetCurPath() + "/" + sConfigFile;
		} else if (sConfigFile.Left(1) != "/") {
			sRetPath = GetConfPath(bAllowMkDir) + "/" + sConfigFile;
		} else {
			sRetPath = sConfigFile;
		}
	}

	return sRetPath;
}

bool CZNC::WriteConfig() {
	if (GetConfigFile().empty()) {
		return false;
	}

	// We first write to a temporary file and then move it to the right place
	CFile *pFile = new CFile(GetConfigFile() + "~");

	if (!pFile->Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
		delete pFile;
		return false;
	}

	// We have to "transfer" our lock on the config to the new file.
	// The old file (= inode) is going away and thus a lock on it would be
	// useless. These lock should always succeed (races, anyone?).
	if (!pFile->TryExLock()) {
		pFile->Delete();
		delete pFile;
		return false;
	}

	pFile->Write(MakeConfigHeader() + "\n");

	pFile->Write("AnonIPLimit  = " + CString(m_uiAnonIPLimit) + "\n");
	pFile->Write("MaxBufferSize= " + CString(m_uiMaxBufferSize) + "\n");
	pFile->Write("SSLCertFile  = " + CString(m_sSSLCertFile) + "\n");
	pFile->Write("ProtectWebSessions = " + CString(m_bProtectWebSessions) + "\n");

	for (size_t l = 0; l < m_vpListeners.size(); l++) {
		CListener* pListener = m_vpListeners[l];
		CString sHostPortion = pListener->GetBindHost();

		if (!sHostPortion.empty()) {
			sHostPortion = sHostPortion.FirstLine() + " ";
		}

		CString sAcceptProtocol;
		if(pListener->GetAcceptType() == CListener::ACCEPT_IRC)
			sAcceptProtocol = "irc_only ";
		else if(pListener->GetAcceptType() == CListener::ACCEPT_HTTP)
			sAcceptProtocol = "web_only ";

		CString s6;
		switch (pListener->GetAddrType()) {
			case ADDR_IPV4ONLY:
				s6 = "4";
				break;
			case ADDR_IPV6ONLY:
				s6 = "6";
				break;
			case ADDR_ALL:
				s6 = " ";
				break;
		}

		pFile->Write("Listener" + s6 + "    = " + sAcceptProtocol + sHostPortion +
			CString((pListener->IsSSL()) ? "+" : "") + CString(pListener->GetPort()) + "\n");
	}

	pFile->Write("ConnectDelay = " + CString(m_uiConnectDelay) + "\n");
	pFile->Write("ServerThrottle = " + CString(m_sConnectThrottle.GetTTL()/1000) + "\n");

	if (!m_sPidFile.empty()) {
		pFile->Write("PidFile      = " + m_sPidFile.FirstLine() + "\n");
	}

	if (!m_sSkinName.empty()) {
		pFile->Write("Skin         = " + m_sSkinName.FirstLine() + "\n");
	}

	if (!m_sStatusPrefix.empty()) {
		pFile->Write("StatusPrefix = " + m_sStatusPrefix.FirstLine() + "\n");
	}

	for (unsigned int m = 0; m < m_vsMotd.size(); m++) {
		pFile->Write("Motd         = " + m_vsMotd[m].FirstLine() + "\n");
	}

	for (unsigned int v = 0; v < m_vsBindHosts.size(); v++) {
		pFile->Write("BindHost     = " + m_vsBindHosts[v].FirstLine() + "\n");
	}

	CGlobalModules& Mods = GetModules();

	for (unsigned int a = 0; a < Mods.size(); a++) {
		CString sName = Mods[a]->GetModName();
		CString sArgs = Mods[a]->GetArgs();

		if (!sArgs.empty()) {
			sArgs = " " + sArgs.FirstLine();
		}

		pFile->Write("LoadModule   = " + sName.FirstLine() + sArgs + "\n");
	}

	for (map<CString,CUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
		CString sErr;

		if (!it->second->IsValid(sErr)) {
			DEBUG("** Error writing config for user [" << it->first << "] [" << sErr << "]");
			continue;
		}

		pFile->Write("\n");

		if (!it->second->WriteConfig(*pFile)) {
			DEBUG("** Error writing config for user [" << it->first << "]");
		}
	}

	// If Sync() fails... well, let's hope nothing important breaks..
	pFile->Sync();

	if (pFile->HadError()) {
		DEBUG("Error while writing the config, errno says: " + CString(strerror(errno)));
		pFile->Delete();
		delete pFile;
		return false;
	}

	// We wrote to a temporary name, move it to the right place
	if (!pFile->Move(GetConfigFile(), true)) {
		DEBUG("Error while replacing the config file with a new version");
		pFile->Delete();
		delete pFile;
		return false;
	}

	// Everything went fine, just need to update the saved path.
	pFile->SetFileName(GetConfigFile());

	// Make sure the lock is kept alive as long as we need it.
	delete m_pLockFile;
	m_pLockFile = pFile;

	return true;
}

CString CZNC::MakeConfigHeader() {
	return
		"// WARNING\n"
		"//\n"
		"// Do NOT edit this file while ZNC is running!\n"
		"// Use webadmin or *admin instead.\n"
		"//\n"
		"// Buf if you feel risky, you might want to read help on /znc saveconfig and /znc rehash.\n"
		"// Also check http://en.znc.in/wiki/Configuration\n";
}

bool CZNC::WriteNewConfig(const CString& sConfigFile) {
	CString sAnswer, sUser;
	VCString vsLines;

	vsLines.push_back(MakeConfigHeader());

	m_sConfigFile = ExpandConfigPath(sConfigFile);
	CUtils::PrintMessage("Building new config");

	CUtils::PrintMessage("");
	CUtils::PrintMessage("First let's start with some global settings...");
	CUtils::PrintMessage("");

	// Listen
#ifdef HAVE_IPV6
	bool b6 = true;
#else
	bool b6 = false;
#endif
	CString sListenHost;
	CString sSSL;
	unsigned int uListenPort = 0;
	bool bSuccess;

	do {
		bSuccess = true;
		while (!CUtils::GetNumInput("What port would you like ZNC to listen on?", uListenPort, 1, 65535)) ;

#ifdef HAVE_LIBSSL
		if (CUtils::GetBoolInput("Would you like ZNC to listen using SSL?", !sSSL.empty())) {
			sSSL = "+";

			CString sPemFile = GetPemLocation();
			if (!CFile::Exists(sPemFile)) {
				CUtils::PrintError("Unable to locate pem file: [" + sPemFile + "]");
				if (CUtils::GetBoolInput("Would you like to create a new pem file now?",
							true)) {
					WritePemFile();
				}
			}
		} else
			sSSL = "";
#endif

#ifdef HAVE_IPV6
		b6 = CUtils::GetBoolInput("Would you like ZNC to listen using ipv6?", b6);
#endif

		CUtils::GetInput("Listen Host", sListenHost, sListenHost, "Blank for all ips");

		CUtils::PrintAction("Verifying the listener");
		CListener* pListener = new CListener(uListenPort, sListenHost, !sSSL.empty(),
				b6 ? ADDR_ALL : ADDR_IPV4ONLY, CListener::ACCEPT_ALL);
		if (!pListener->Listen()) {
			CUtils::PrintStatus(false, FormatBindError());
			bSuccess = false;
		} else
			CUtils::PrintStatus(true);
		delete pListener;
	} while (!bSuccess);

	if (!sListenHost.empty()) {
		sListenHost += " ";
	}

	vsLines.push_back("Listener" + CString(b6 ? " " : "4") + "  = " + sListenHost + sSSL + CString(uListenPort));
	// !Listen

	set<CModInfo> ssGlobalMods;
	GetModules().GetAvailableMods(ssGlobalMods, true);
	size_t uNrOtherGlobalMods = FilterUncommonModules(ssGlobalMods);

	if (!ssGlobalMods.empty()) {
		CUtils::PrintMessage("");
		CUtils::PrintMessage("-- Global Modules --");
		CUtils::PrintMessage("");

		CTable Table;
		Table.AddColumn("Name");
		Table.AddColumn("Description");
		set<CModInfo>::iterator it;

		for (it = ssGlobalMods.begin(); it != ssGlobalMods.end(); ++it) {
			const CModInfo& Info = *it;
			Table.AddRow();
			Table.SetCell("Name", Info.GetName());
			Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
		}

		unsigned int uTableIdx = 0; CString sLine;
		while (Table.GetLine(uTableIdx++, sLine)) {
			CUtils::PrintMessage(sLine);
		}

		if (uNrOtherGlobalMods > 0) {
			CUtils::PrintMessage("And " + CString(uNrOtherGlobalMods) + " other (uncommon) modules. You can enable those later.");
		}

		CUtils::PrintMessage("");

		for (it = ssGlobalMods.begin(); it != ssGlobalMods.end(); ++it) {
			const CModInfo& Info = *it;
			CString sName = Info.GetName();

			if (CDebug::StdoutIsTTY()) {
				if (CUtils::GetBoolInput("Load global module <\033[1m" + sName + "\033[22m>?", false))
					vsLines.push_back("LoadModule = " + sName);
			} else {
				if (CUtils::GetBoolInput("Load global module <" + sName + ">?", false))
					vsLines.push_back("LoadModule = " + sName);
			}
		}
	}

	// User
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Now we need to set up a user...");
	CUtils::PrintMessage("ZNC needs one user per IRC network.");
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
		vsLines.push_back("\tPass       = " + CUtils::sDefaultHash + "#" + sAnswer + "#" + sSalt + "#");

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
		CUtils::GetInput("Bind Host", sAnswer, "", "optional");
		if (!sAnswer.empty()) {
			vsLines.push_back("\tBindHost   = " + sAnswer);
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

		set<CModInfo> ssUserMods;
		GetModules().GetAvailableMods(ssUserMods);
		size_t uNrOtherUserMods = FilterUncommonModules(ssUserMods);

		if (ssUserMods.size()) {
			vsLines.push_back("");
			CUtils::PrintMessage("");
			CUtils::PrintMessage("-- User Modules --");
			CUtils::PrintMessage("");

			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");
			set<CModInfo>::iterator it;

			for (it = ssUserMods.begin(); it != ssUserMods.end(); ++it) {
				const CModInfo& Info = *it;
				Table.AddRow();
				Table.SetCell("Name", Info.GetName());
				Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
			}

			unsigned int uTableIdx = 0; CString sLine;
			while (Table.GetLine(uTableIdx++, sLine)) {
				CUtils::PrintMessage(sLine);
			}

			if (uNrOtherUserMods > 0) {
				CUtils::PrintMessage("And " + CString(uNrOtherUserMods) + " other (uncommon) modules. You can enable those later.");
			}

			CUtils::PrintMessage("");

			for (it = ssUserMods.begin(); it != ssUserMods.end(); ++it) {
				const CModInfo& Info = *it;
				CString sName = Info.GetName();

				if (CDebug::StdoutIsTTY()) {
					if (CUtils::GetBoolInput("Load module <\033[1m" + sName + "\033[22m>?", false))
						vsLines.push_back("\tLoadModule = " + sName);
				} else {
					if (CUtils::GetBoolInput("Load module <" + sName + ">?", false))
						vsLines.push_back("\tLoadModule = " + sName);
				}
			}
		}

		vsLines.push_back("");
		CUtils::PrintMessage("");
		CUtils::PrintMessage("-- IRC Servers --");
		CUtils::PrintMessage("Only add servers from the same IRC network.");
		CUtils::PrintMessage("If a server from the list can't be reached, another server will be used.");
		CUtils::PrintMessage("");

		do {
			CString sHost, sPass;
			bool bSSL = false;
			unsigned int uServerPort = 0;

			while (!CUtils::GetInput("IRC server", sHost, "", "host only") || !CServer::IsValidHostName(sHost)) ;
			while (!CUtils::GetNumInput("[" + sHost + "] Port", uServerPort, 1, 65535, 6667)) ;
			CUtils::GetInput("[" + sHost + "] Password (probably empty)", sPass);

#ifdef HAVE_LIBSSL
			bSSL = CUtils::GetBoolInput("Does this server use SSL?", false);
#endif

			vsLines.push_back("\tServer     = " + sHost + ((bSSL) ? " +" : " ") + CString(uServerPort) + " " + sPass);

			CUtils::PrintMessage("");
		} while (CUtils::GetBoolInput("Would you like to add another server for this IRC network?", false));

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
	} while (CUtils::GetBoolInput("Would you like to set up another user (e.g. for connecting to another network)?", false));
	// !User

	CFile File;
	bool bFileOK, bFileOpen = false;
	do {
		CUtils::PrintAction("Writing config [" + m_sConfigFile + "]");

		bFileOK = true;
		if (CFile::Exists(m_sConfigFile)) {
			if (!File.TryExLock(m_sConfigFile)) {
				CUtils::PrintStatus(false, "ZNC is currently running on this config.");
				bFileOK = false;
			} else {
				File.Close();
				CUtils::PrintStatus(false, "This config already exists.");
				if (CUtils::GetBoolInput("Would you like to overwrite it?", false))
					CUtils::PrintAction("Overwriting config [" + m_sConfigFile + "]");
				else
					bFileOK = false;
			}
		}

		if (bFileOK) {
			File.SetFileName(m_sConfigFile);
			if (File.Open(O_WRONLY | O_CREAT | O_TRUNC, 0600)) {
				bFileOpen = true;
			} else {
				CUtils::PrintStatus(false, "Unable to open file");
				bFileOK = false;
			}
		}
		if (!bFileOK) {
			CUtils::GetInput("Please specify an alternate location (or \"stdout\" for displaying the config)", m_sConfigFile, m_sConfigFile);
			if (m_sConfigFile.Equals("stdout"))
				bFileOK = true;
			else
				m_sConfigFile = ExpandConfigPath(m_sConfigFile);
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
		if (File.HadError())
			CUtils::PrintStatus(false, "There was an error while writing the config");
		else
			CUtils::PrintStatus(true);
	} else {
		cout << endl << "----------------------------------------------------------------------------" << endl << endl;
	}

	if (File.HadError()) {
		bFileOpen = false;
		CUtils::PrintMessage("Printing the new config to stdout instead:");
		cout << endl << "----------------------------------------------------------------------------" << endl << endl;
		for (unsigned int a = 0; a < vsLines.size(); a++) {
			cout << vsLines[a] << endl;
		}
		cout << endl << "----------------------------------------------------------------------------" << endl << endl;
	}

	const CString sProtocol(sSSL.empty() ? "http" : "https");
	CUtils::PrintMessage("");
	CUtils::PrintMessage("To connect to this ZNC you need to connect to it as your IRC server", true);
	CUtils::PrintMessage("using the port that you supplied.  You have to supply your login info", true);
	CUtils::PrintMessage("as the IRC server password like this: user:pass.", true);
	CUtils::PrintMessage("");
	CUtils::PrintMessage("Try something like this in your IRC client...", true);
	CUtils::PrintMessage("/server <znc_server_ip> " + sSSL + CString(uListenPort) + " " + sUser + ":<pass>", true);
	CUtils::PrintMessage("And this in your browser...", true);
	CUtils::PrintMessage(sProtocol + "://<znc_server_ip>:" + CString(uListenPort) + "/", true);
	CUtils::PrintMessage("");

	File.UnLock();
	return bFileOpen && CUtils::GetBoolInput("Launch ZNC now?", true);
}

size_t CZNC::FilterUncommonModules(set<CModInfo>& ssModules) {
	const char* ns[] = { "webadmin", "admin",
		"chansaver", "keepnick", "simple_away", "partyline",
		"kickrejoin", "nickserv", "perform" };
	const set<CString> ssNames(ns, ns + sizeof(ns) / sizeof(ns[0]));

	size_t uNrRemoved = 0;
	for(set<CModInfo>::iterator it = ssModules.begin(); it != ssModules.end(); ) {
		if(ssNames.count(it->GetName()) > 0) {
			it++;
		} else {
			set<CModInfo>::iterator it2 = it++;
			ssModules.erase(it2);
			uNrRemoved++;
		}
	}

	return uNrRemoved;
}

bool CZNC::ParseConfig(const CString& sConfig)
{
	CString s;

	m_sConfigFile = ExpandConfigPath(sConfig, false);

	return DoRehash(s);
}

bool CZNC::RehashConfig(CString& sError)
{
	ALLMODULECALL(OnPreRehash(), NOTHING);

	// This clears m_msDelUsers
	HandleUserDeletion();

	// Mark all users as going-to-be deleted
	m_msDelUsers = m_msUsers;
	m_msUsers.clear();

	if (DoRehash(sError)) {
		ALLMODULECALL(OnPostRehash(), NOTHING);

		return true;
	}

	// Rehashing failed, try to recover
	CString s;
	while (!m_msDelUsers.empty()) {
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
		CUtils::PrintMessage("Restart ZNC with the --makeconf option if you wish to create this config.");
		return false;
	}

	if (!CFile::IsReg(m_sConfigFile)) {
		sError = "Not a file";
		CUtils::PrintStatus(false, sError);
		return false;
	}

	CFile *pFile = new CFile(m_sConfigFile);

	// need to open the config file Read/Write for fcntl()
	// exclusive locking to work properly!
	if (!pFile->Open(m_sConfigFile, O_RDWR)) {
		sError = "Can not open config file";
		CUtils::PrintStatus(false, sError);
		delete pFile;
		return false;
	}

	if (!pFile->TryExLock()) {
		sError = "ZNC is already running on this config.";
		CUtils::PrintStatus(false, sError);
		delete pFile;
		return false;
	}

	// (re)open the config file
	delete m_pLockFile;
	m_pLockFile = pFile;
	CFile &File = *pFile;

	CConfig config;
	if (!config.Parse(File, sError)) {
		CUtils::PrintStatus(false, sError);
		return false;
	}
	CUtils::PrintStatus(true);

	m_vsBindHosts.clear();
	m_vsMotd.clear();

	// Delete all listeners
	while (!m_vpListeners.empty()) {
		delete m_vpListeners[0];
		m_vpListeners.erase(m_vpListeners.begin());
	}

	MCString msModules;          // Modules are queued for later loading

	VCString vsList;
	VCString::const_iterator vit;
	config.FindStringVector("loadmodule", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		CString sModName = vit->Token(0);
		CString sArgs = vit->Token(1, true);

		if (msModules.find(sModName) != msModules.end()) {
			sError = "Module [" + sModName +
				"] already loaded";
			CUtils::PrintError(sError);
			return false;
		}
		CString sModRet;
		CModule *pOldMod;

		pOldMod = GetModules().FindModule(sModName);
		if (!pOldMod) {
			CUtils::PrintAction("Loading Global Module [" + sModName + "]");

			bool bModRet = GetModules().LoadModule(sModName, sArgs, NULL, sModRet);

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}
		} else if (pOldMod->GetArgs() != sArgs) {
			CUtils::PrintAction("Reloading Global Module [" + sModName + "]");

			bool bModRet = GetModules().ReloadModule(sModName, sArgs, NULL, sModRet);

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}
		} else
			CUtils::PrintMessage("Module [" + sModName + "] already loaded.");

		msModules[sModName] = sArgs;
	}

	CString sISpoofFormat, sISpoofFile;
	config.FindStringEntry("ispoofformat", sISpoofFormat);
	config.FindStringEntry("ispooffile", sISpoofFile);
	if (!sISpoofFormat.empty() || !sISpoofFile.empty()) {
		CModule *pIdentFileMod = GetModules().FindModule("identfile");
		if (!pIdentFileMod) {
			CUtils::PrintAction("Loading Global Module [identfile]");

			CString sModRet;
			bool bModRet = GetModules().LoadModule("identfile", "", NULL, sModRet);

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}

			pIdentFileMod = GetModules().FindModule("identfile");
			msModules["identfile"] = "";
		}

		pIdentFileMod->SetNV("File", sISpoofFile);
		pIdentFileMod->SetNV("Format", sISpoofFormat);
	}

	config.FindStringVector("motd", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		AddMotd(*vit);
	}

	config.FindStringVector("bindhost", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		AddBindHost(*vit);
	}
	config.FindStringVector("vhost", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		AddBindHost(*vit);
	}

	CString sVal;
	if (config.FindStringEntry("pidfile", sVal))
		m_sPidFile = sVal;
	if (config.FindStringEntry("statusprefix", sVal))
		m_sStatusPrefix = sVal;
	if (config.FindStringEntry("sslcertfile", sVal))
		m_sSSLCertFile = sVal;
	if (config.FindStringEntry("skin", sVal))
		SetSkinName(sVal);
	if (config.FindStringEntry("connectdelay", sVal))
		m_uiConnectDelay = sVal.ToUInt();
	if (config.FindStringEntry("serverthrottle", sVal))
		m_sConnectThrottle.SetTTL(sVal.ToUInt() * 1000);
	if (config.FindStringEntry("anoniplimit", sVal))
		m_uiAnonIPLimit = sVal.ToUInt();
	if (config.FindStringEntry("maxbuffersize", sVal))
		m_uiMaxBufferSize = sVal.ToUInt();
	if (config.FindStringEntry("protectwebsessions", sVal))
  		m_bProtectWebSessions = sVal.ToBool();

	// This has to be after SSLCertFile is handled since it uses that value
	const char *szListenerEntries[] = {
		"listen", "listen6", "listen4",
		"listener", "listener6", "listener4"
	};
	const size_t numListenerEntries = sizeof(szListenerEntries) / sizeof(szListenerEntries[0]);

	for (size_t i = 0; i < numListenerEntries; i++) {
		config.FindStringVector(szListenerEntries[i], vsList);
		vit = vsList.begin();

		for (; vit != vsList.end(); ++vit) {
			if (!AddListener(szListenerEntries[i] + CString(" ") + *vit, sError))
				return false;
		}
	}

	CConfig::SubConfig subConf;
	CConfig::SubConfig::const_iterator subIt;
	config.FindSubConfig("user", subConf);
	for (subIt = subConf.begin(); subIt != subConf.end(); ++subIt) {
		const CString& sUserName = subIt->first;
		CConfig* pSubConf = subIt->second.m_pSubConfig;
		CUser* pRealUser = NULL;

		CUtils::PrintMessage("Loading user [" + sUserName + "]");

		// Either create a CUser* or use an existing one
		map<CString, CUser*>::iterator it = m_msDelUsers.find(sUserName);

		if (it != m_msDelUsers.end()) {
			pRealUser = it->second;
			m_msDelUsers.erase(it);
		}

		CUser* pUser = new CUser(sUserName);

		if (!m_sStatusPrefix.empty()) {
			if (!pUser->SetStatusPrefix(m_sStatusPrefix)) {
				sError = "Invalid StatusPrefix [" + m_sStatusPrefix + "] Must be 1-5 chars, no spaces.";
				CUtils::PrintError(sError);
				return false;
			}
		}

		if (!pUser->ParseConfig(pSubConf, sError)) {
			CUtils::PrintError(sError);
			delete pUser;
			pUser = NULL;
			return false;
		}

		if (!pSubConf->empty()) {
			sError = "Unhandled lines in config for User [" + sUserName + "]!";
			CUtils::PrintError(sError);

			DumpConfig(pSubConf);
			return false;
		}

		CString sErr;
		if (pRealUser) {
			if (!pRealUser->Clone(*pUser, sErr)
					|| !AddUser(pRealUser, sErr)) {
				sError = "Invalid user [" + pUser->GetUserName() + "] " + sErr;
				DEBUG("CUser::Clone() failed in rehash");
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
	}

	if (!config.empty()) {
		sError = "Unhandled lines in config!";
		CUtils::PrintError(sError);

		DumpConfig(&config);
		return false;
	}


	// Unload modules which are no longer in the config
	set<CString> ssUnload;
	for (size_t i = 0; i < GetModules().size(); i++) {
		CModule *pCurMod = GetModules()[i];

		if (msModules.find(pCurMod->GetModName()) == msModules.end())
			ssUnload.insert(pCurMod->GetModName());
	}

	for (set<CString>::iterator it = ssUnload.begin(); it != ssUnload.end(); ++it) {
		if (GetModules().UnloadModule(*it))
			CUtils::PrintMessage("Unloaded Global Module [" + *it + "]");
		else
			CUtils::PrintMessage("Could not unload [" + *it + "]");
	}

	if (m_msUsers.empty()) {
		sError = "You must define at least one user in your config.";
		CUtils::PrintError(sError);
		return false;
	}

	if (m_vpListeners.empty()) {
		sError = "You must supply at least one Listen port in your config.";
		CUtils::PrintError(sError);
		return false;
	}

	// Make sure that users that want to connect do so and also make sure a
	// new ConnectDelay setting is applied.
	DisableConnectUser();
	EnableConnectUser();

	return true;
}

void CZNC::DumpConfig(const CConfig* pConfig) {
	CConfig::EntryMapIterator eit = pConfig->BeginEntries();
	for (; eit != pConfig->EndEntries(); ++eit) {
		const CString& sKey = eit->first;
		const VCString& vsList = eit->second;
		VCString::const_iterator it = vsList.begin();
		for (; it != vsList.end(); ++it) {
			CUtils::PrintError(sKey + " = " + *it);
		}
	}

	CConfig::SubConfigMapIterator sit = pConfig->BeginSubConfigs();
	for (; sit != pConfig->EndSubConfigs(); ++sit) {
		const CString& sKey = sit->first;
		const CConfig::SubConfig& sSub = sit->second;
		CConfig::SubConfig::const_iterator it = sSub.begin();

		for (; it != sSub.end(); ++it) {
			CUtils::PrintError("SubConfig [" + sKey + " " + it->first + "]:");
			DumpConfig(it->second.m_pSubConfig);
		}
	}
}

void CZNC::ClearBindHosts() {
	m_vsBindHosts.clear();
}

bool CZNC::AddBindHost(const CString& sHost) {
	if (sHost.empty()) {
		return false;
	}

	for (unsigned int a = 0; a < m_vsBindHosts.size(); a++) {
		if (m_vsBindHosts[a].Equals(sHost)) {
			return false;
		}
	}

	m_vsBindHosts.push_back(sHost);
	return true;
}

bool CZNC::RemBindHost(const CString& sHost) {
	VCString::iterator it;
	for (it = m_vsBindHosts.begin(); it != m_vsBindHosts.end(); ++it) {
		if (sHost.Equals(*it)) {
			m_vsBindHosts.erase(it);
			return true;
		}
	}

	return false;
}

void CZNC::Broadcast(const CString& sMessage, bool bAdminOnly,
		CUser* pSkipUser, CClient *pSkipClient) {
	for (map<CString,CUser*>::iterator a = m_msUsers.begin(); a != m_msUsers.end(); ++a) {
		if (bAdminOnly && !a->second->IsAdmin())
			continue;

		if (a->second != pSkipUser) {
			CString sMsg = sMessage;

			MODULECALL(OnBroadcast(sMsg), a->second, NULL, continue);
			a->second->PutStatusNotice("*** " + sMsg, NULL, pSkipClient);
		}
	}
}

CModule* CZNC::FindModule(const CString& sModName, const CString& sUsername) {
	if (sUsername.empty()) {
		return CZNC::Get().GetModules().FindModule(sModName);
	}

	CUser* pUser = FindUser(sUsername);

	return (!pUser) ? NULL : pUser->GetModules().FindModule(sModName);
}

CModule* CZNC::FindModule(const CString& sModName, CUser* pUser) {
	if (pUser) {
		return pUser->GetModules().FindModule(sModName);
	}

	return CZNC::Get().GetModules().FindModule(sModName);
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
	if (FindUser(pUser->GetUserName()) != NULL) {
		sErrorRet = "User already exists";
		DEBUG("User [" << pUser->GetUserName() << "] - already exists");
		return false;
	}
	if (!pUser->IsValid(sErrorRet)) {
		DEBUG("Invalid user [" << pUser->GetUserName() << "] - ["
				<< sErrorRet << "]");
		return false;
	}
	GLOBALMODULECALL(OnAddUser(*pUser, sErrorRet), pUser, NULL,
		DEBUG("AddUser [" << pUser->GetUserName() << "] aborted by a module ["
			<< sErrorRet << "]");
		return false;
	);
	m_msUsers[pUser->GetUserName()] = pUser;
	return true;
}

CListener* CZNC::FindListener(u_short uPort, const CString& sBindHost, EAddrType eAddr) {
	vector<CListener*>::iterator it;

	for (it = m_vpListeners.begin(); it < m_vpListeners.end(); ++it) {
		if ((*it)->GetPort() != uPort)
			continue;
		if ((*it)->GetBindHost() != sBindHost)
			continue;
		if ((*it)->GetAddrType() != eAddr)
			continue;
		return *it;
	}
	return NULL;
}

bool CZNC::AddListener(const CString& sLine, CString& sError) {
	CString sName = sLine.Token(0);
	CString sValue = sLine.Token(1, true);

	EAddrType eAddr = ADDR_ALL;
	if (sName.Equals("Listen4") || sName.Equals("Listen") || sName.Equals("Listener4")) {
		eAddr = ADDR_IPV4ONLY;
	}
	if (sName.Equals("Listener6")) {
		eAddr = ADDR_IPV6ONLY;
	}

	CListener::EAcceptType eAccept = CListener::ACCEPT_ALL;
	if (sValue.TrimPrefix("irc_only "))
		eAccept = CListener::ACCEPT_IRC;
	else if (sValue.TrimPrefix("web_only "))
		eAccept = CListener::ACCEPT_HTTP;

	bool bSSL = false;
	CString sPort;
	CString sBindHost;

	if (ADDR_IPV4ONLY == eAddr) {
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

	switch (eAddr) {
		case ADDR_ALL:
			sIPV6Comment = "";
			break;
		case ADDR_IPV4ONLY:
			sIPV6Comment = " using ipv4";
			break;
		case ADDR_IPV6ONLY:
			sIPV6Comment = " using ipv6";
	}

	unsigned short uPort = sPort.ToUShort();
	CUtils::PrintAction("Binding to port [" + CString((bSSL) ? "+" : "") + CString(uPort) + "]" + sHostComment + sIPV6Comment);

#ifndef HAVE_IPV6
	if (ADDR_IPV6ONLY == eAddr) {
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

	CListener* pListener = new CListener(uPort, sBindHost, bSSL, eAddr, eAccept);

	if (!pListener->Listen()) {
		sError = FormatBindError();
		CUtils::PrintStatus(false, sError);
		delete pListener;
		return false;
	}

	m_vpListeners.push_back(pListener);
	CUtils::PrintStatus(true);

	return true;
}

bool CZNC::AddListener(CListener* pListener) {
	if (!pListener->GetRealListener()) {
		// Listener doesnt actually listen
		delete pListener;
		return false;
	}

	// We don't check if there is an identical listener already listening
	// since one can't listen on e.g. the same port multiple times

	m_vpListeners.push_back(pListener);
	return true;
}

bool CZNC::DelListener(CListener* pListener) {
	vector<CListener*>::iterator it;

	for (it = m_vpListeners.begin(); it < m_vpListeners.end(); ++it) {
		if (*it == pListener) {
			m_vpListeners.erase(it);
			delete pListener;
			return true;
		}
	}

	return false;
}

CZNC& CZNC::Get() {
	static CZNC* pZNC = new CZNC;
	return *pZNC;
}

CZNC::TrafficStatsMap CZNC::GetTrafficStats(TrafficStatsPair &Users,
			TrafficStatsPair &ZNC, TrafficStatsPair &Total) {
	TrafficStatsMap ret;
	unsigned long long uiUsers_in, uiUsers_out, uiZNC_in, uiZNC_out;
	const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

	uiUsers_in = uiUsers_out = 0;
	uiZNC_in  = BytesRead();
	uiZNC_out = BytesWritten();

	for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
		ret[it->first] = TrafficStatsPair(it->second->BytesRead(), it->second->BytesWritten());
		uiUsers_in  += it->second->BytesRead();
		uiUsers_out += it->second->BytesWritten();
	}

	for (CSockManager::const_iterator it = m_Manager.begin(); it != m_Manager.end(); ++it) {
		CUser *pUser = NULL;
		if ((*it)->GetSockName().Left(5) == "IRC::") {
			pUser = ((CIRCSock *) *it)->GetUser();
		} else if ((*it)->GetSockName().Left(5) == "USR::") {
			pUser = ((CClient*) *it)->GetUser();
		}

		if (pUser) {
			ret[pUser->GetUserName()].first  += (*it)->GetBytesRead();
			ret[pUser->GetUserName()].second += (*it)->GetBytesWritten();
			uiUsers_in  += (*it)->GetBytesRead();
			uiUsers_out += (*it)->GetBytesWritten();
		} else {
			uiZNC_in  += (*it)->GetBytesRead();
			uiZNC_out += (*it)->GetBytesWritten();
		}
	}

	Users = TrafficStatsPair(uiUsers_in, uiUsers_out);
	ZNC   = TrafficStatsPair(uiZNC_in, uiZNC_out);
	Total = TrafficStatsPair(uiUsers_in + uiZNC_in, uiUsers_out + uiZNC_out);

	return ret;
}

void CZNC::AuthUser(CSmartPtr<CAuthBase> AuthClass) {
	// TODO unless the auth module calls it, CUser::IsHostAllowed() is not honoured
	GLOBALMODULECALL(OnLoginAttempt(AuthClass), NULL, NULL, return);

	CUser* pUser = FindUser(AuthClass->GetUsername());

	if (!pUser || !pUser->CheckPass(AuthClass->GetPassword())) {
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
	virtual ~CConnectUserTimer() {
		// This is only needed when ZNC shuts down:
		// CZNC::~CZNC() sets its CConnectUserTimer pointer to NULL and
		// calls the manager's Cleanup() which destroys all sockets and
		// timers. If something calls CZNC::EnableConnectUser() here
		// (e.g. because a CIRCSock is destroyed), the socket manager
		// deletes that timer almost immediately, but CZNC now got a
		// dangling pointer to this timer which can crash later on.
		//
		// Unlikely but possible ;)
		CZNC::Get().LeakConnectUser(this);
	}

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

			DEBUG("Connecting user [" << pUser->GetUserName() << "]");

			if (CZNC::Get().ConnectUser(pUser))
				// User connecting, wait until next time timer fires
				return;
		}

		if (bUsersLeft == false) {
			DEBUG("ConnectUserTimer done");
			CZNC::Get().DisableConnectUser();
		}
	}

private:
	size_t m_uiPosNextUser;
};

void CZNC::SetConnectDelay(unsigned int i) {
	if (m_uiConnectDelay != i && m_pConnectUserTimer != NULL) {
		m_pConnectUserTimer->Start(i);
	}
	m_uiConnectDelay = i;
}

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

void CZNC::LeakConnectUser(CConnectUserTimer *pTimer) {
	if (m_pConnectUserTimer == pTimer)
		m_pConnectUserTimer = NULL;
}

bool CZNC::WaitForChildLock() {
	return m_pLockFile && m_pLockFile->ExLock();
}
