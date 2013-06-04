/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/ZNCDebug.h>
#include <iostream>
#include <sys/time.h>
#include <stdio.h>

bool CDebug::stdoutIsTTY = true;
bool CDebug::debug =
#ifdef _DEBUG
		true;
#else
		false;
#endif

CDebugStream::~CDebugStream() {
	timeval tTime;
	gettimeofday(&tTime, NULL);
	time_t tSec = (time_t)tTime.tv_sec; // some systems (e.g. openbsd) define tv_sec as long int instead of time_t
	tm tM;
	tzset();// localtime_r requires this
	localtime_r(&tSec, &tM);
	char sTime[20] = {};
	strftime(sTime, sizeof(sTime), "%Y-%m-%d %H:%M:%S", &tM);
	char sUsec[7] = {};
	snprintf(sUsec, sizeof(sUsec), "%06lu", (unsigned long int)tTime.tv_usec);
	std::cout << "[" << sTime << "." << sUsec << "] " << CString(this->str()).Escape_n(CString::EDEBUG) << std::endl;
}
