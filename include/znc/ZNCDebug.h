/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef ZNCDEBUG_H
#define ZNCDEBUG_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <sstream>

/** Output a debug info if debugging is enabled.
 *  If ZNC was compiled with <code>--enable-debug</code> or was started with
 *  <code>--debug</code>, the given argument will be sent to stdout.
 *
 *  You can use all the features of C++ streams:
 *  @code
 *  DEBUG("I had " << errors << " errors");
 *  @endcode
 *
 *  @param f The expression you want to display.
 */
#define DEBUG(f) do { \
	if (CDebug::Debug()) { \
		CDebugStream sDebug;\
		sDebug << f;\
	} \
} while (0)

class CDebug {
public:
	static void SetStdoutIsTTY(bool b) { stdoutIsTTY = b; }
	static bool StdoutIsTTY() { return stdoutIsTTY; }
	static void SetDebug(bool b) { debug = b; }
	static bool Debug() { return debug; }

protected:
	static bool stdoutIsTTY;
	static bool debug;
};

class CDebugStream : public std::ostringstream {
public:
	~CDebugStream();
};

#endif // !ZNCDEBUG_H
