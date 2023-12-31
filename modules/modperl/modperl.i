/*
 * Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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
#include "znc/Utils.h"
#include "znc/Threads.h"
#include "znc/Config.h"
#include "znc/Socket.h"
#include "znc/Modules.h"
#include "znc/Nick.h"
#include "znc/Chan.h"
#include "znc/User.h"
#include "znc/IRCNetwork.h"
#include "znc/Client.h"
#include "znc/IRCSock.h"
#include "znc/Listener.h"
#include "znc/HTTPSock.h"
#include "znc/Template.h"
#include "znc/WebModules.h"
#include "znc/znc.h"
#include "znc/Server.h"
#include "znc/ZNCString.h"
#include "znc/FileUtils.h"
#include "znc/ZNCDebug.h"
#include "znc/ExecSock.h"
#include "znc/Buffer.h"
#include "modperl/module.h"
#define stat struct stat
%}

%apply long { off_t };
%apply long { uint16_t };
%apply long { uint32_t };
%apply long { uint64_t };

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

%typemap(out) VCString {
    EXTEND(sp, $1.size());
    for (int i = 0; i < $1.size(); ++i) {
        SV* x = newSV(0);
        SwigSvFromString(x, $1[i]);
        $result = sv_2mortal(x);
        argvi++;
    }
}
%typemap(out) const VCString& {
    EXTEND(sp, $1->size());
    for (int i = 0; i < $1->size(); ++i) {
        SV* x = newSV(0);
        SwigSvFromString(x, (*$1)[i]);
        $result = sv_2mortal(x);
        argvi++;
    }
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

#define REGISTER_ZNC_MESSAGE(M) \
    %template(As_ ## M) CMessage::As<M>;

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
#include "znc/zncconfig.h"
#include "znc/ZNCString.h"
%include "znc/defines.h"
%include "znc/Translation.h"
%include "znc/Utils.h"
%include "znc/Threads.h"
%include "znc/Config.h"
%include "znc/Csocket.h"
%template(ZNCSocketManager) TSocketManager<CZNCSock>;
%include "znc/Socket.h"
%include "znc/FileUtils.h"
%include "znc/Message.h"
%include "znc/Modules.h"
%include "znc/Nick.h"
%include "znc/Chan.h"
%include "znc/User.h"
%include "znc/IRCNetwork.h"
%include "znc/Client.h"
%include "znc/IRCSock.h"
%include "znc/Listener.h"
%include "znc/HTTPSock.h"
%include "znc/Template.h"
%include "znc/WebModules.h"
%include "znc/znc.h"
%include "znc/Server.h"
%include "znc/ZNCDebug.h"
%include "znc/ExecSock.h"
%include "znc/Buffer.h"

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
    VCString GetNVKeys() {
        VCString result;
        for (auto i = $self->BeginNV(); i != $self->EndNV(); ++i) {
            result.push_back(i->first);
        }
        return result;
    }
	bool ExistsNV(const CString& sName) {
		return $self->EndNV() != $self->FindNV(sName);
	}
}

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
		return std::make_shared<CWebSubPage>(sName, sTitle, vParams, uFlags);
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
