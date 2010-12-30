/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <Python.h>

int main(int argc, char** argv) {
	Py_Initialize();
	PyObject* pyModule = PyImport_ImportModule("py_compile");
	if (!pyModule) {
		PyErr_Print();
		Py_Finalize();
		return 1;
	}
	PyObject* pyFunc = PyObject_GetAttrString(pyModule, "compile");
	Py_CLEAR(pyModule);
	if (!pyFunc) {
		PyErr_Print();
		Py_Finalize();
		return 2;
	}
	PyObject* pyRes = PyObject_CallFunction(pyFunc, const_cast<char*>("ss"), argv[1], argv[2]);
	Py_CLEAR(pyFunc);
	if (!pyRes) {
		PyErr_Print();
		Py_Finalize();
		return 3;
	}
	Py_CLEAR(pyRes);
	Py_Finalize();
	return 0;
}
