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

#include <Python.h>
#include <string>

void fail(PyObject* py, int n) {
	// Doesn't clear any variables, but meh, finalize anyway...
	if (!py) {
		PyErr_Print();
		Py_Finalize();
		exit(n);
	}
}

int main(int argc, char** argv) {
	// Don't use this as an example: this has awful memory leaks.
	Py_Initialize();

	PyObject* pyModule = PyImport_ImportModule("py_compile");
	fail(pyModule, 1);

	PyObject* pyFunc = PyObject_GetAttrString(pyModule, "compile");
	fail(pyFunc, 2);

	std::string cfile = argv[2];
	if (cfile.find('/') == std::string::npos) {
		cfile = "./" + cfile;
	}
	PyObject* pyKW = Py_BuildValue("{sssN}", "cfile", cfile.c_str(), "doraise", Py_True);
	fail(pyKW, 3);

	PyObject* pyArg = Py_BuildValue("(s)", argv[1]);
	fail(pyArg, 4);

	PyObject* pyRes = PyObject_Call(pyFunc, pyArg, pyKW);
	fail(pyRes, 5);

	Py_Finalize();
	return 0;
}
