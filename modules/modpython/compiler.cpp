/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <Python.h>

int main() {
	Py_Initialize();
	int res = PyRun_SimpleString("import compileall; print('Optimizing python files for later use...'); compileall.compile_dir('.')");
	Py_Finalize();
	return res;
}
