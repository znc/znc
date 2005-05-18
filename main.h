#ifndef _MAIN_H
#define _MAIN_H

#ifndef _MODDIR_
#define _MODDIR_ "/usr/share/znc"
#endif

#define VERSION 0.036

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

#include "String.h"
#include "Csocket.h"
#include "Utils.h"

#endif // !_MAIN_H
