/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "FileUtils.h"
#include "IRCSock.h"
#include "Modules.h"
#include "Nick.h"
#include "User.h"
#include "znc.h"

#include "modperl/module.h"
#include "modperl/swigperlrun.h"
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "modperl/pstring.h"

// Allows perl to load .so files when needed by .pm
// For example, it needs to load ZNC.so
extern "C" {
	void boot_DynaLoader (pTHX_ CV* cv);
	static void xs_init(pTHX) {
		newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);
	}
}

class CModPerl: public CGlobalModule {
	PerlInterpreter *m_pPerl;
public:
	GLOBALMODCONSTRUCTOR(CModPerl) {
		m_pPerl = NULL;
	}

#define PSTART dSP; I32 ax; int ret = 0; ENTER; SAVETMPS; PUSHMARK(SP)
#define PCALL(name) PUTBACK; ret = call_pv(name, G_EVAL|G_ARRAY); SPAGAIN; SP -= ret; ax = (SP - PL_stack_base) + 1
#define PEND ax += 0; PUTBACK; FREETMPS; LEAVE
#define PUSH_STR(s) XPUSHs(PString(s).GetSV())
#define PUSH_PTR(type, p) XPUSHs(SWIG_NewInstanceObj(const_cast<type>(p), SWIG_TypeQuery(#type), SWIG_SHADOW))

	bool OnLoad(const CString& sArgsi, CString& sMessage) {
		CString sModPath, sTmp;
		if (!CModules::FindModPath("modperl/startup.pl", sModPath, sTmp)) {
			sMessage = "startup.pl not found.";
			return false;
		}
		sTmp = CDir::ChangeDir(sModPath, "..");
		int argc = 6;
		char *pArgv[] = {"", "-T", "-w",
			"-I", const_cast<char*>(sTmp.c_str()),
			const_cast<char*>(sModPath.c_str()), NULL};
		char **argv = pArgv;
		PERL_SYS_INIT3(&argc, &argv, &environ);
		m_pPerl = perl_alloc();
		perl_construct(m_pPerl);
		if (perl_parse(m_pPerl, xs_init, argc, argv, environ)) {
			perl_free(m_pPerl);
			PERL_SYS_TERM();
			m_pPerl = NULL;
			sMessage = "Can't initialize perl.";
			DEBUG(__PRETTY_FUNCTION__ << " can't init perl");
			return false;
		}
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
		PSTART;
		PCALL("ZNC::Core::Init");
		PEND;
		return true;
	}

	virtual EModRet OnModuleLoading(const CString& sModName, const CString& sArgs,
			bool& bSuccess, CString& sRetMsg) {
		if (!GetUser()) {
			return CONTINUE;
		}
		EModRet result = HALT;
		PSTART;
		PUSH_STR(sModName);
		PUSH_STR(sArgs);
		PUSH_PTR(CUser*, GetUser());
		PCALL("ZNC::Core::LoadModule");

		if (SvTRUE(ERRSV)) {
			sRetMsg = PString(ERRSV);
			bSuccess = false;
			result = HALT;
			DEBUG("Perl ZNC::Core::LoadModule died: " << sRetMsg);
		} else if (ret < 1 || 2 < ret) {
			sRetMsg = "Error: Perl ZNC::Core::LoadModule returned " + CString(ret) + " values.";
			bSuccess = false;
			result = HALT;
		} else {
			ELoadPerlMod eLPM = static_cast<ELoadPerlMod>(SvUV(ST(0)));
			if (Perl_NotFound == eLPM) {
				result = CONTINUE; // Not a Perl module
			} else {
				sRetMsg = PString(ST(1));
				result = HALT;
				bSuccess = eLPM == Perl_Loaded;
			}
		}

		PEND;
		return result;
	}

	virtual EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg) {
		CPerlModule* pMod = AsPerlModule(pModule);
		if (pMod) {
			CString sModName = pMod->GetModName();
			PSTART;
			PUSH_PTR(CPerlModule*, pMod);
			PCALL("ZNC::Core::UnloadModule");
			if (SvTRUE(ERRSV)) {
				bSuccess = false;
				sRetMsg = PString(ERRSV);
			} else {
				bSuccess = true;
				sRetMsg = "Module [" + sModName + "] unloaded";
			}
			PEND;
			DEBUG(__PRETTY_FUNCTION__ << " " << sRetMsg);
			return HALT;
		}
		return CONTINUE;
	}

	virtual EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
			bool& bSuccess, CString& sRetMsg) {
		PSTART;
		PUSH_STR(sModule);
		PCALL("ZNC::Core::GetModInfo");
		EModRet result = CONTINUE;
		if (SvTRUE(ERRSV)) {
			bSuccess = false;
			sRetMsg = PString(ERRSV);
		} else if (0 < ret) {
			switch(static_cast<ELoadPerlMod>(SvUV(ST(0)))) {
				case Perl_NotFound:
					result = CONTINUE;
					break;
				case Perl_Loaded:
					result = HALT;
					if (4 == ret) {
						ModInfo.SetGlobal(false);
						ModInfo.SetDescription(PString(ST(2)));
						ModInfo.SetName(sModule);
						ModInfo.SetPath(PString(ST(1)));
						ModInfo.SetWikiPage(PString(ST(3)));
						bSuccess = true;
					} else {
						bSuccess = false;
						sRetMsg = "Something weird happened";
					}
					break;
				case Perl_LoadError:
					result = HALT;
					bSuccess = false;
					if (2 == ret) {
						sRetMsg = PString(ST(1));
					} else {
						sRetMsg = "Something weird happened";
					}
			}
		} else {
			result = HALT;
			bSuccess = false;
			sRetMsg = "Something weird happened";
		}
		PEND;
		DEBUG(__PRETTY_FUNCTION__ << " " << sRetMsg);
		return result;
	}

	virtual void OnGetAvailableMods(set<CModInfo>& ssMods, bool bGlobal) {
		if (bGlobal) {
			return;
		}

		unsigned int a = 0;
		CDir Dir;

		CModules::ModDirList dirs = CModules::GetModDirs();

		while (!dirs.empty()) {
			Dir.FillByWildcard(dirs.front().first, "*.pm");
			dirs.pop();

			for (a = 0; a < Dir.size(); a++) {
				CFile& File = *Dir[a];
				CString sName = File.GetShortName();
				CString sPath = File.GetLongName();
				CModInfo ModInfo;
				sName.RightChomp(3);
				PSTART;
				PUSH_STR(sPath);
				PUSH_STR(sName);
				PCALL("ZNC::Core::ModInfoByPath");
				if (!SvTRUE(ERRSV) && ret == 2) {
					ModInfo.SetGlobal(false);
					ModInfo.SetDescription(PString(ST(0)));
					ModInfo.SetName(sName);
					ModInfo.SetPath(sPath);
					ModInfo.SetWikiPage(PString(ST(1)));
					ssMods.insert(ModInfo);
				}
				PEND;
			}
		}
	}

	virtual ~CModPerl() {
		if (m_pPerl) {
			PSTART;
			PCALL("ZNC::Core::UnloadAll");
			PEND;
			perl_destruct(m_pPerl);
			perl_free(m_pPerl);
			PERL_SYS_TERM();
		}
	}

};

#include "modperl/functions.cpp"

VWebSubPages& CPerlModule::GetSubPages() {
	VWebSubPages* result = _GetSubPages();
	if (!result) {
		return CModule::GetSubPages();
	}
	return *result;
}

void CPerlTimer::RunJob() {
	CPerlModule* pMod = AsPerlModule(GetModule());
	if (pMod) {
		PSTART;
		PUSH_STR(pMod->GetPerlID());
		PUSH_STR(GetPerlID());
		PCALL("ZNC::Core::CallTimer");
		PEND;
	}
}

CPerlTimer::~CPerlTimer() {
	CPerlModule* pMod = AsPerlModule(GetModule());
	if (pMod) {
		PSTART;
		PUSH_STR(pMod->GetPerlID());
		PUSH_STR(GetPerlID());
		PCALL("ZNC::Core::RemoveTimer");
		PEND;
	}
}

#define SOCKSTART PSTART; PUSH_STR(pMod->GetPerlID()); PUSH_STR(GetPerlID())
#define SOCKCBCHECK(OnSuccess) PCALL("ZNC::Core::CallSocket"); if (SvTRUE(ERRSV)) { Close(); DEBUG("Perl socket hook died with: " + PString(ERRSV)); } else { OnSuccess; } PEND
#define CBSOCK(Func) void CPerlSocket::Func() {\
	CPerlModule* pMod = AsPerlModule(GetModule());\
	if (pMod) {\
		SOCKSTART;\
		PUSH_STR("On" #Func);\
		SOCKCBCHECK();\
	}\
}
CBSOCK(Connected);
CBSOCK(Disconnected);
CBSOCK(Timeout);
CBSOCK(ConnectionRefused);

void CPerlSocket::ReadData(const char *data, size_t len) {
	CPerlModule* pMod = AsPerlModule(GetModule());
	if (pMod) {
		SOCKSTART;
		PUSH_STR("OnReadData");
		XPUSHs(sv_2mortal(newSVpvn(data, len)));
		mXPUSHi(len);
		SOCKCBCHECK();
	}
}
void CPerlSocket::ReadLine(const CString& sLine) {
	CPerlModule* pMod = AsPerlModule(GetModule());
	if (pMod) {
		SOCKSTART;
		PUSH_STR("OnReadLine");
		PUSH_STR(sLine);
		SOCKCBCHECK();
	}
}
Csock* CPerlSocket::GetSockObj(const CString& sHost, unsigned short uPort) {
	CPerlModule* pMod = AsPerlModule(GetModule());
	Csock* result = NULL;
	if (pMod) {
		SOCKSTART;
		PUSH_STR("_Accepted");
		PUSH_STR(sHost);
		mXPUSHi(uPort);
		SOCKCBCHECK(
				result = SvToPtr<CPerlSocket>("CPerlSocket*")(ST(0));
		);
	}
	return result;
}

CPerlSocket::~CPerlSocket() {
	CPerlModule* pMod = AsPerlModule(GetModule());
	if (pMod) {
		SOCKSTART;
		PCALL("ZNC::Core::RemoveSocket");
		PEND;
	}
}

template<> void TModInfo<CModPerl>(CModInfo& Info) {
	Info.SetWikiPage("modperl");
}

GLOBALMODULEDEFS(CModPerl, "Loads perl scripts as ZNC modules")
