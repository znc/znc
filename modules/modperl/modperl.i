/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

%module ZNC %{
#ifdef Copy
# undef Copy
#endif
#ifdef Pause
# undef Pause
#endif
#include <utility>
#include "../Utils.h"
#include "../Socket.h"
#include "../Modules.h"
#include "../Nick.h"
#include "../Chan.h"
#include "../User.h"
#include "../Client.h"
#include "../IRCSock.h"
#include "../Listener.h"
#include "../HTTPSock.h"
#include "../Template.h"
#include "../WebModules.h"
#include "../znc.h"
#include "../Server.h"
#include "../ZNCString.h"
#include "../FileUtils.h"
#include "../ZNCDebug.h"
#include "../ExecSock.h"
#include "modperl/module.h"
#define stat struct stat
%}

%begin %{
#include "zncconfig.h"
%}

%include <typemaps.i>
%include <stl.i>
%include <std_list.i>

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

#define u_short unsigned short
#define u_int unsigned int
#include "../ZNCString.h"
%include "../defines.h"
%include "../Utils.h"
%include "../Csocket.h"
%template(ZNCSocketManager) TSocketManager<CZNCSock>;
%include "../Socket.h"
%include "../FileUtils.h"
%include "../Modules.h"
%include "../Nick.h"
%include "../Chan.h"
%include "../User.h"
%include "../Client.h"
%include "../IRCSock.h"
%include "../Listener.h"
%include "../HTTPSock.h"
%include "../Template.h"
%include "../WebModules.h"
%include "../znc.h"
%include "../Server.h"
%include "../ZNCDebug.h"
%include "../ExecSock.h"

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

/* Web */

%template(StrPair) pair<CString, CString>;
%template(VPair) vector<pair<CString, CString> >;
typedef vector<pair<CString, CString> > VPair;
%template(VWebSubPages) vector<TWebSubPage>;

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
%}

/* vim: set filetype=cpp noexpandtab: */
