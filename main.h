/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _MAIN_H
#define _MAIN_H

// The following defines are for #if comparison (preprocessor only likes ints)
#define VERSION_MAJOR	0
#define VERSION_MINOR	89
// This one is for display purpose
#define VERSION		(VERSION_MAJOR + VERSION_MINOR / 1000.0)

// You can add -DVERSION_EXTRA="stuff" to your CXXFLAGS!
#ifndef VERSION_EXTRA
#define VERSION_EXTRA ""
#endif

#ifndef _MODDIR_
#define _MODDIR_ "/usr/lib/znc"
#endif

#ifndef _SKINDIR_
#define _SKINDIR_ _MODDIR_ "/webskins"
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
 *  "Do". They are called when the associated event happens.
 *
 *  Feel free to also look at existing modules.
 */

#endif // !_MAIN_H
