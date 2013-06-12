/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

%module ZNC %{
#ifdef Copy
# undef Copy
#endif
#ifdef Pause
# undef Pause
#endif
#ifdef seed
# undef seed
#endif
#include <utility>
#include "../include/znc/Utils.h"
#include "../include/znc/Config.h"
#include "../include/znc/Socket.h"
#include "../include/znc/Modules.h"
#include "../include/znc/Nick.h"
#include "../include/znc/Chan.h"
#include "../include/znc/User.h"
#include "../include/znc/IRCNetwork.h"
#include "../include/znc/Client.h"
#include "../include/znc/IRCSock.h"
#include "../include/znc/Listener.h"
#include "../include/znc/HTTPSock.h"
#include "../include/znc/Template.h"
#include "../include/znc/WebModules.h"
#include "../include/znc/znc.h"
#include "../include/znc/Server.h"
#include "../include/znc/ZNCString.h"
#include "../include/znc/FileUtils.h"
#include "../include/znc/ZNCDebug.h"
#include "../include/znc/ExecSock.h"
#include "../include/znc/Buffer.h"
#include "modperl/module.h"
#define stat struct stat
%}

%apply long { off_t };

%begin %{
#include "znc/zncconfig.h"
%}

%include <typemaps.i>
%include <stl.i>
%include <std_list.i>
%include <std_deque.i>

namespace std {
	template<class K> class set {
		public:
		set();
		set(const set<K>&);
	};
}
%include "modperl/CString.i"
%template(_stringlist) std::list<CString>;
%typemap(out) std::list<CString> {
	std::list<CString>::const_iterator i;
	unsigned int j;
	int len = $1.size();
	SV **svs = new SV*[len];
	for (i=$1.begin(), j=0; i!=$1.end(); i++, j++) {
		svs[j] = sv_newmortal();
		SwigSvFromString(svs[j], *i);
	}
	AV *myav = av_make(len, svs);
	delete[] svs;
	$result = newRV_noinc((SV*) myav);
	sv_2mortal($result);
	argvi++;
}

%template(VIRCNetworks) std::vector<CIRCNetwork*>;
%template(VChannels) std::vector<CChan*>;
%template(VCString) std::vector<CString>;
typedef std::vector<CString> VCString;
/*%template(MNicks) std::map<CString, CNick>;*/
/*%template(SModInfo) std::set<CModInfo>;
%template(SCString) std::set<CString>;
typedef std::set<CString> SCString;*/
%template(PerlMCString) std::map<CString, CString>;
class MCString : public std::map<CString, CString> {};
/*%template(PerlModulesVector) std::vector<CModule*>;*/
%template(VListeners) std::vector<CListener*>;
%template(BufLines) std::deque<CBufLine>;
%template(VVString) std::vector<VCString>;

%typemap(out) std::map<CString, CNick> {
	HV* myhv = newHV();
	for (std::map<CString, CNick>::const_iterator i = $1.begin(); i != $1.end(); ++i) {
		SV* val = SWIG_NewInstanceObj(const_cast<CNick*>(&i->second), SWIG_TypeQuery("CNick*"), SWIG_SHADOW);
		SvREFCNT_inc(val);// it was created mortal
		hv_store(myhv, i->first.c_str(), i->first.length(), val, 0);
	}
	$result = newRV_noinc((SV*)myhv);
	sv_2mortal($result);
	argvi++;
}

#define u_short unsigned short
#define u_int unsigned int
#include "../include/znc/ZNCString.h"
%include "../include/znc/defines.h"
%include "../include/znc/Utils.h"
%include "../include/znc/Config.h"
%include "../include/znc/Csocket.h"
%template(ZNCSocketManager) TSocketManager<CZNCSock>;
%include "../include/znc/Socket.h"
%include "../include/znc/FileUtils.h"
%include "../include/znc/Modules.h"
%include "../include/znc/Nick.h"
%include "../include/znc/Chan.h"
%include "../include/znc/User.h"
%include "../include/znc/IRCNetwork.h"
%include "../include/znc/Client.h"
%include "../include/znc/IRCSock.h"
%include "../include/znc/Listener.h"
%include "../include/znc/HTTPSock.h"
%include "../include/znc/Template.h"
%include "../include/znc/WebModules.h"
%include "../include/znc/znc.h"
%include "../include/znc/Server.h"
%include "../include/znc/ZNCDebug.h"
%include "../include/znc/ExecSock.h"
%include "../include/znc/Buffer.h"

%include "modperl/module.h"

%inline %{
	class String : public CString {
		public:
			String() {}
			String(const CString& s)	: CString(s) {}
			String(double d, int prec=2): CString(d, prec) {}
			String(float f, int prec=2) : CString(f, prec) {}
			String(int i)			   : CString(i) {}
			String(unsigned int i)	  : CString(i) {}
			String(long int i)		  : CString(i) {}
			String(unsigned long int i) : CString(i) {}
			String(char c)			  : CString(c) {}
			String(unsigned char c)	 : CString(c) {}
			String(short int i)		 : CString(i) {}
			String(unsigned short int i): CString(i) {}
			String(bool b)			  : CString(b) {}
			CString GetPerlStr() {
				return *this;
			}
	};
%}

%extend CModule {
	std::list<CString> _GetNVKeys() {
		std::list<CString> res;
		for (MCString::iterator i = $self->BeginNV(); i != $self->EndNV(); ++i) {
			res.push_back(i->first);
		}
		return res;
	}
	bool ExistsNV(const CString& sName) {
		return $self->EndNV() != $self->FindNV(sName);
	}
}

%perlcode %{
	package ZNC::CModule;
	sub GetNVKeys {
		my $result = _GetNVKeys(@_);
		return @$result;
	}
%}

%extend CModules {
	void push_back(CModule* p) {
		$self->push_back(p);
	}
	bool removeModule(CModule* p) {
		for (CModules::iterator i = $self->begin(); $self->end() != i; ++i) {
			if (*i == p) {
				$self->erase(i);
				return true;
			}
		}
		return false;
	}
}

%extend CUser {
	std::vector<CIRCNetwork*> GetNetworks_() {
		return $self->GetNetworks();
	}
}

%extend CIRCNetwork {
	std::vector<CChan*> GetChans_() {
		return $self->GetChans();
	}
}

%extend CChan {
	std::map<CString, CNick> GetNicks_() {
		return $self->GetNicks();
	}
}

/* Web */

%template(StrPair) std::pair<CString, CString>;
%template(VPair) std::vector<std::pair<CString, CString> >;
typedef std::vector<std::pair<CString, CString> > VPair;
%template(VWebSubPages) std::vector<TWebSubPage>;

%inline %{
	void _VPair_Add2Str(VPair* self, const CString& a, const CString& b) {
		self->push_back(std::make_pair(a, b));
	}
%}

%extend CTemplate {
	void set(const CString& key, const CString& value) {
		(*$self)[key] = value;
	}
}

%inline %{
	TWebSubPage _CreateWebSubPage(const CString& sName, const CString& sTitle, const VPair& vParams, unsigned int uFlags) {
		return new CWebSubPage(sName, sTitle, vParams, uFlags);
	}
%}

%perlcode %{
	package ZNC;
	sub CreateWebSubPage {
		my ($name, %arg) = @_;
		my $params = $arg{params}//{};
		my $vpair = ZNC::VPair->new;
		while (my ($key, $val) = each %$params) {
			ZNC::_VPair_Add2Str($vpair, $key, $val);
		}
		my $flags = 0;
		$flags |= $ZNC::CWebSubPage::F_ADMIN if $arg{admin}//0;
		return _CreateWebSubPage($name, $arg{title}//'', $vpair, $flags);
	}
%}

%inline %{
	void _CleanupStash(const CString& sModname) {
		hv_clear(gv_stashpv(sModname.c_str(), 0));
	}
%}

%perlcode %{
	package ZNC;
	*CONTINUE = *ZNC::CModule::CONTINUE;
	*HALT = *ZNC::CModule::HALT;
	*HALTMODS = *ZNC::CModule::HALTMODS;
	*HALTCORE = *ZNC::CModule::HALTCORE;
	*UNLOAD = *ZNC::CModule::UNLOAD;

	package ZNC::CIRCNetwork;
	*GetChans = *GetChans_;

	package ZNC::CUser;
	*GetNetworks = *GetNetworks_;

	package ZNC::CChan;
	sub _GetNicks_ {
		my $result = GetNicks_(@_);
		return %$result;
	}
	*GetNicks = *_GetNicks_;
%}

/* vim: set filetype=cpp: */
