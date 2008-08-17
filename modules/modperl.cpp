/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * @author imaginos@imaginos.net
 *
 * NOTE, the proper way to valgrind this module is to compile znc with debug, and do the following ...
 *
 * PERL_DESTRUCT_LEVEL=2 GLIBCPP_FORCE_NEW=1 valgrind --leak-check=full /path/to/znc
 *
 * GLIBCPP_FORCE_NEW is an STL env flag to turn of mem pooling, and stops false alarms within stl
 * PERL_DESTRUCT_LEVEL is a perl env flag to force the VM to cleanup, this also stops false alarms, but within perl
 */

#ifdef HAVE_PERL
#include "Chan.h"
#include "User.h"
#include "znc.h"

// perl stuff
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#define NICK(a) a.GetNickMask()
#define CHAN(a) a.GetName()
#define ZNCEvalCB "ZNC::COREEval"
#define ZNCCallFuncCB "ZNC::CORECallFunc"
#define ZNCCallTimerCB "ZNC::CORECallTimer"
#define ZNCCallSockCB "ZNC::CORECallSock"
#define ZNCSOCK ":::ZncSock:::"

class PString : public CString
{
public:
	enum EType
	{
		STRING,
		INT,
		UINT,
		NUM,
		BOOL
	};

	PString() : CString() { m_eType = STRING; }
	PString(const char* c) : CString(c) { m_eType = STRING; }
	PString(const CString& s) : CString(s) { m_eType = STRING; }
	PString(int i) : CString(i) { m_eType = INT; }
	PString(u_int i) : CString(i) { m_eType = UINT; }
	PString(long i) : CString(i) { m_eType = INT; }
	PString(u_long i) : CString(i) { m_eType = UINT; }
	PString(long long i) : CString(i) { m_eType = INT; }
	PString(unsigned long long i) : CString(i) { m_eType = UINT; }
	PString(double i) : CString(i) { m_eType = NUM; }
	PString(bool b) : CString((b ? "1" : "0")) { m_eType = BOOL; }

	virtual ~PString() {}

	EType GetType() const { return(m_eType); }
	void SetType(EType e) { m_eType = e; }

	SV * GetSV(bool bMakeMortal = true) const
	{
		SV *pSV = NULL;
		switch(GetType())
		{
			case NUM:
				pSV = newSVnv(ToDouble());
				break;
			case INT:
				pSV = newSViv(ToLongLong());
				break;
			case UINT:
			case BOOL:
				pSV = newSVuv(ToULongLong());
				break;
			case STRING:
			default:
				pSV = newSVpv(data(), length());
				break;
		}

		if (bMakeMortal)
			pSV = sv_2mortal(pSV);

		return(pSV);
	}

private:
	EType	m_eType;
};


class CPerlHash : public map< CString, PString >
{
public:

	HV *GetHash()
	{
		HV *pHash = newHV();
		sv_2mortal((SV *) pHash);
		for (CPerlHash::iterator it = this->begin(); it != this->end(); it++)
		{
			SV *pSV = it->second.GetSV(false);
			hv_store(pHash, it->first.c_str(), it->first.length(), pSV, 0);
		}

		return(pHash);
	}
};

typedef vector< PString > VPString;

class CModPerl;
static CModPerl *g_ModPerl = NULL;

class CPerlSock : public Csock
{
public:
	CPerlSock() : Csock()
	{
		m_iParentFD = -1;
		SetSockName(ZNCSOCK);
	}
	CPerlSock(const CS_STRING & sHost, u_short iPort, int iTimeout = 60)
		: Csock(sHost, iPort, iTimeout)
	{
		m_iParentFD = -1;
		SetSockName(ZNCSOCK);
	}


// # OnSockDestroy($sockhandle)
	virtual ~CPerlSock();

	virtual Csock *GetSockObj(const CS_STRING & sHostname, u_short iPort);

	void SetParentFD(int iFD) { m_iParentFD = iFD; }
	void SetUsername(const CString & sUsername) { m_sUsername = sUsername; }
	void SetModuleName(const CString & sModuleName) { m_sModuleName = sModuleName; }

	const CString &  GetUsername() { return(m_sUsername); }
	const CString &  GetModuleName() { return(m_sModuleName); }

// # OnConnect($sockhandle, $parentsockhandle)
	virtual void Connected();
// # OnConnectionFrom($sockhandle, $remotehost, $remoteport)
	virtual bool ConnectionFrom(const CS_STRING & sHost, u_short iPort);
// # OnError($sockhandle, $errno)
	virtual void SockError(int iErrno);
// # OnConnectionRefused($sockhandle)
	virtual void ConnectionRefused();
// # OnTimeout($sockhandle)
	virtual void Timeout();
// # OnDisconnect($sockhandle)
	virtual void Disconnected();
// # OnData($sockhandle, $bytes, $length)
	virtual void ReadData(const char *data, int len);
// # OnReadLine($sockhandle, $line)
	virtual void ReadLine(const CS_STRING & sLine);


private:
	CString		m_sModuleName;
	CString		m_sUsername;	// NEED these so we can send the signal to the right guy
	int			m_iParentFD;
	VPString	m_vArgs;

	void SetupArgs()
	{
		m_vArgs.clear();
		m_vArgs.push_back(m_sModuleName);
		m_vArgs.push_back(GetRSock());
	}

	void AddArg(const PString & sArg)
	{
		m_vArgs.push_back(sArg);
	}

	int CallBack(const PString & sFuncName);
};

class CPerlTimer : public CTimer
{
public:
	CPerlTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CPerlTimer() {}

	void SetFuncName(const CString & sFuncName) { m_sFuncName = sFuncName; }
	void SetUserName(const CString & sUserName) { m_sUserName = sUserName; }
	void SetModuleName(const CString & sModuleName) { m_sModuleName = sModuleName; }

protected:
	virtual void RunJob();

	CString		m_sFuncName;
	CString		m_sUserName;
	CString		m_sModuleName;
};

class CModPerl : public CGlobalModule
{
public:
	GLOBALMODCONSTRUCTOR(CModPerl)
	{
		g_ModPerl = this;
		m_pPerl = NULL;
	}

	virtual ~CModPerl()
	{
		DestroyAllSocks();
		if (m_pPerl)
		{
			const map<CString,CUser*> & msUsers = CZNC::Get().GetUserMap();

			for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++)
			{ // need to set it on all of these
				m_pUser = it->second;
				CBNone("OnShutdown");
				m_pUser = NULL;
			}
			PerlInterpShutdown();
		}
		g_ModPerl = NULL;
	}

	void PerlInterpShutdown()
	{
		perl_destruct(m_pPerl);
		perl_free(m_pPerl);
		PERL_SYS_TERM();
		m_pPerl = NULL;
	}

	bool SetupZNCScript()
	{
		CString sModule, sTmp;

		if (!CZNC::Get().FindModPath("modperl.pm", sModule, sTmp))
			return false;

		CString sBuffer, sScript;
		CFile cFile(sModule);
		if (!cFile.Exists() || !cFile.Open(O_RDONLY))
			return false;

		while (cFile.ReadLine(sBuffer))
			sScript += sBuffer;
		cFile.Close();

		eval_pv(sScript.c_str(), FALSE);
		return true;
	}

	virtual EModRet OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan)
	{
		if ((sName.CaseCmp("loadperlmodule") == 0) && (pUser))
		{
			m_pUser = pUser;
			if (sValue.Right(3) == ".pm")
				LoadPerlMod(sValue);
			else
				LoadPerlMod(sValue + ".pm");
			m_pUser = NULL;
			return(HALT);
		}
		return(CONTINUE);
	}

	void DumpError(const CString & sError)
	{
		CString sTmp = sError;
		for (CString::size_type a = 0; a < sTmp.size(); a++)
		{
			if (isspace(sTmp[a]))
				sTmp[a] = ' ';
		}
		PutModule(sTmp);
		DEBUG_ONLY(cerr << sTmp << endl);
	}

	CSockManager * GetSockManager() { return(m_pManager); }
	void DestroyAllSocks(const CString & sModuleName = "");

	CUser * GetUser(const CString & sUsername = "", bool bSetUserContext = false)
	{
		if (sUsername.empty())
			return(m_pUser);

		CUser *pUser = CZNC::Get().GetUser(sUsername);
		if (bSetUserContext)
			m_pUser = pUser;

		return(pUser);
	}
	void UnSetUser() { m_pUser = NULL; }

	virtual bool OnLoad(const CString & sArgs, CString & sMessage);
	virtual void OnUserAttached() {  CBNone("OnUserAttached"); }
	virtual void OnUserDetached() {  CBNone("OnUserDetached"); }
	virtual void OnIRCDisconnected() {  CBNone("OnIRCDisconnected"); }
	virtual void OnIRCConnected() {  CBNone("OnIRCConnected"); }

	virtual EModRet OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort,
		const CString& sFile, unsigned long uFileSize);

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange)
	{
		CBFour("OnOp", NICK(OpNick), NICK(Nick), CHAN(Channel), bNoChange);
	}
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange)
	{
		CBFour("OnDeop", NICK(OpNick), NICK(Nick), CHAN(Channel), bNoChange);
	}
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange)
	{
		CBFour("OnVoice", NICK(OpNick), NICK(Nick), CHAN(Channel), bNoChange);
	}
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange)
	{
		CBFour("OnDevoice", NICK(OpNick), NICK(Nick), CHAN(Channel), bNoChange);
	}
	virtual void OnRawMode(const CNick& Nick, CChan& Channel, const CString& sModes, const CString& sArgs)
	{
		CBFour("OnRawMode", NICK(Nick), CHAN(Channel), sModes, sArgs);
	}
	virtual EModRet OnUserRaw(CString& sLine) { return(CBSingle("OnUserRaw", sLine)); }
	virtual EModRet OnRaw(CString& sLine) { return(CBSingle("OnRaw", sLine)); }

	virtual void OnModCommand(const CString& sCommand)
	{
		if (CBSingle("OnModCommand", sCommand) == 0)
			Eval(sCommand);
	}
	virtual void OnModNotice(const CString& sMessage) { CBSingle("OnModNotice", sMessage); }
	virtual void OnModCTCP(const CString& sMessage) { CBSingle("OnModCTCP", sMessage); }

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans)
	{
		VPString vsArgs;
		vsArgs.push_back(Nick.GetNickMask());
		vsArgs.push_back(sMessage);
		for (vector<CChan*>::size_type a = 0; a < vChans.size(); a++)
			vsArgs.push_back(vChans[a]->GetName());

		CallBack("OnQuit", vsArgs);
	}

	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		VPString vsArgs;
		vsArgs.push_back(Nick.GetNickMask());
		vsArgs.push_back(sNewNick);
		for (vector<CChan*>::size_type a = 0; a < vChans.size(); a++)
			vsArgs.push_back(vChans[a]->GetName());

		CallBack("OnNick", vsArgs);
	}

	virtual void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage)
	{
		CBFour("OnKick", NICK(Nick), sOpNick, CHAN(Channel), sMessage);
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) { CBDouble("OnJoin", NICK(Nick), CHAN(Channel)); }
	virtual void OnPart(const CNick& Nick, CChan& Channel) { CBDouble("OnPart", NICK(Nick), CHAN(Channel)); }

	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage)
	{
		return CBDouble("OnUserCTCPReply", sTarget, sMessage);
	}
	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage)
	{
		return CBDouble("OnCTCPReply", NICK(Nick), sMessage);
	}
	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage)
	{
		return CBDouble("OnUserCTCP", sTarget, sMessage);
	}
	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage)
	{
		return CBDouble("OnPrivCTCP", NICK(Nick), sMessage);
	}
	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage)
	{
		return CBTriple("OnChanCTCP", NICK(Nick), CHAN(Channel), sMessage);
	}
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage)
	{
		return CBDouble("OnUserMsg", sTarget, sMessage);
	}
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage)
	{
		return CBDouble("OnPrivMsg", NICK(Nick), sMessage);
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan & Channel, CString & sMessage)
	{
		return(CBTriple("OnChanMsg", NICK(Nick), CHAN(Channel), sMessage));
	}
	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage)
	{
		return CBDouble("OnUserNotice", sTarget, sMessage);
	}
	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage)
	{
		return CBDouble("OnPrivNotice", NICK(Nick), sMessage);
	}
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage)
	{
		return(CBTriple("OnChanNotice", NICK(Nick), CHAN(Channel), sMessage));
	}

	enum ECBTYPES
	{
		CB_LOCAL 	= 1,
		CB_ONHOOK 	= 2,
		CB_TIMER 	= 3,
		CB_SOCK		= 4
	};

	EModRet CallBack(const PString & sHookName, const VPString & vsArgs,
			ECBTYPES eCBType = CB_ONHOOK, const PString & sUsername = "");

	EModRet CBNone(const PString & sHookName)
	{
		VPString vsArgs;
		return(CallBack(sHookName, vsArgs));
	}

	template <class A>
	inline EModRet CBSingle(const PString & sHookName, const A & a)
	{
		VPString vsArgs;
		vsArgs.push_back(a);
		return(CallBack(sHookName, vsArgs));
	}
	template <class A, class B>
	inline EModRet CBDouble(const PString & sHookName, const A & a, const B & b)
	{
		VPString vsArgs;
		vsArgs.push_back(a);
		vsArgs.push_back(b);
		return(CallBack(sHookName, vsArgs));
	}
	template <class A, class B, class C>
	inline EModRet CBTriple(const PString & sHookName, const A & a, const B & b, const C & c)
	{
		VPString vsArgs;
		vsArgs.push_back(a);
		vsArgs.push_back(b);
		vsArgs.push_back(c);
		return(CallBack(sHookName, vsArgs));
	}
	template <class A, class B, class C, class D>
	inline EModRet CBFour(const PString & sHookName, const A & a, const B & b, const C & c, const D & d)
	{
		VPString vsArgs;
		vsArgs.push_back(a);
		vsArgs.push_back(b);
		vsArgs.push_back(c);
		vsArgs.push_back(d);
		return(CallBack(sHookName, vsArgs));
	}

	bool Eval(const CString & sScript, const CString & sFuncName = ZNCEvalCB);

	virtual EModRet OnStatusCommand(const CString& sLine)
	{
		CString sCommand = sLine.Token(0);

		if (sCommand.CaseCmp("loadperlmod", 11) == 0 || sCommand.CaseCmp("unloadperlmod", 13) == 0 || sCommand.CaseCmp("reloadperlmod", 13) == 0)
		{
			CString sModule = sLine.Token(1);
			if (sModule.Right(3) != ".pm")
				sModule += ".pm";
			if (sCommand.CaseCmp("loadperlmod", 11) == 0)
				LoadPerlMod(sModule);
			else if (sCommand.CaseCmp("unloadperlmod", 13) == 0)
				UnloadPerlMod(sModule);
			else
			{
				UnloadPerlMod(sModule);
				LoadPerlMod(sModule);
			}
			return(HALT);
		}
		return(CONTINUE);
	}

	void LoadPerlMod(const CString & sModule);
	void UnloadPerlMod(const CString & sModule);

private:
	PerlInterpreter	*m_pPerl;

};

GLOBALMODULEDEFS(CModPerl, "Loads perl scripts as ZNC modules")



//////////////////////////////// PERL GUTS //////////////////////////////

XS(XS_ZNC_COREAddTimer)
{
	dXSARGS;
	if (items != 5)
		Perl_croak(aTHX_ "Usage: COREAddTimer(modname, funcname, description, interval, cycles)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			CString sModName = (char *)SvPV(ST(0),PL_na);
			CString sFuncName = (char *)SvPV(ST(1),PL_na);
			CString sDesc = (char *)SvPV(ST(2),PL_na);
			u_int iInterval = (u_int)SvUV(ST(3));
			u_int iCycles = (u_int)SvUV(ST(4));
			CString sUserName = g_ModPerl->GetUser()->GetUserName();
			CString sLabel = sUserName + sModName + sFuncName;
			CPerlTimer *pTimer = new CPerlTimer(g_ModPerl, iInterval, iCycles, sLabel, sDesc);
			pTimer->SetFuncName(sFuncName);
			pTimer->SetUserName(sUserName);
			pTimer->SetModuleName(sModName);
			g_ModPerl->AddTimer(pTimer);
		}
		PUTBACK;
	}
}

XS(XS_ZNC_CORERemTimer)
{
	dXSARGS;
	if (items != 2)
		Perl_croak(aTHX_ "Usage: CORERemTimer(modname, funcname)");
	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			CString sModName = (char *)SvPV(ST(0),PL_na);
			CString sFuncName = (char *)SvPV(ST(1),PL_na);
			CString sUserName = g_ModPerl->GetUser()->GetUserName();
			CString sLabel = sUserName + sModName + sFuncName;
			CTimer *pTimer = g_ModPerl->FindTimer(sLabel);
			if (pTimer)
				pTimer->Stop();
			else
				g_ModPerl->PutModule("Unable to find Timer!");
		}
		PUTBACK;
	}
}
XS(XS_ZNC_COREPuts)
{
	dXSARGS;
	if (items != 2)
		Perl_croak(aTHX_ "Usage: COREPuts(sWHich, sLine)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			CString sWhich = (char *)SvPV(ST(0),PL_na);
			CString sLine = (char *)SvPV(ST(1),PL_na);

			if (sWhich == "IRC")
				g_ModPerl->PutIRC(sLine);
			else if (sWhich == "Status")
				g_ModPerl->PutStatus(sLine);
			else if (sWhich == "User")
				g_ModPerl->PutUser(sLine);
		}
		PUTBACK;
	}
}

XS(XS_ZNC_LoadMod)
{
	dXSARGS;
	if (items != 1)
		Perl_croak(aTHX_ "Usage: LoadMod(module)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if (g_ModPerl)
		{
			CString sModule = (char *)SvPV(ST(0),PL_na);
			g_ModPerl->LoadPerlMod(sModule);
		}
		PUTBACK;
	}
}

XS(XS_ZNC_UnloadMod)
{
	dXSARGS;
	if (items != 1)
		Perl_croak(aTHX_ "Usage: UnloadMod(module)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if (g_ModPerl)
		{
			CString sModule = (char *)SvPV(ST(0),PL_na);
			g_ModPerl->UnloadPerlMod(sModule);
		}
		PUTBACK;
	}
}

XS(XS_ZNC_COREPutModule)
{
	dXSARGS;
	if (items != 4)
		Perl_croak(aTHX_ "Usage: COREPutModule(sWhich sLine, sIdent, sHost)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if (g_ModPerl)
		{
			CString sWhich = (char *)SvPV(ST(0),PL_na);
			CString sLine = (char *)SvPV(ST(1),PL_na);
			CString sIdent = (char *)SvPV(ST(2),PL_na);
			CString sHost = (char *)SvPV(ST(3),PL_na);
			if (sWhich == "Module")
				g_ModPerl->PutModule(sLine, sIdent, sHost);
			else
				g_ModPerl->PutModNotice(sLine, sIdent, sHost);
		}
		PUTBACK;
	}
}

XS(XS_ZNC_GetNicks)
{
	dXSARGS;
	if (items != 1)
		Perl_croak(aTHX_ "Usage: GetNicks(sChan)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			CString sChan = (char *)SvPV(ST(0),PL_na);
			CUser * pUser = g_ModPerl->GetUser();
			CChan * pChan = pUser->FindChan(sChan);
			if (!pChan)
				XSRETURN(0);

			const map< CString,CNick* > & mscNicks = pChan->GetNicks();

			for (map< CString,CNick* >::const_iterator it = mscNicks.begin(); it != mscNicks.end(); it++)
			{
				CNick & cNick = *(it->second);
				CPerlHash cHash;
				cHash["Nick"] = cNick.GetNick();
				cHash["Ident"] = cNick.GetIdent();
				cHash["Host"] = cNick.GetHost();
				cHash["Perms"] = cNick.GetPermStr();
				XPUSHs(newRV_noinc((SV*)cHash.GetHash()));
			}
		}
		PUTBACK;
	}
}

XS(XS_ZNC_GetString)
{
	dXSARGS;

	if (items != 1)
		Perl_croak(aTHX_ "Usage: GetString(sName)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			CUser * pUser = g_ModPerl->GetUser();
			PString sReturn;
			CString sName = (char *)SvPV(ST(0),PL_na);

			if (sName == "UserName") sReturn = pUser->GetUserName();
			else if (sName == "Nick") sReturn = pUser->GetNick();
			else if (sName == "AltNick") sReturn = pUser->GetAltNick();
			else if (sName == "Ident") sReturn = pUser->GetIdent();
			else if (sName == "RealName") sReturn = pUser->GetRealName();
			else if (sName == "VHost") sReturn = pUser->GetVHost();
			else if (sName == "Pass") sReturn = pUser->GetPass();
			else if (sName == "CurPath") sReturn = CZNC::Get().GetCurPath();
			else if (sName == "DLPath") sReturn = pUser->GetDLPath();
			else if (sName == "ModPath") sReturn = CZNC::Get().GetModPath();
			else if (sName == "HomePath") sReturn = CZNC::Get().GetHomePath();
			else if (sName == "SavePath") sReturn = g_ModPerl->GetSavePath();
			else if (sName == "StatusPrefix") sReturn = pUser->GetStatusPrefix();
			else if (sName == "DefaultChanModes") sReturn = pUser->GetDefaultChanModes();
			else if (sName == "IRCServer") sReturn = pUser->GetIRCServer();
			else
				XSRETURN(0);

			XPUSHs(sReturn.GetSV());
		}
		PUTBACK;
	}
}

////////////// Perl SOCKET guts /////////////
#define MANAGER g_ModPerl->GetSockManager()
XS(XS_ZNC_WriteSock)
{
	dXSARGS;
	if (items != 3)
		Perl_croak(aTHX_ "Usage: ZNC::WriteSock(sockhandle, bytes, len)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if (g_ModPerl)
		{
			PString sReturn = false;
			int iSockFD = SvIV(ST(0));
			STRLEN iLen = SvUV(ST(2));
			if (iLen > 0)
			{
				PString sData;
				STRLEN iLen2 = iLen;
				sData.append(SvPV(ST(1), iLen), iLen2);
				CPerlSock *pSock = (CPerlSock *)MANAGER->FindSockByFD(iSockFD);
				if ((pSock) && (pSock->GetSockName() == ZNCSOCK))
					sReturn = pSock->Write(sData.data(), sData.length());
			}
			XPUSHs(sReturn.GetSV());
		}
		PUTBACK;
	}
}

XS(XS_ZNC_CloseSock)
{
	dXSARGS;
	if (items != 1)
		Perl_croak(aTHX_ "Usage: ZNC::CloseSock(sockhandle)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if (g_ModPerl)
		{
			int iSockFD = SvIV(ST(0));
			CPerlSock *pSock = (CPerlSock *)MANAGER->FindSockByFD(iSockFD);
			if ((pSock) && (pSock->GetSockName() == ZNCSOCK))
				pSock->Close();
		}
		PUTBACK;
	}
}

XS(XS_ZNC_SetSockValue)
{
	dXSARGS;
	if (items < 3)
		Perl_croak(aTHX_ "Usage: ZNC::SetSockValue(sockhandle, what, value)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if (g_ModPerl)
		{
			int iSockFD = SvIV(ST(0));
			PString sWhat = (char *)SvPV(ST(1),PL_na);
			CPerlSock *pSock = (CPerlSock *)MANAGER->FindSockByFD(iSockFD);
			if ((pSock) && (pSock->GetSockName() == ZNCSOCK))
			{
				if (sWhat == "timeout")
				{
					u_int iTimeout = SvUV(ST(2));
					pSock->SetTimeout(iTimeout);
				}
			}
		}
		PUTBACK;
	}
}

XS(XS_ZNC_COREConnect)
{
	dXSARGS;
	if (items != 6)
		Perl_croak(aTHX_ "Usage: ZNC::COREConnect($modname, $host, $port, $timeout, $bEnableReadline, $bUseSSL)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			PString sReturn = -1;
			PString sModuleName = (char *)SvPV(ST(0),PL_na);
			PString sHostname = (char *)SvPV(ST(1),PL_na);
			int iPort = SvIV(ST(2));
			u_int iTimeout = SvUV(ST(3));
			u_int iEnableReadline = SvUV(ST(4));
			u_int iUseSSL = SvUV(ST(5));
			CPerlSock *pSock = new CPerlSock(sHostname, iPort, iTimeout);
			pSock->SetSockName(ZNCSOCK);
			pSock->SetUsername(g_ModPerl->GetUser()->GetUserName());
			pSock->SetModuleName(sModuleName);
			if (iEnableReadline)
				pSock->EnableReadLine();

			if (MANAGER->Connect(sHostname, iPort, ZNCSOCK, iTimeout, (iUseSSL != 0), "", pSock))
				sReturn = pSock->GetRSock();

			XPUSHs(sReturn.GetSV());
		}
		PUTBACK;
	}
}

XS(XS_ZNC_COREListen)
{
	dXSARGS;
	if (items != 5)
		Perl_croak(aTHX_ "Usage: ZNC::COREListen($modname, $port, $bindhost, $bEnableReadline, $bUseSSL)");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ((g_ModPerl) && (g_ModPerl->GetUser()))
		{
			PString sReturn = -1;
			PString sModuleName = (char *)SvPV(ST(0),PL_na);
			int iPort = SvIV(ST(1));
			PString sHostname = (char *)SvPV(ST(2),PL_na);
			u_int iEnableReadline = SvUV(ST(3));
			u_int iUseSSL = SvUV(ST(4));

			CPerlSock *pSock = new CPerlSock();
			pSock->SetSockName(ZNCSOCK);
			pSock->SetUsername(g_ModPerl->GetUser()->GetUserName());
			pSock->SetModuleName(sModuleName);

			if (iEnableReadline)
				pSock->EnableReadLine();

			bool bContinue = true;

#ifdef HAVE_LIBSSL
			if (iUseSSL != 0)
			{
				if (CFile::Exists(CZNC::Get().GetPemLocation()))
					pSock->SetPemLocation(CZNC::Get().GetPemLocation());
				else
				{
					g_ModPerl->PutModule("PEM File does not exist! (looking for " + CZNC::Get().GetPemLocation() + ")");
					bContinue = false;
				}
			}
#endif /* HAVE_LIBSSL */

			if (bContinue)
			{
				if (MANAGER->ListenHost(iPort, ZNCSOCK, sHostname, (iUseSSL != 0), SOMAXCONN, pSock))
					sReturn = pSock->GetRSock();
			}
			else
				sReturn = -1;

			XPUSHs(sReturn.GetSV());

		}
		PUTBACK;
	}
}

/////////// supporting functions from within module

bool CModPerl::Eval(const CString & sScript, const CString & sFuncName)
{
	dSP;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVpv(sScript.c_str(), sScript.length())));
	PUTBACK;

	SPAGAIN;

	call_pv(sFuncName.c_str(), G_EVAL|G_KEEPERR|G_VOID|G_DISCARD);

	bool bReturn = true;

	if (SvTRUE(ERRSV))
	{
		DumpError(SvPV(ERRSV, PL_na));
		bReturn = false;
	}
	PUTBACK;
	FREETMPS;

	return(bReturn);
}

CModPerl::EModRet CModPerl::CallBack(const PString & sHookName, const VPString & vsArgs,
		ECBTYPES eCBType, const PString & sUsername)
{
	if (!m_pPerl)
		return(CONTINUE);

	dSP;
	SAVETMPS;

	PUSHMARK(SP);

	CString sFuncToCall;
	if (eCBType == CB_LOCAL)
		sFuncToCall = sHookName;
	else
	{
		if (sUsername.empty())
		{
			if (!m_pUser)
				return(CONTINUE);

			XPUSHs(PString(m_pUser->GetUserName()).GetSV());
		}
		else
			XPUSHs(sUsername.GetSV());
		XPUSHs(sHookName.GetSV());
		if (eCBType == CB_ONHOOK)
			sFuncToCall = ZNCCallFuncCB;
		else if (eCBType == CB_TIMER)
			sFuncToCall = ZNCCallTimerCB;
		else
			sFuncToCall = ZNCCallSockCB;
	}
	for (VPString::size_type a = 0; a < vsArgs.size(); a++)
		XPUSHs(vsArgs[a].GetSV());

	PUTBACK;

	int iCount = call_pv(sFuncToCall.c_str(), G_EVAL|G_SCALAR);

	SPAGAIN;
	int iRet = CONTINUE;

	if (SvTRUE(ERRSV))
	{
		CString sError = SvPV(ERRSV, PL_na);
		DumpError(sHookName + ": " + sError);

		if (eCBType == CB_TIMER)
			iRet = HALT;
	} else
	{
		if (iCount == 1)
			iRet = POPi;
	}

	PUTBACK;
	FREETMPS;

	return((CModPerl::EModRet)iRet);
}

////////////////////// Events ///////////////////

// special case this, required for perl modules that are dynamic
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

bool CModPerl::OnLoad(const CString & sArgs, CString & sMessage)
{
	int iArgc = 5;
	const char * pArgv[] = { "", "-e", "0", "-T", "-w", NULL };
	PERL_SYS_INIT3( &iArgc, (char ***)&pArgv, &environ );
	m_pPerl = perl_alloc();
	perl_construct(m_pPerl);

	if (perl_parse(m_pPerl, NULL, iArgc, (char **)pArgv, (char **)NULL) != 0)
	{
		perl_free(m_pPerl);
		PERL_SYS_TERM();
		m_pPerl = NULL;
		return(false);
	}

#ifdef PERL_EXIT_DESTRUCT_END
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#endif /* PERL_EXIT_DESTRUCT_END */

	/* system functions */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, "modperl");
	newXS("ZNC::COREPutModule", XS_ZNC_COREPutModule, "modperl");
	newXS("ZNC::COREAddTimer", XS_ZNC_COREAddTimer, "modperl");
	newXS("ZNC::CORERemTimer", XS_ZNC_CORERemTimer, "modperl");
	newXS("ZNC::COREPuts", XS_ZNC_COREPuts, "modperl");
	newXS("ZNC::COREConnect", XS_ZNC_COREConnect, "modperl");
	newXS("ZNC::COREListen", XS_ZNC_COREListen, "modperl");

	/* user functions */
	newXS("ZNC::GetNicks", XS_ZNC_GetNicks, "modperl");
	newXS("ZNC::GetString", XS_ZNC_GetString, "modperl");
	newXS("ZNC::LoadMod", XS_ZNC_LoadMod, "modperl");
	newXS("ZNC::UnloadMod", XS_ZNC_UnloadMod, "modperl");
	newXS("ZNC::WriteSock", XS_ZNC_WriteSock, "modperl");
	newXS("ZNC::CloseSock", XS_ZNC_CloseSock, "modperl");
	newXS("ZNC::SetSockValue", XS_ZNC_SetSockValue, "modperl");

	// this sets up the eval CB that we call from here on out. this way we can grab the error produced
	if (!SetupZNCScript()) {
		sMessage = "Failed to load modperl.pm";
		return false;
	}

	HV *pZNCSpace = get_hv("ZNC::", TRUE);

	if (!pZNCSpace)
		return(false);

	sv_2mortal((SV*)pZNCSpace);

	newCONSTSUB(pZNCSpace, "CONTINUE", PString(CONTINUE).GetSV());
	newCONSTSUB(pZNCSpace, "HALT", PString(HALT).GetSV());
	newCONSTSUB(pZNCSpace, "HALTMODS", PString(HALTMODS).GetSV());
	newCONSTSUB(pZNCSpace, "HALTCORE", PString(HALTCORE).GetSV());

	return(true);
}

void CModPerl::LoadPerlMod(const CString & sModule)
{
	if (!m_pUser)
	{
		DEBUG_ONLY(cerr << "LoadPerlMod: No User is set!!!" << endl);
		return;
	}

	CString sModPath, sTmp;

	if (!CZNC::Get().FindModPath(sModule, sModPath, sTmp))
		PutStatus("No such module " + sModule);
	else
	{
		PutStatus("Using " + sModPath);
		Eval("ZNC::CORELoadMod('" + m_pUser->GetUserName() + "', '" + sModPath + "');");
	}
}

void CModPerl::DestroyAllSocks(const CString & sModuleName)
{
	for (u_int a = 0; a < m_pManager->size(); a++)
	{
		if ((*m_pManager)[a]->GetSockName() == ZNCSOCK)
		{
			if ((sModuleName.empty()) || (sModuleName == ((CPerlSock *)(*m_pManager)[a])->GetModuleName()))
				m_pManager->DelSock(a--);
		}
	}
}
void CModPerl::UnloadPerlMod(const CString & sModule)
{
	DestroyAllSocks(sModule);
	if (!m_pUser)
	{
		DEBUG_ONLY(cerr << "UnloadPerlMod: No User is set!!!" << endl);
		return;
	}
	Eval("ZNC::COREUnloadMod('" + m_pUser->GetUserName() + "', '" + sModule + "');");
}


CModPerl::EModRet CModPerl::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort,
		const CString& sFile, unsigned long uFileSize)
{
	VPString vsArgs;
	vsArgs.push_back(NICK(RemoteNick));
	vsArgs.push_back(uLongIP);
	vsArgs.push_back(uPort);
	vsArgs.push_back(sFile);

	return(CallBack("OnDCCUserSend", vsArgs));
}

void CPerlTimer::RunJob()
{
	if (!((CModPerl *)m_pModule)->GetUser(m_sUserName, true))
	{
		Stop();
		return;
	}

	VPString vArgs;
	vArgs.push_back(m_sModuleName);
	if (((CModPerl *)m_pModule)->CallBack(m_sFuncName, vArgs, CModPerl::CB_TIMER) != CModPerl::CONTINUE)
		Stop();

	((CModPerl *)m_pModule)->UnSetUser();
}

/////////////////////////// CPerlSock stuff ////////////////////
#define SOCKCB(a) if (CallBack(a) != CModPerl::CONTINUE) { Close(); }
int CPerlSock::CallBack(const PString & sFuncName)
{
	if (!g_ModPerl->GetUser(m_sUsername, true))
	{
		Close();
		return(CModPerl::HALT);
	}
	int i = g_ModPerl->CallBack(sFuncName, m_vArgs, CModPerl::CB_SOCK, m_sUsername);
	g_ModPerl->UnSetUser();
	return(i);
}

// # OnConnect($sockhandle, $parentsockhandle)
void CPerlSock::Connected()
{
	if (GetType() == INBOUND)
	{
		m_vArgs.clear();
		m_vArgs.push_back(m_sModuleName);
		m_vArgs.push_back(m_iParentFD);
		m_vArgs.push_back(GetRSock());
		SOCKCB("OnNewSock");
	}
	SetupArgs();
	if (GetType() == INBOUND)
		AddArg(m_iParentFD);

	SOCKCB("OnConnect")
}

// # OnConnectionFrom($sockhandle, $remotehost, $remoteport)
bool CPerlSock::ConnectionFrom(const CS_STRING & sHost, u_short iPort)
{
	SetupArgs();
	AddArg(sHost);
	AddArg(iPort);

	// special case here
	if (CallBack("OnConnectionFrom") != CModPerl::CONTINUE)
		return(false);

	return(true);
}


// # OnError($sockhandle, $errno)
void CPerlSock::SockError(int iErrno)
{
	SetupArgs();
	AddArg(iErrno);
	SOCKCB("OnError")
}

// # OnConnectionRefused($sockhandle)
void CPerlSock::ConnectionRefused()
{
	SetupArgs();
	SOCKCB("OnConnectionRefused")
}

// # OnTimeout($sockhandle)
void CPerlSock::Timeout()
{
	SetupArgs();
	SOCKCB("OnTimeout")
}

// # OnDisconnect($sockhandle)
void CPerlSock::Disconnected()
{
	SetupArgs();
	SOCKCB("OnDisconnect");
}
// # OnData($sockhandle, $bytes, $length)
void CPerlSock::ReadData(const char *data, int len)
{
	SetupArgs();
	PString sData;
	sData.append(data, len);
	AddArg(sData);
	AddArg(len);
	SOCKCB("OnData")
}
// # OnReadLine($sockhandle, $line)
void CPerlSock::ReadLine(const CS_STRING & sLine)
{
	SetupArgs();
	AddArg(sLine);
	SOCKCB("OnReadLine");
}

// # OnSockDestroy($sockhandle)
CPerlSock::~CPerlSock()
{
	SetupArgs();
	CallBack("OnSockDestroy");
}

Csock *CPerlSock::GetSockObj(const CS_STRING & sHostname, u_short iPort)
{
	CPerlSock *p = new CPerlSock(sHostname, iPort);
	p->SetParentFD(GetRSock());
	p->SetUsername(m_sUsername);
	p->SetModuleName(m_sModuleName);
	p->SetSockName(ZNCSOCK);
	if (HasReadLine())
		p->EnableReadLine();

	return(p);
}

#endif /* HAVE_PERL */








