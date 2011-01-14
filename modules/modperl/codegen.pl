#!/usr/bin/env perl
#
# Copyright (C) 2004-2011  See the AUTHORS file for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#

use strict;
use warnings;
use IO::File;
use feature 'switch', 'say';

open my $in, $ARGV[0] or die;
open my $out, ">", $ARGV[1] or die;

print $out <<'EOF';
/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

/***************************************************************************
 * This file is generated automatically using codegen.pl from functions.in *
 * Don't change it manually.                                               *
 ***************************************************************************/

/*#include "module.h"
#include "swigperlrun.h"
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "pstring.h"*/

namespace {
	template<class T>
	struct SvToPtr {
		CString m_sType;
		SvToPtr(const CString& sType) {
			m_sType = sType;
		}
		T* operator()(SV* sv) {
			T* result;
			int res = SWIG_ConvertPtr(sv, (void**)&result, SWIG_TypeQuery(m_sType.c_str()), 0);
			if (SWIG_IsOK(res)) {
				return result;
			}
			return NULL;
		}
	};

	CModule::EModRet SvToEModRet(SV* sv) {
		return static_cast<CModule::EModRet>(SvUV(sv));
	}
}
/*
#define PSTART dSP; I32 ax; int ret = 0; ENTER; SAVETMPS; PUSHMARK(SP)
#define PCALL(name) PUTBACK; ret = call_pv(name, G_EVAL|G_ARRAY); SPAGAIN; SP -= ret; ax = (SP - PL_stack_base) + 1
#define PEND PUTBACK; FREETMPS; LEAVE
#define PUSH_STR(s) XPUSHs(PString(s).GetSV())
#define PUSH_PTR(type, p) XPUSHs(SWIG_NewInstanceObj(const_cast<type>(p), SWIG_TypeQuery(#type), SWIG_SHADOW))
*/
#define PSTART_IDF(Func) PSTART; PUSH_STR(GetPerlID()); PUSH_STR(#Func)
#define PCALLMOD(Error, Success) PCALL("ZNC::Core::CallModFunc"); if (SvTRUE(ERRSV)) { DEBUG("Perl hook died with: " + PString(ERRSV)); Error; } else { Success; } PEND

EOF

while (<$in>) {
	my ($type, $name, $args, $default) = /(\S+)\s+(\w+)\((.*)\)(?:=(\w+))?/ or next;
	$type =~ s/(EModRet)/CModule::$1/;
	$type =~ s/^\s*(.*?)\s*$/$1/;
	unless (defined $default) {
		given ($type) {
			when ('bool')			 { $default = 'true' }
			when ('CModule::EModRet') { $default = 'CONTINUE' }
			when ('CString')		  { $default = '""' }
			when (/\*$/)			  { $default = "($type)NULL" }
		}
	}
	my @arg = map {
		my ($t, $v) = /^\s*(.*\W)\s*(\w+)\s*$/;
		$t =~ s/^\s*(.*?)\s*$/$1/;
		my ($tt, $tm) = $t =~ /^(.*?)\s*?(\*|&)?$/;
		{type=>$t, var=>$v, base=>$tt, mod=>$tm//''}
	} split /,/, $args;
	say $out "$type CPerlModule::$name($args) {";
	say $out "\t$type result = $default;" if $type ne 'void';
	say $out "\tPSTART_IDF($name);";
	given ($type) {
		when ('CString') { print $out "\tPUSH_STR($default);" }
		when (/\*$/)	 { my $t=$type; $t=~s/^const//; print $out "\tPUSH_PTR($t, $default);" }
		when ('void')	{ print $out "\tmXPUSHi(0);" }
		default		  { print $out "\tmXPUSHi(static_cast<int>($default));" }
	}
	say $out " // Default value";
	for my $a (@arg) {
		given ($a->{type}) {
			when (/(vector\s*<\s*(.*)\*\s*>)/) {
				my ($vec, $sub) = ($1, $2);
				my $dot = '.';
				$dot = '->' if $a->{mod} eq '*';
				say $out "\tfor (${vec}::const_iterator i = $a->{var}${dot}begin(); i != $a->{var}${dot}end(); ++i) {";
#atm sub is always "...*" so...
				say $out "\t\tPUSH_PTR($sub*, *i);";
				say $out "\t}";
			}
			when (/CString/) { say $out "\tPUSH_STR($a->{var});" }
			when (/\*$/)	 { my $t=$a->{type}; $t=~s/^const//; say $out "\tPUSH_PTR($t, $a->{var});" }
			when (/&$/)	  { my $b=$a->{base}; $b=~s/^const//; say $out "\tPUSH_PTR($b*, &$a->{var});" }
			when (/unsigned/){ say $out "\tmXPUSHu($a->{var});" }
			default		  { say $out "\tmXPUSHi($a->{var});" }
		}
	}
	say $out "\tPCALLMOD(,";
	my $x = 0;
	say $out "\t\tresult = ".sv($type)."(ST(0));" if $type ne 'void';
	for my $a (@arg) {
		$x++;
		say $out "\t\t$a->{var} = PString(ST($x));" if $a->{base} eq 'CString' && $a->{mod} eq '&';
	}
	say $out "\t);";
	say $out "\treturn result;" if $type ne 'void';
	say $out "}\n";
}

sub sv {
	my $type = shift;
	given ($type) {
		when (/^(.*)\*$/)		 { return "SvToPtr<$1>(\"$type\")" }
		when ('CString')		  { return 'PString' }
		when ('CModule::EModRet') { return 'SvToEModRet' }
		when (/unsigned/)		 { return 'SvUV' }
		default				   { return 'SvIV' }
	}
}
