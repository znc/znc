/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_MAIN_H
#define ZNC_MAIN_H

#include <znc/zncconfig.h>
#include <znc/version.h>

extern bool ZNC_NO_NEED_TO_DO_ANYTHING_ON_MODULE_CALL_EXITER;
#define NOTHING &ZNC_NO_NEED_TO_DO_ANYTHING_ON_MODULE_CALL_EXITER

#define ALLMODULECALL(macFUNC, macEXITER)                                      \
    do {                                                                       \
        CModules& GMods = CZNC::Get().GetModules();                            \
        bool bAllExit = false;                                                 \
        if (GMods.macFUNC) {                                                   \
            bAllExit = true;                                                   \
        } else {                                                               \
            const map<CString, CUser*>& mUsers = CZNC::Get().GetUserMap();     \
            map<CString, CUser*>::const_iterator it;                           \
            for (it = mUsers.begin(); it != mUsers.end(); ++it) {              \
                CModules& UMods = it->second->GetModules();                    \
                if (UMods.macFUNC) {                                           \
                    bAllExit = true;                                           \
                    break;                                                     \
                }                                                              \
                const vector<CIRCNetwork*>& mNets = it->second->GetNetworks(); \
                vector<CIRCNetwork*>::const_iterator it2;                      \
                for (it2 = mNets.begin(); it2 != mNets.end(); ++it2) {         \
                    CModules& NMods = (*it2)->GetModules();                    \
                    if (NMods.macFUNC) {                                       \
                        bAllExit = true;                                       \
                        break;                                                 \
                    }                                                          \
                }                                                              \
                if (bAllExit) break;                                           \
            }                                                                  \
        }                                                                      \
        if (bAllExit) *macEXITER = true;                                       \
    } while (false)

#define _GLOBALMODULECALL(macFUNC, macUSER, macNETWORK, macCLIENT, macEXITER) \
    do {                                                                      \
        CModules& GMods = CZNC::Get().GetModules();                           \
        CTemporaryModsUser TempUser(GMods, macUSER);                          \
        CTemporaryModsNetwork TempNetwork(GMods, macNETWORK);                 \
        CTemporaryModsClient TempClient(GMods, macCLIENT);                    \
        if (GMods.macFUNC) {                                                  \
            *macEXITER = true;                                                \
        }                                                                     \
    } while (false)

#define _USERMODULECALL(macFUNC, macUSER, macNETWORK, macCLIENT, macEXITER) \
    do {                                                                    \
        bool bGlobalExited = false;                                         \
        _GLOBALMODULECALL(macFUNC, macUSER, macNETWORK, macCLIENT,          \
                          &bGlobalExited);                                  \
        if (bGlobalExited) {                                                \
            *macEXITER = true;                                              \
            break;                                                          \
        }                                                                   \
        if (macUSER != nullptr) {                                           \
            CModules& UMods = macUSER->GetModules();                        \
            CTemporaryModsNetwork TempNetwork(UMods, macNETWORK);           \
            CTemporaryModsClient TempClient(UMods, macCLIENT);              \
            if (UMods.macFUNC) {                                            \
                *macEXITER = true;                                          \
            }                                                               \
        }                                                                   \
    } while (false)

#define NETWORKMODULECALL(macFUNC, macUSER, macNETWORK, macCLIENT, macEXITER) \
    do {                                                                      \
        bool bUserExited = false;                                             \
        _USERMODULECALL(macFUNC, macUSER, macNETWORK, macCLIENT,              \
                        &bUserExited);                                        \
        if (bUserExited) {                                                    \
            *macEXITER = true;                                                \
            break;                                                            \
        }                                                                     \
        if (macNETWORK != nullptr) {                                          \
            CModules& NMods = macNETWORK->GetModules();                       \
            CTemporaryModsClient TempClient(NMods, macCLIENT);                \
            if (NMods.macFUNC) {                                              \
                *macEXITER = true;                                            \
            }                                                                 \
        }                                                                     \
    } while (false)

#define GLOBALMODULECALL(macFUNC, macEXITER) \
    _GLOBALMODULECALL(macFUNC, nullptr, nullptr, nullptr, macEXITER)

#define USERMODULECALL(macFUNC, macUSER, macCLIENT, macEXITER) \
    _USERMODULECALL(macFUNC, macUSER, nullptr, macCLIENT, macEXITER)

/** @mainpage
 *  Welcome to the API documentation for ZNC.
 *
 *  To write your own module, you should start with writing a new class which
 *  inherits from CModule. Use #MODCONSTRUCTOR for the module's constructor and
 *  call #MODULEDEFS at the end of your source file.
 *  Congratulations, you just wrote your first module. <br>
 *  For global modules, the procedure is similar. Instead of #MODULEDEFS call
 *  #GLOBALMODULEDEFS.
 *
 *  If you want your module to actually do something, you should override some
 *  of the hooks from CModule. These are the functions whose names start with
 *  "On". They are called when the associated event happens.
 *
 *  Feel free to also look at existing modules.
 */

#endif  // !ZNC_MAIN_H
