/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "IRCSock.h"
#include "Modules.h"
#include "Server.h"
#include "User.h"
#include "znc.h"

#include <tcl.h>

#define STDVAR (ClientData cd, Tcl_Interp *irp, int argc, const char *argv[])

#define BADARGS(nl, nh, example) do {                               \
	if ((argc < (nl)) || (argc > (nh))) {                       \
		Tcl_AppendResult(irp, "wrong # args: should be \"", \
			argv[0], (example), "\"", NULL);            \
		return TCL_ERROR;                                   \
	}                                                           \
} while (0)

class CModTcl;

class CModTclTimer : public CTimer {
public:

	CModTclTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription) : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CModTclTimer() {}
protected:
	virtual void RunJob();
	CModTcl* m_pParent;
};

class CModTclStartTimer : public CTimer {
public:

	CModTclStartTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription) : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CModTclStartTimer() {}
protected:
	virtual void RunJob();
	CModTcl* m_pParent;
};


class CModTcl : public CModule {
public:
	MODCONSTRUCTOR(CModTcl) {
		interp = NULL;
	}

	virtual ~CModTcl() {
		if (interp) {
			Tcl_DeleteInterp(interp);
		}
	}

	virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
#ifndef MOD_MODTCL_ALLOW_EVERYONE
		if (!m_pUser->IsAdmin()) {
			sErrorMsg = "You must be admin to use the modtcl module";
			return false;
		}
#endif

		AddTimer(new CModTclStartTimer(this, 1, 1, "ModTclStarter", "Timer for modtcl to load the interpreter."));
		return true;
	}

	void Start() {
		CString sMyArgs = GetArgs();

		interp = Tcl_CreateInterp();
		Tcl_Init(interp);
		Tcl_CreateCommand(interp, "Binds::ProcessPubm", tcl_Bind, this, NULL);
		Tcl_CreateCommand(interp, "Binds::ProcessTime", tcl_Bind, this, NULL);
		Tcl_CreateCommand(interp, "Binds::ProcessEvnt", tcl_Bind, this, NULL);
		Tcl_CreateCommand(interp, "Binds::ProcessNick", tcl_Bind, this, NULL);
		Tcl_CreateCommand(interp, "Binds::ProcessKick", tcl_Bind, this, NULL);
		Tcl_CreateCommand(interp, "PutIRC", tcl_PutIRC, this, NULL);
		Tcl_CreateCommand(interp, "PutIRCAs", tcl_PutIRCAs, this, NULL);
		Tcl_CreateCommand(interp, "PutModule", tcl_PutModule, this, NULL);
		Tcl_CreateCommand(interp, "PutStatus", tcl_PutStatus, this, NULL);
		Tcl_CreateCommand(interp, "PutStatusNotice", tcl_PutStatusNotice, this, NULL);
		Tcl_CreateCommand(interp, "PutUser", tcl_PutUser, this, NULL);

		Tcl_CreateCommand(interp, "GetLocalIP", tcl_GetLocalIP, this, NULL);
		Tcl_CreateCommand(interp, "GetCurNick", tcl_GetCurNick, this, NULL);
		Tcl_CreateCommand(interp, "GetUsername", tcl_GetUsername, this, NULL);
		Tcl_CreateCommand(interp, "GetRealName", tcl_GetRealName, this, NULL);
		Tcl_CreateCommand(interp, "GetVHost", tcl_GetBindHost, this, NULL);
		Tcl_CreateCommand(interp, "GetBindHost", tcl_GetBindHost, this, NULL);
		Tcl_CreateCommand(interp, "GetChans", tcl_GetChans, this, NULL);
		Tcl_CreateCommand(interp, "GetChannelUsers", tcl_GetChannelUsers, this, NULL);
		Tcl_CreateCommand(interp, "GetChannelModes", tcl_GetChannelModes, this, NULL);
		Tcl_CreateCommand(interp, "GetServer", tcl_GetServer, this, NULL);
		Tcl_CreateCommand(interp, "GetServerOnline", tcl_GetServerOnline, this, NULL);
		Tcl_CreateCommand(interp, "GetModules", tcl_GetModules, this, NULL);

		Tcl_CreateCommand(interp, "exit", tcl_exit, this, NULL);

		if (!sMyArgs.empty()) {
			i = Tcl_EvalFile(interp, sMyArgs.c_str());
			if (i != TCL_OK) {
				PutModule(Tcl_GetStringResult(interp));
			}
		}

		AddTimer(new CModTclTimer(this, 1, 0, "ModTclUpdate", "Timer for modtcl to process pending events and idle callbacks."));
	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sResult;
		VCString vsResult;
		CString sCmd = sCommand;

		if (sCmd.Token(0).CaseCmp(".tcl") == 0)
			sCmd = sCmd.Token(1,true);

		if (sCmd.Left(1).CaseCmp(".") == 0)
			sCmd = "Binds::ProcessDcc - - {" + sCmd + "}";

		Tcl_Eval(interp, sCmd.c_str());

		sResult = CString(Tcl_GetStringResult(interp));
		if (!sResult.empty()) {
			sResult.Split("\n", vsResult);
			unsigned int a = 0;
			for (a = 0; a < vsResult.size(); a++)
				PutModule(vsResult[a].TrimRight_n());
		}
	}

	virtual void TclUpdate() {
		while (Tcl_DoOneEvent(TCL_DONT_WAIT)) {}
		i = Tcl_Eval(interp,"Binds::ProcessTime");
		if (i != TCL_OK) {
			PutModule(Tcl_GetStringResult(interp));
		}
	}

	CString TclEscape(CString sLine) {
		sLine.Replace("\\","\\\\");
		sLine.Replace("{","\\{");
		sLine.Replace("}","\\}");
		return sLine;
	}

	virtual void OnPreRehash() {
		if (interp)
			Tcl_Eval(interp,"Binds::ProcessEvnt prerehash");
	}

	virtual void OnPostRehash() {
		if (interp) {
			Tcl_Eval(interp,"rehash");
			Tcl_Eval(interp,"Binds::ProcessEvnt rehash");
		}
	}

	virtual void OnIRCConnected() {
		if (interp)
			Tcl_Eval(interp, "Binds::ProcessEvnt init-server");
	}

	virtual void OnIRCDisconnected() {
		if (interp)
			Tcl_Eval(interp, "Binds::ProcessEvnt disconnect-server");
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		CString sMes = TclEscape(sMessage);
		CString sNick = TclEscape(CString(Nick.GetNick()));
		CString sHost = TclEscape(CString(Nick.GetIdent() + "@" + Nick.GetHost()));
		CString sChannel = TclEscape(CString(Channel.GetName()));

		CString sCommand = "Binds::ProcessPubm {" + sNick + "} {" + sHost + "} - {" + sChannel + "} {" + sMes + "}";
		i = Tcl_Eval(interp, sCommand.c_str());
		if (i != TCL_OK) {
			PutModule(Tcl_GetStringResult(interp));
		}
		return CONTINUE;
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		CString sOldNick = TclEscape(CString(OldNick.GetNick()));
		CString sNewNickTmp = TclEscape(sNewNick);
		CString sHost = TclEscape(CString(OldNick.GetIdent() + "@" + OldNick.GetHost()));

		CString sCommand;
		// Nick change is triggered for each common chan so that binds can be chan specific
		unsigned int nLength = vChans.size();
		for (unsigned int n = 0; n < nLength; n++) {
			sCommand = "Binds::ProcessNick {" + sOldNick + "} {" + sHost + "} - {" + vChans[n]->GetName() + "} {" + sNewNickTmp + "}";
			i = Tcl_Eval(interp, sCommand.c_str());
			if (i != TCL_OK) {
				PutModule(Tcl_GetStringResult(interp));
			}
		}
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
		CString sOpNick = TclEscape(CString(OpNick.GetNick()));
		CString sNick = TclEscape(sKickedNick);
		CString sOpHost = TclEscape(CString(OpNick.GetIdent() + "@" + OpNick.GetHost()));

		CString sCommand = "Binds::ProcessKick {" + sOpNick + "} {" + sOpHost + "} - {" + Channel.GetName() + "} {" + sNick + "} {" + sMessage + "}";
		i = Tcl_Eval(interp, sCommand.c_str());
		if (i != TCL_OK) {
			PutModule(Tcl_GetStringResult(interp));
		}
	}


private:
	Tcl_Interp *interp;
	int i;

	static CString argvit(const char *argv[], unsigned int end, unsigned int begin, CString delim) {
		CString sRet;
		unsigned int i;
		if (begin < end)
			sRet = CString(argv[begin]);

		for (i = begin + 1; i < end; i++) {
			sRet = sRet + delim + CString(argv[i]);
		}

		return sRet;
	}

	// Placeholder for binds incase binds.tcl isn't used
	static int tcl_Bind STDVAR {return TCL_OK;}

	static int tcl_GetLocalIP STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		Tcl_SetResult(irp, (char *)mod->m_pUser->GetLocalIP().c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetCurNick STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		Tcl_SetResult(irp, (char *)mod->m_pUser->GetCurNick().c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetUsername STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		Tcl_SetResult(irp, (char *)mod->m_pUser->GetUserName().c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetRealName STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		Tcl_SetResult(irp, (char *)mod->m_pUser->GetRealName().c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetBindHost STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		Tcl_SetResult(irp, (char *)mod->m_pUser->GetBindHost().c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetChans STDVAR {
		char *p;
		const char *l[1];
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(1, 1, "");

		const vector<CChan*>& Channels = mod->m_pUser->GetChans();
		for (unsigned int c = 0; c < Channels.size(); c++) {
			CChan* pChan = Channels[c];
			l[0] = pChan->GetName().c_str();
			p = Tcl_Merge(1, l);
			Tcl_AppendElement(irp, p);
			Tcl_Free((char *)p);
		}

		return TCL_OK;
	}

	static int tcl_GetChannelUsers STDVAR {
		char *p;
		const char *l[4];
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " channel");

		CString sChannel = argvit(argv, argc, 1, " ");
		CChan *pChannel = mod->m_pUser->FindChan(sChannel);

		if (!pChannel) {
			CString sMsg = "invalid channel: " + sChannel;
			Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
			return TCL_ERROR;
		}

		const map<CString,CNick>& msNicks = pChannel->GetNicks();
		for (map<CString,CNick>::const_iterator it = msNicks.begin(); it != msNicks.end(); ++it) {
			const CNick& Nick = it->second;
			l[0] = (Nick.GetNick()).c_str();
			l[1] = (Nick.GetIdent()).c_str();
			l[2] = (Nick.GetHost()).c_str();
			l[3] = (Nick.GetPermStr()).c_str();
			p = Tcl_Merge(4, l);
			Tcl_AppendElement(irp, p);
			Tcl_Free((char *)p);
		}

		return TCL_OK;
	}

	static int tcl_GetChannelModes STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " channel");

		CString sChannel = argvit(argv, argc, 1, " ");
		CChan *pChannel = mod->m_pUser->FindChan(sChannel);
		CString sMsg;

		if (!pChannel) {
			sMsg = "invalid channel: " + sChannel;
			Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
			return TCL_ERROR;
		}

		sMsg = pChannel->GetModeString();
		Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetServer STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		CServer* pServer = mod->m_pUser->GetCurrentServer();
		CString sMsg;
		if (pServer)
			sMsg = pServer->GetName() + ":" + CString(pServer->GetPort());
		Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetServerOnline STDVAR {
		CModTcl *mod = static_cast<CModTcl *>(cd);
		CIRCSock* pIRCSock = mod->m_pUser->GetIRCSock();
		CString sMsg = "0";
		if (pIRCSock)
			sMsg = CString(pIRCSock->GetStartTime());
		Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
		return TCL_OK;
	}

	static int tcl_GetModules STDVAR {
		char *p;
		const char *l[3];
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(1, 1, "");

		CModules& GModules = CZNC::Get().GetModules();
		CModules& Modules = mod->m_pUser->GetModules();

		for (unsigned int b = 0; b < GModules.size(); b++) {
			l[0] = GModules[b]->GetModName().c_str();
			l[1] = GModules[b]->GetArgs().c_str();
			l[2] = "1"; // IsGlobal
			p = Tcl_Merge(3, l);
			Tcl_AppendElement(irp, p);
			Tcl_Free((char *)p);
		}
		for (unsigned int b = 0; b < Modules.size(); b++) {
			l[0] = Modules[b]->GetModName().c_str();
			l[1] = Modules[b]->GetArgs().c_str();
			l[2] = "0"; // IsGlobal
			p = Tcl_Merge(3, l);
			Tcl_AppendElement(irp, p);
			Tcl_Free((char *)p);
		}

		return TCL_OK;
	}

	static int tcl_PutIRC STDVAR {
		CString sMsg;
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " string");
		sMsg = argvit(argv, argc, 1, " ");
		mod->m_pUser->PutIRC(sMsg);
		return TCL_OK;
	}

	static int tcl_PutIRCAs STDVAR {
		CString sMsg;

		BADARGS(3, 999, " user string");

		CUser *pUser = CZNC::Get().FindUser(argv[1]);
		if (!pUser) {
			sMsg = "invalid user " + CString(argv[1]);
			Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
			return TCL_ERROR;
		}

		sMsg = argvit(argv, argc, 2, " ");
		pUser->PutIRC(sMsg);
		return TCL_OK;
	}

	static int tcl_PutModule STDVAR {
		CString sMsg;
		VCString vsMsg;
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " string");
		sMsg = argvit(argv, argc, 1, " ");
		//mod->PutModule(sMsg);
		sMsg.Split("\n", vsMsg);
		unsigned int a = 0;
		for (a = 0; a < vsMsg.size(); a++)
			mod->PutModule(vsMsg[a].TrimRight_n());
		return TCL_OK;
	}

	static int tcl_PutStatus STDVAR {
		CString sMsg;
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " string");
		sMsg = argvit(argv, argc, 1, " ");
		mod->PutStatus(sMsg);
		return TCL_OK;
	}

	static int tcl_PutStatusNotice STDVAR {
		CString sMsg;
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " string");
		sMsg = argvit(argv, argc, 1, " ");
		mod->m_pUser->PutStatusNotice(sMsg);
		return TCL_OK;
	}

	static int tcl_PutUser STDVAR {
		CString sMsg;
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(2, 999, " string");
		sMsg = argvit(argv, argc, 1, " ");
		mod->m_pUser->PutUser(sMsg);
		return TCL_OK;
	}

	static int tcl_exit STDVAR {
		CString sMsg;
		CModTcl *mod = static_cast<CModTcl *>(cd);

		BADARGS(1, 2, " ?reason?");

		if (! mod->m_pUser->IsAdmin()) {
			sMsg = "You need to be administrator to shutdown the bnc.";
			Tcl_SetResult(irp, (char*)sMsg.c_str(), TCL_VOLATILE);
			return TCL_ERROR;
		}
		if (argc > 1) {
			sMsg = argvit(argv, argc, 1, " ");
			CZNC::Get().Broadcast(sMsg);
			usleep(100000); // Sleep for 10ms to attempt to allow the previous Broadcast() to go through to all users
		}

		throw CException(CException::EX_Shutdown);

		return TCL_OK;
	}
};

void CModTclTimer::RunJob() {
	CModTcl *p = (CModTcl *)m_pModule;
	if (p)
		p->TclUpdate();
}

void CModTclStartTimer::RunJob() {
	CModTcl *p = (CModTcl *)m_pModule;
	if (p)
		p->Start();
}

template<> void TModInfo<CModTcl>(CModInfo& Info) {
	Info.SetWikiPage("modtcl");
}

MODULEDEFS(CModTcl, "Loads Tcl scripts as ZNC modules")
