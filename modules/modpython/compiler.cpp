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
	int res = PyRun_SimpleString(
			"import compileall\n"
			"print('Optimizing python files for later use...')\n"
			"import sys\n"
			"if sys.version_info < (3, 2):\n"
			"    compileall.compile_dir('.')\n"
			"else:\n"
			"    compileall.compile_dir('.', legacy=True)\n"
			);
	Py_Finalize();
	return res;
}
