/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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
