//! @author prozac@rottenboy.com

#ifndef _MAIN_H
#define _MAIN_H

// Keep the number in sync with configure.in (and also with configure)
#define VERSION 0.048

#ifndef _MODDIR_
#define _MODDIR_ "/usr/libexec/znc"
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

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#include "String.h"
#include "Csocket.h"
#include "Utils.h"

#endif // !_MAIN_H
