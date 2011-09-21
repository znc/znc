/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _MAIN_H
#define _MAIN_H

#include "zncconfig.h"

// The following defines are for #if comparison (preprocessor only likes ints)
#define VERSION_MAJOR  0
#define VERSION_MINOR  202
// This one is for display purpose
#define VERSION        (VERSION_MAJOR + VERSION_MINOR / 1000.0)

// You can add -DVERSION_EXTRA="stuff" to your CXXFLAGS!
#ifndef VERSION_EXTRA
#define VERSION_EXTRA ""
#endif

#define NOTHING (void)0

#define ALLMODULECALL(macFUNC, macEXITER)                                     \
	do {                                                                  \
		CGlobalModules& GMods = CZNC::Get().GetModules();             \
		if (GMods.macFUNC) {                                          \
			macEXITER;                                            \
		} else {                                                      \
			const map<CString, CUser*>& mUsers =                  \
				CZNC::Get().GetUserMap();                     \
			map<CString, CUser*>::const_iterator it;              \
			for (it = mUsers.begin(); it != mUsers.end(); ++it) { \
				CModules& UMods = it->second->GetModules();   \
				if (UMods.macFUNC) {                          \
					macEXITER;                            \
				}                                             \
			}                                                     \
		}                                                             \
	} while (false)

#define GLOBALMODULECALL(macFUNC, macUSER, macCLIENT, macEXITER)   \
	do {                                                       \
		CGlobalModules& GMods = CZNC::Get().GetModules();  \
		CUser* pOldGUser = GMods.GetUser();                \
		CClient* pOldGClient = GMods.GetClient();          \
		GMods.SetUser(macUSER);                            \
		GMods.SetClient(macCLIENT);                        \
		if (GMods.macFUNC) {                               \
			GMods.SetUser(pOldGUser);                  \
			GMods.SetClient(pOldGClient);              \
			macEXITER;                                 \
		}                                                  \
		GMods.SetUser(pOldGUser);                          \
		GMods.SetClient(pOldGClient);                      \
	} while (false)

#define MODULECALL(macFUNC, macUSER, macCLIENT, macEXITER)                \
	if (macUSER) {                                                    \
		GLOBALMODULECALL(macFUNC, macUSER, macCLIENT, macEXITER); \
		CModules& UMods = macUSER->GetModules();                  \
		CClient* pOldUClient = UMods.GetClient();                 \
		UMods.SetClient(macCLIENT);                               \
		if (UMods.macFUNC) {                                      \
			UMods.SetClient(pOldUClient);                     \
			macEXITER;                                        \
		}                                                         \
		UMods.SetClient(pOldUClient);                             \
	}

/** @mainpage
 *  Welcome to the API documentation for ZNC.
 *
 *  To write your own module, you should start with writing a new class which
 *  inherits from CModule. Use #MODCONSTRUCTOR for the module's constructor and
 *  call #MODULEDEFS at the end of your source file.
 *  Congratulations, you just wrote your first module. <br>
 *  For global modules, the procedure is similar. Instead of CModule you inherit
 *  from CGlobalModule. The two macros are replaced by #GLOBALMODCONSTRUCTOR and
 *  #GLOBALMODULEDEFS.
 *
 *  If you want your module to actually do something, you should override some
 *  of the hooks from CModule. These are the functions whose names start with
 *  "On". They are called when the associated event happens.
 *
 *  Feel free to also look at existing modules.
 */

#endif // !_MAIN_H
