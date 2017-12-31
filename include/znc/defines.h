/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ZNC_DEFINES_H
#define ZNC_DEFINES_H

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
#define CS_DEBUG(f) DEBUG(__FILE__ << ":" << __LINE__ << " " << f)
#define PERROR(f)                                         \
    DEBUG(__FILE__ << ":" << __LINE__ << " " << f << ": " \
                   << strerror(GetSockError()))

#endif  // !ZNC_DEFINES_H
