/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _DEFINES_H
#define _DEFINES_H

#include <znc/zncconfig.h>

// This header file is just for Csocket

#include <znc/ZNCDebug.h>
#include <znc/ZNCString.h>

#define CS_STRING CString
#define _NO_CSOCKET_NS

#ifdef _DEBUG
#define __DEBUG__
#endif

// Redefine some Csocket debugging mechanisms to use ZNC's
#define CS_DEBUG(f)  DEBUG(__FILE__ << ":" << __LINE__ << " " << f)
#define PERROR(f)    DEBUG(__FILE__ << ":" << __LINE__ << " " << f << ": " << strerror(GetSockError()))


#endif // !_DEFINES_H
