/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _MAIN_H
#define _MAIN_H

// The following defines are for #if comparison (preprocessor only likes ints)
#define VERSION_MAJOR	0
#define VERSION_MINOR	78
// This one is for display purpose
#define VERSION		(VERSION_MAJOR + VERSION_MINOR / 1000.0)

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
		CUser* pOldGUser = GMods.GetUser();				\
		CClient* pOldGClient = GMods.GetClient();			\
		CClient* pOldUClient = UMods.GetClient();			\
		GMods.SetUser(macUSER);						\
		GMods.SetClient(macCLIENT);	\
		UMods.SetClient(macCLIENT);							\
		if (GMods.macFUNC || UMods.macFUNC) {				\
			GMods.SetUser(pOldGUser);				\
			GMods.SetClient(pOldGClient);		\
			UMods.SetClient(pOldUClient);							\
			macEXITER;										\
		}													\
		GMods.SetUser(pOldGUser);					\
		GMods.SetClient(pOldGClient);			\
		UMods.SetClient(pOldUClient);								\
	}
#else
#define MODULECALL(macFUNC, macUSER, macCLIENT, macEXITER)
#endif

#endif // !_MAIN_H
