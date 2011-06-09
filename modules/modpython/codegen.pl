#!/usr/bin/env perl
#
# Copyright (C) 2004-2011  See the AUTHORS file for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#
# Parts of SWIG are used here.

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
 *
 * Parts of SWIG are used here.
 */

/***************************************************************************
 * This file is generated automatically using codegen.pl from functions.in *
 * Don't change it manually.                                               *
 ***************************************************************************/

namespace {
/*	template<class T>
	struct pyobj_to_ptr {
		CString m_sType;
		SvToPtr(const CString& sType) {
			m_sType = sType;
		}
		bool operator()(PyObject* py, T** result) {
			T* x = NULL;
			int res = SWIG_ConvertPtr(sv, (void**)&x, SWIG_TypeQuery(m_sType.c_str()), 0);
			if (SWIG_IsOK(res)) {
				*result = x;
				return true;
			}
			DEBUG("modpython: ");
			return false;
		}
	};

	CModule::EModRet SvToEModRet(PyObject* py, CModule::EModRet* result) {
		long int x = PyLong_AsLong();
		return static_cast<CModule::EModRet>(SvUV(sv));
	}*/

	inline swig_type_info* SWIG_pchar_descriptor(void) {
		static int init = 0;
		static swig_type_info* info = 0;
		if (!init) {
			info = SWIG_TypeQuery("_p_char");
			init = 1;
		}
		return info;
	}

	inline int SWIG_AsCharPtrAndSize(PyObject *obj, char** cptr, size_t* psize, int *alloc) {
#if PY_VERSION_HEX>=0x03000000
		if (PyUnicode_Check(obj))
#else
			if (PyString_Check(obj))
#endif
			{
				char *cstr; Py_ssize_t len;
#if PY_VERSION_HEX>=0x03000000
				if (!alloc && cptr) {
					/* We can't allow converting without allocation, since the internal
					   representation of string in Python 3 is UCS-2/UCS-4 but we require
					   a UTF-8 representation.
					   TODO(bhy) More detailed explanation */
					return SWIG_RuntimeError;
				}
				obj = PyUnicode_AsUTF8String(obj);
				PyBytes_AsStringAndSize(obj, &cstr, &len);
				if(alloc) *alloc = SWIG_NEWOBJ;
#else
				PyString_AsStringAndSize(obj, &cstr, &len);
#endif
				if (cptr) {
					if (alloc) {
						/*
						   In python the user should not be able to modify the inner
						   string representation. To warranty that, if you define
						   SWIG_PYTHON_SAFE_CSTRINGS, a new/copy of the python string
						   buffer is always returned.

						   The default behavior is just to return the pointer value,
						   so, be careful.
						 */
#if defined(SWIG_PYTHON_SAFE_CSTRINGS)
						if (*alloc != SWIG_OLDOBJ)
#else
							if (*alloc == SWIG_NEWOBJ)
#endif
							{
								*cptr = (char *)memcpy((char *)malloc((len + 1)*sizeof(char)), cstr, sizeof(char)*(len + 1));
								*alloc = SWIG_NEWOBJ;
							}
							else {
								*cptr = cstr;
								*alloc = SWIG_OLDOBJ;
							}
					} else {
#if PY_VERSION_HEX>=0x03000000
						assert(0); /* Should never reach here in Python 3 */
#endif
						*cptr = SWIG_Python_str_AsChar(obj);
					}
				}
				if (psize) *psize = len + 1;
#if PY_VERSION_HEX>=0x03000000
				Py_XDECREF(obj);
#endif
				return SWIG_OK;
			} else {
				swig_type_info* pchar_descriptor = SWIG_pchar_descriptor();
				if (pchar_descriptor) {
					void* vptr = 0;
					if (SWIG_ConvertPtr(obj, &vptr, pchar_descriptor, 0) == SWIG_OK) {
						if (cptr) *cptr = (char *) vptr;
						if (psize) *psize = vptr ? (strlen((char *)vptr) + 1) : 0;
						if (alloc) *alloc = SWIG_OLDOBJ;
						return SWIG_OK;
					}
				}
			}
		return SWIG_TypeError;
	}

	inline int SWIG_AsPtr_std_string (PyObject * obj, CString **val) {
		char* buf = 0 ; size_t size = 0; int alloc = SWIG_OLDOBJ;
		if (SWIG_IsOK((SWIG_AsCharPtrAndSize(obj, &buf, &size, &alloc)))) {
			if (buf) {
				if (val) *val = new CString(buf, size - 1);
				if (alloc == SWIG_NEWOBJ) free((char*)buf);
				return SWIG_NEWOBJ;
			} else {
				if (val) *val = 0;
				return SWIG_OLDOBJ;
			}
		} else {
			static int init = 0;
			static swig_type_info* descriptor = 0;
			if (!init) {
				descriptor = SWIG_TypeQuery("CString" " *");
				init = 1;
			}
			if (descriptor) {
				CString *vptr;
				int res = SWIG_ConvertPtr(obj, (void**)&vptr, descriptor, 0);
				if (SWIG_IsOK(res) && val) *val = vptr;
				return res;
			}
		}
		return SWIG_ERROR;
	}
}

EOF
=b
bool OnFoo(const CString& x) {
	PyObject* pyName = Py_BuildValue("s", "OnFoo");
	if (!pyName) {
		CString s = GetPyExceptionStr();
		DEBUG("modpython: username/module/OnFoo: can't name method to call: " << s);
		return default;
	}
	PyObject* pyArg1 = Py_BuildValue("s", x.c_str());
	if (!pyArg1) {
		CString s = GetPyExceptionStr();
		DEBUG("modpython: username/module/OnFoo: can't convert parameter x to PyObject*: " << s);
		Py_CLEAR(pyName);
		return default;
	}
	PyObject* pyArg2 = ...;
	if (!pyArg2) {
		CString s = ...;
		DEBUG(...);
		Py_CLEAR(pyName);
		Py_CLEAR(pyArg1);
		return default;
	}
	PyObject* pyArg3 = ...;
	if (!pyArg3) {
		CString s = ...;
		DEBUG(...);
		Py_CLEAR(pyName);
		Py_CLEAR(pyArg1);
		Py_CLEAR(pyArg2);
		return default;
	}
	PyObject* pyRes = PyObject_CallMethodObjArgs(m_pyObj, pyName, pyArg1, pyArg2, pyArg3, NULL);
	if (!pyRes) {
		CString s = ...;
		DEBUG("modpython: username/module/OnFoo failed: " << s);
		Py_CLEAR(pyName);
		Py_CLEAR(pyArg1);
		Py_CLEAR(pyArg2);
		Py_CLEAR(pyArg3);
		return default;
	}
	Py_CLEAR(pyName);
	Py_CLEAR(pyArg1);
	Py_CLEAR(pyArg2);
	Py_CLEAR(pyArg3);
	bool res = PyLong_AsLong(pyRes);
	if (PyErr_Occured()) {
		CString s = GetPyExceptionStr();
		DEBUG("modpython: username/module/OnFoo returned unexpected value: " << s);
		Py_CLEAR(pyRes);
		return default;
	}
	Py_CLEAR(pyRes);
	return res;
}
=cut

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
		my ($tb, $tm) = $t =~ /^(.*?)\s*?(\*|&)?$/;
		{type=>$t, var=>$v, base=>$tb, mod=>$tm//'', pyvar=>"pyArg_$v", error=>"can't convert parameter '$v' to PyObject"}
	} split /,/, $args;

	unshift @arg, {type=>'$func$', var=>"", base=>"", mod=>"", pyvar=>"pyName", error=>"can't convert string '$name' to PyObject"};

	my $cleanup = '';
	$default = '' if $type eq 'void';

	say $out "$type CPyModule::$name($args) {";
	for my $a (@arg) {
		print $out "\tPyObject* $a->{pyvar} = ";
		given ($a->{type}) {
			when ('$func$') {
				say $out "Py_BuildValue(\"s\", \"$name\");";
			}
			when (/vector\s*<\s*.*\*\s*>/) {
				say $out "PyList_New(0);";
			}
			when (/CString/) {
				if ($a->{base} eq 'CString' && $a->{mod} eq '&') {
					say $out "CPyRetString::wrap($a->{var});";
				} else {
					say $out "Py_BuildValue(\"s\", $a->{var}.c_str());";
				}
			}
			when (/\*$/) {
				(my $t = $a->{type}) =~ s/^const//;
				say $out "SWIG_NewInstanceObj(const_cast<$t>($a->{var}), SWIG_TypeQuery(\"$t\"), 0);";
			}
			when (/&$/) {
				(my $b = $a->{base}) =~ s/^const//;
				say $out "SWIG_NewInstanceObj(const_cast<$b*>(&$a->{var}), SWIG_TypeQuery(\"$b*\"), 0);";
			}
			when ('bool') {
				say $out "Py_BuildValue(\"l\", (long int)$a->{var});";
			}
			default {
				my %letter = (
						'int' => 'i',
						'char' => 'b',
						'short int' => 'h',
						'long int' => 'l',
						'unsigned char' => 'B',
						'unsigned short' => 'H',
						'unsigned int' => 'I',
						'unsigned long' => 'k',
						'long long' => 'L',
						'unsigned long long' => 'K',
						'ssize_t' => 'n',
						'double' => 'd',
						'float' => 'f',
						);
				if (exists $letter{$a->{type}}) {
					say $out "Py_BuildValue(\"$letter{$a->{type}}\", $a->{var});"
				} else {
					say $out "...;";
				}
			}
		}
		say $out "\tif (!$a->{pyvar}) {";
		say $out "\t\tCString sPyErr = m_pModPython->GetPyExceptionStr();";
		say $out "\t\tDEBUG".'("modpython: " << GetUser()->GetUserName() << "/" << GetModName() << '."\"/$name: $a->{error}: \" << sPyErr);";
		print $out $cleanup;
		say $out "\t\treturn $default;";
		say $out "\t}";

		$cleanup .= "\t\tPy_CLEAR($a->{pyvar});\n";

		if ($a->{type} =~ /(vector\s*<\s*(.*)\*\s*>)/) {
			my ($vec, $sub) = ($1, $2);
			(my $cleanup1 = $cleanup) =~ s/\t\t/\t\t\t/g;
			my $dot = '.';
			$dot = '->' if $a->{mod} eq '*';
			say $out "\tfor (${vec}::const_iterator i = $a->{var}${dot}begin(); i != $a->{var}${dot}end(); ++i) {";
			say $out "\t\tPyObject* pyVecEl = SWIG_NewInstanceObj(*i, SWIG_TypeQuery(\"$sub*\"), 0);";
			say $out "\t\tif (!pyVecEl) {";
			say $out "\t\t\tCString sPyErr = m_pModPython->GetPyExceptionStr();";
			say $out "\t\t\tDEBUG".'("modpython: " << GetUser()->GetUserName() << "/" << GetModName() << '.
				"\"/$name: can't convert element of vector '$a->{var}' to PyObject: \" << sPyErr);";
			print $out $cleanup1;
			say $out "\t\t\treturn $default;";
			say $out "\t\t}";
			say $out "\t\tif (PyList_Append($a->{pyvar}, pyVecEl)) {";
			say $out "\t\t\tCString sPyErr = m_pModPython->GetPyExceptionStr();";
			say $out "\t\t\tDEBUG".'("modpython: " << GetUser()->GetUserName() << "/" << GetModName() << '.
				"\"/$name: can't add element of vector '$a->{var}' to PyObject: \" << sPyErr);";
			say $out "\t\t\tPy_CLEAR(pyVecEl);";
			print $out $cleanup1;
			say $out "\t\t\treturn $default;";
			say $out "\t\t}";
			say $out "\t\tPy_CLEAR(pyVecEl);";
			say $out "\t}";
		}
	}

	print $out "\tPyObject* pyRes = PyObject_CallMethodObjArgs(m_pyObj";
	print $out ", $_->{pyvar}" for @arg;
	say $out ", NULL);";
	say $out "\tif (!pyRes) {";
	say $out "\t\tCString sPyErr = m_pModPython->GetPyExceptionStr();";
	say $out "\t\tDEBUG".'("modpython: " << GetUser()->GetUserName() << "/" << GetModName() << '."\"/$name failed: \" << sPyErr);";
	print $out $cleanup;
	say $out "\t\treturn $default;";
	say $out "\t}";

	$cleanup =~ s/\t\t/\t/g;

	print $out $cleanup;

	if ($type ne 'void') {
		say $out "\t$type result;";
		say $out "\tif (pyRes == Py_None) {";
		say $out "\t\tresult = $default;";
		say $out "\t} else {";
		given ($type) {
			when (/^(.*)\*$/) {
				say $out "\t\tint res = SWIG_ConvertPtr(pyRes, (void**)&result, SWIG_TypeQuery(\"$type\"), 0);";
				say $out "\t\tif (!SWIG_IsOK(res)) {";
				say $out "\t\t\tDEBUG(\"modpython: \" << GetUser()->GetUserName() << \"/\" << GetModName() << \"/$name was expected to return '$type' but error=\" << res);";
				say $out "\t\t\tresult = $default;";
				say $out "\t\t}";
			}
			when ('CString') {
				say $out "\t\tCString* p = NULL;";
				say $out "\t\tint res = SWIG_AsPtr_std_string(pyRes, &p);";
				say $out "\t\tif (!SWIG_IsOK(res)) {";
				say $out "\t\t\tDEBUG(\"modpython: \" << GetUser()->GetUserName() << \"/\" << GetModName() << \"/$name was expected to return '$type' but error=\" << res);";
				say $out "\t\t\tresult = $default;";
				say $out "\t\t} else if (!p) {";
				say $out "\t\t\tDEBUG(\"modpython: \" << GetUser()->GetUserName() << \"/\" << GetModName() << \"/$name was expected to return '$type' but returned NULL\");";
				say $out "\t\t\tresult = $default;";
				say $out "\t\t} else result = *p;";
				say $out "\t\tif (SWIG_IsNewObj(res)) free((char*)p); // Don't ask me, that's how SWIG works...";
			}
			when ('CModule::EModRet') {
				say $out "\t\tlong int x = PyLong_AsLong(pyRes);";
				say $out "\t\tif (PyErr_Occurred()) {";
				say $out "\t\t\tCString sPyErr = m_pModPython->GetPyExceptionStr();";
				say $out "\t\t\tDEBUG".'("modpython: " << GetUser()->GetUserName() << "/" << GetModName() << '."\"/$name was expected to return EModRet but: \" << sPyErr);";
				say $out "\t\t\tresult = $default;";
				say $out "\t\t} else { result = (CModule::EModRet)x; }";
			}
			when ('bool') {
  				say $out "\t\tint x = PyObject_IsTrue(pyRes);";
				say $out "\t\tif (-1 == x) {";
				say $out "\t\t\tCString sPyErr = m_pModPython->GetPyExceptionStr();";
				say $out "\t\t\tDEBUG".'("modpython: " << GetUser()->GetUserName() << "/" << GetModName() << '."\"/$name was expected to return EModRet but: \" << sPyErr);";
				say $out "\t\t\tresult = $default;";
				say $out "\t\t} else result = x ? true : false;";
			}
			default {
				say $out "\t\tI don't know how to convert PyObject to $type :(";
			}
		}
		say $out "\t}";
		say $out "\tPy_CLEAR(pyRes);";
		say $out "\treturn result;";
	} else {
		say $out "\tPy_CLEAR(pyRes);";
	}
	say $out "}\n";
}

sub getres {
	my $type = shift;
	given ($type) {
		when (/^(.*)\*$/)		 { return "pyobj_to_ptr<$1>(\"$type\")" }
		when ('CString')		  { return 'PString' }
		when ('CModule::EModRet') { return 'SvToEModRet' }
		when (/unsigned/)		 { return 'SvUV' }
		default				   { return 'SvIV' }
	}
}
