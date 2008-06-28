/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _MAIN_H
#define _MAIN_H

// Keep the number in sync with configure.in (and also with configure)
#define VERSION 0.057

// You can add -DVERSION_EXTRA="stuff" to your CXXFLAGS!
#ifndef VERSION_EXTRA
#define VERSION_EXTRA ""
#endif

#ifndef _MODDIR_
#define _MODDIR_ "/usr/lib/znc"
#endif

#ifndef _DATADIR_
#define _DATADIR_ "/usr/share/znc"
#endif

#ifdef _MODULES
#define MODULECALL(macFUNC, macUSER, macCLIENT, macEXITER)	\
	if (macUSER) {											\
		CGlobalModules& GMods = CZNC::Get().GetModules();	\
		CModules& UMods = macUSER->GetModules();			\
		GMods.SetUser(macUSER); GMods.SetClient(macCLIENT);	\
		UMods.SetClient(macCLIENT);							\
		if (GMods.macFUNC || UMods.macFUNC) {				\
			GMods.SetUser(NULL); GMods.SetClient(NULL);		\
			UMods.SetClient(NULL);							\
			macEXITER;										\
		}													\
		GMods.SetUser(NULL); GMods.SetClient(NULL);			\
		UMods.SetClient(NULL);								\
	}
#else
#define MODULECALL(macFUNC, macUSER, macCLIENT, macEXITER)
#endif


#ifndef CS_STRING
#define CS_STRING CString
#endif

#ifndef _NO_CSOCKET_NS
#define _NO_CSOCKET_NS
#endif

#ifdef _DEBUG
#define __DEBUG__
#endif

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX	1024
#warning PATH_MAX is undefined
#endif

#include "String.h"

#endif // !_MAIN_H
