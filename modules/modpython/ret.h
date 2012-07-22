/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#pragma once

class CPyRetString {
public:
	CString& s;
	CPyRetString(CString& S) : s(S) {}
	static PyObject* wrap(CString& S) {
		CPyRetString* x = new CPyRetString(S);
		return SWIG_NewInstanceObj(x, SWIG_TypeQuery("CPyRetString*"), SWIG_POINTER_OWN);
	}
};

class CPyRetBool {
public:
	bool& b;
	CPyRetBool(bool& B) : b(B) {}
	static PyObject* wrap(bool& B) {
		CPyRetBool* x = new CPyRetBool(B);
		return SWIG_NewInstanceObj(x, SWIG_TypeQuery("CPyRetBool*"), SWIG_POINTER_OWN);
	}
};
