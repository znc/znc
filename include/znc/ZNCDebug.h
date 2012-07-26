/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef ZNCDEBUG_H
#define ZNCDEBUG_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <iostream>
#include <ctime>
#include <sys/time.h>
#include <sstream>
#include <iomanip>

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
		std::cout << CDebug::GetTimestamp() << f << std::endl; \
	} \
} while (0)

class CDebug {
public:
	static void SetStdoutIsTTY(bool b) { stdoutIsTTY = b; }
	static bool StdoutIsTTY() { return stdoutIsTTY; }
	static void SetDebug(bool b) { debug = b; }
	static bool Debug() { return debug; }
	static CString GetTimestamp() {
		char buf[64];
		timeval time_now;
		gettimeofday(&time_now, NULL);
		time_t currentSec = (time_t) time_now.tv_sec; // cast from long int
		tm* time_info = localtime(&currentSec);
		strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S.", time_info);
		std::ostringstream buffer;
		buffer << buf << std::setw(6) << std::setfill('0') << (time_now.tv_usec) << "] ";
		return buffer.str();
	}

protected:
	static bool stdoutIsTTY;
	static bool debug;
};

#endif // !ZNCDEBUG_H
