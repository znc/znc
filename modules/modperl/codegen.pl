#!/usr/bin/env perl
#
# Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

use strict;
use warnings;
use IO::File;
use feature 'switch', 'say';

open my $in, $ARGV[0] or die;
open my $out, ">", $ARGV[1] or die;

print $out <<'EOF';
/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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

/***************************************************************************
 * This file is generated automatically using codegen.pl from functions.in *
 * Don't change it manually.                                               *
 ***************************************************************************/

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
			return nullptr;
		}
	};

	CModule::EModRet SvToEModRet(SV* sv) {
		return static_cast<CModule::EModRet>(SvUV(sv));
	}
}
#define PSTART_IDF(Func) PSTART; XPUSHs(GetPerlObj()); PUSH_STR(#Func)
#define PCALLMOD(Error, Success) PCALL("ZNC::Core::CallModFunc"); if (SvTRUE(ERRSV)) { DEBUG("Perl hook died with: " + PString(ERRSV)); Error; } else if (SvIV(ST(0))) { Success; } else { Error; } PEND

EOF

while (<$in>) {
	my ($type, $name, $args, $default) = /(\S+)\s+(\w+)\((.*)\)(?:=(\w+))?/ or next;
	$type =~ s/(EModRet)/CModule::$1/;
	$type =~ s/^\s*(.*?)\s*$/$1/;
	my @arg = map {
		my ($t, $v) = /^\s*(.*\W)\s*(\w+)\s*$/;
		$t =~ s/^\s*(.*?)\s*$/$1/;
		my ($tt, $tm) = $t =~ /^(.*?)\s*?(\*|&)?$/;
		{type=>$t, var=>$v, base=>$tt, mod=>$tm//''}
	} split /,/, $args;
	unless (defined $default) {
		$default = "CModule::$name(" . (join ', ', map { $_->{var} } @arg) . ")";
	}
	say $out "$type CPerlModule::$name($args) {";
	say $out "\t$type result{};" if $type ne 'void';
	say $out "\tPSTART_IDF($name);";
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
			when (/SCString/)	  { my $b=$a->{base}; $b=~s/^const//; say $out "\tPUSH_PTR($b*, &$a->{var});" }
			when (/CString/) { say $out "\tPUSH_STR($a->{var});" }
			when (/\*$/)	 { my $t=$a->{type}; $t=~s/^const//; say $out "\tPUSH_PTR($t, $a->{var});" }
			when (/&$/)	  { my $b=$a->{base}; $b=~s/^const//; say $out "\tPUSH_PTR($b*, &$a->{var});" }
			when (/unsigned/){ say $out "\tmXPUSHu($a->{var});" }
			default		  { say $out "\tmXPUSHi($a->{var});" }
		}
	}
	say $out "\tPCALLMOD(";
	print $out "\t\t";
	print $out "result = " if $type ne 'void';
	say $out "$default;,";
	my $x = 1;
	say $out "\t\tresult = ".sv($type)."(ST(1));" if $type ne 'void';
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
