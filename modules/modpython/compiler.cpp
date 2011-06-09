/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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
