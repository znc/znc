/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <znc/Chan.h>
#include <znc/FileUtils.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/User.h>
#include <znc/znc.h>

#include "modpython/swigpyrun.h"
#include "modpython/module.h"
#include "modpython/ret.h"

using std::vector;
using std::set;

class CModPython : public CModule {
    PyObject* m_PyZNCModule;
    PyObject* m_PyFormatException;
    vector<PyObject*> m_vpObject;

  public:
    CString GetPyExceptionStr() {
        PyObject* ptype;
        PyObject* pvalue;
        PyObject* ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        CString result;
        if (!pvalue) {
            Py_INCREF(Py_None);
            pvalue = Py_None;
        }
        if (!ptraceback) {
            Py_INCREF(Py_None);
            ptraceback = Py_None;
        }
        PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
        PyObject* strlist = PyObject_CallFunctionObjArgs(
            m_PyFormatException, ptype, pvalue, ptraceback, nullptr);
        Py_CLEAR(ptype);
        Py_CLEAR(pvalue);
        Py_CLEAR(ptraceback);
        if (!strlist) {
            return "Couldn't get exact error message";
        }

        if (PySequence_Check(strlist)) {
            PyObject* strlist_fast =
                PySequence_Fast(strlist, "Shouldn't happen (1)");
            PyObject** items = PySequence_Fast_ITEMS(strlist_fast);
            Py_ssize_t L = PySequence_Fast_GET_SIZE(strlist_fast);
            for (Py_ssize_t i = 0; i < L; ++i) {
                PyObject* utf8 = PyUnicode_AsUTF8String(items[i]);
                result += PyBytes_AsString(utf8);
                Py_CLEAR(utf8);
            }
            Py_CLEAR(strlist_fast);
        } else {
            result = "Can't get exact error message";
        }

        Py_CLEAR(strlist);

        return result;
    }

    MODCONSTRUCTOR(CModPython) {
        CZNC::Get().ForceEncoding();
        Py_Initialize();
        m_PyFormatException = nullptr;
        m_PyZNCModule = nullptr;
    }

    bool OnLoad(const CString& sArgsi, CString& sMessage) override {
        CString sModPath, sTmp;
#ifdef __CYGWIN__
        CString sDllPath = "modpython/_znc_core.dll";
#else
        CString sDllPath = "modpython/_znc_core.so";
#endif
        if (!CModules::FindModPath(sDllPath, sModPath, sTmp)) {
            sMessage = sDllPath + " not found.";
            return false;
        }
        sTmp = CDir::ChangeDir(sModPath, "..");

        PyObject* pyModuleTraceback = PyImport_ImportModule("traceback");
        if (!pyModuleTraceback) {
            sMessage = "Couldn't import python module traceback";
            return false;
        }
        m_PyFormatException =
            PyObject_GetAttrString(pyModuleTraceback, "format_exception");
        if (!m_PyFormatException) {
            sMessage = "Couldn't get traceback.format_exception";
            Py_CLEAR(pyModuleTraceback);
            return false;
        }
        Py_CLEAR(pyModuleTraceback);

        PyObject* pySysModule = PyImport_ImportModule("sys");
        if (!pySysModule) {
            sMessage = GetPyExceptionStr();
            return false;
        }
        PyObject* pySysPath = PyObject_GetAttrString(pySysModule, "path");
        if (!pySysPath) {
            sMessage = GetPyExceptionStr();
            Py_CLEAR(pySysModule);
            return false;
        }
        Py_CLEAR(pySysModule);
        PyObject* pyIgnored =
            PyObject_CallMethod(pySysPath, const_cast<char*>("append"),
                                const_cast<char*>("s"), sTmp.c_str());
        if (!pyIgnored) {
            sMessage = GetPyExceptionStr();
            Py_CLEAR(pyIgnored);
            return false;
        }
        Py_CLEAR(pyIgnored);
        Py_CLEAR(pySysPath);

        m_PyZNCModule = PyImport_ImportModule("znc");
        if (!m_PyZNCModule) {
            sMessage = GetPyExceptionStr();
            return false;
        }

        return true;
    }

    EModRet OnModuleLoading(const CString& sModName, const CString& sArgs,
                            CModInfo::EModuleType eType, bool& bSuccess,
                            CString& sRetMsg) override {
        PyObject* pyFunc = PyObject_GetAttrString(m_PyZNCModule, "load_module");
        if (!pyFunc) {
            sRetMsg = GetPyExceptionStr();
            DEBUG("modpython: " << sRetMsg);
            bSuccess = false;
            return HALT;
        }
        PyObject* pyRes = PyObject_CallFunction(
            pyFunc, const_cast<char*>("ssiNNNN"), sModName.c_str(),
            sArgs.c_str(), (int)eType,
            (eType == CModInfo::GlobalModule
                 ? Py_None
                 : SWIG_NewInstanceObj(GetUser(), SWIG_TypeQuery("CUser*"), 0)),
            (eType == CModInfo::NetworkModule
                 ? SWIG_NewInstanceObj(GetNetwork(),
                                       SWIG_TypeQuery("CIRCNetwork*"), 0)
                 : Py_None),
            CPyRetString::wrap(sRetMsg),
            SWIG_NewInstanceObj(reinterpret_cast<CModule*>(this),
                                SWIG_TypeQuery("CModPython*"), 0));
        if (!pyRes) {
            sRetMsg = GetPyExceptionStr();
            DEBUG("modpython: " << sRetMsg);
            bSuccess = false;
            Py_CLEAR(pyFunc);
            return HALT;
        }
        Py_CLEAR(pyFunc);
        long int ret = PyLong_AsLong(pyRes);
        if (PyErr_Occurred()) {
            sRetMsg = GetPyExceptionStr();
            DEBUG("modpython: " << sRetMsg);
            Py_CLEAR(pyRes);
            return HALT;
        }
        Py_CLEAR(pyRes);
        switch (ret) {
            case 0:
                // Not found
                return CONTINUE;
            case 1:
                // Error
                bSuccess = false;
                return HALT;
            case 2:
                // Success
                bSuccess = true;
                return HALT;
        }
        bSuccess = false;
        sRetMsg += " unknown value returned by modpython.load_module";
        return HALT;
    }

    EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess,
                              CString& sRetMsg) override {
        CPyModule* pMod = AsPyModule(pModule);
        if (pMod) {
            CString sModName = pMod->GetModName();
            PyObject* pyFunc =
                PyObject_GetAttrString(m_PyZNCModule, "unload_module");
            if (!pyFunc) {
                sRetMsg = GetPyExceptionStr();
                DEBUG("modpython: " << sRetMsg);
                bSuccess = false;
                return HALT;
            }
            PyObject* pyRes =
                PyObject_CallFunctionObjArgs(pyFunc, pMod->GetPyObj(), nullptr);
            if (!pyRes) {
                sRetMsg = GetPyExceptionStr();
                DEBUG("modpython: " << sRetMsg);
                bSuccess = false;
                Py_CLEAR(pyFunc);
                return HALT;
            }
            if (!PyObject_IsTrue(pyRes)) {
                // python module, but not handled by modpython itself.
                // some module-provider written on python loaded it?
                return CONTINUE;
            }
            Py_CLEAR(pyFunc);
            Py_CLEAR(pyRes);
            bSuccess = true;
            sRetMsg = "Module [" + sModName + "] unloaded";
            return HALT;
        }
        return CONTINUE;
    }

    EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
                         bool& bSuccess, CString& sRetMsg) override {
        PyObject* pyFunc =
            PyObject_GetAttrString(m_PyZNCModule, "get_mod_info");
        if (!pyFunc) {
            sRetMsg = GetPyExceptionStr();
            DEBUG("modpython: " << sRetMsg);
            bSuccess = false;
            return HALT;
        }
        PyObject* pyRes = PyObject_CallFunction(
            pyFunc, const_cast<char*>("sNN"), sModule.c_str(),
            CPyRetString::wrap(sRetMsg),
            SWIG_NewInstanceObj(&ModInfo, SWIG_TypeQuery("CModInfo*"), 0));
        if (!pyRes) {
            sRetMsg = GetPyExceptionStr();
            DEBUG("modpython: " << sRetMsg);
            bSuccess = false;
            Py_CLEAR(pyFunc);
            return HALT;
        }
        Py_CLEAR(pyFunc);
        long int x = PyLong_AsLong(pyRes);
        if (PyErr_Occurred()) {
            sRetMsg = GetPyExceptionStr();
            DEBUG("modpython: " << sRetMsg);
            bSuccess = false;
            Py_CLEAR(pyRes);
            return HALT;
        }
        Py_CLEAR(pyRes);
        switch (x) {
            case 0:
                return CONTINUE;
            case 1:
                bSuccess = false;
                return HALT;
            case 2:
                bSuccess = true;
                return HALT;
        }
        bSuccess = false;
        sRetMsg = CString("Shouldn't happen. ") + __PRETTY_FUNCTION__ + " on " +
                  __FILE__ + ":" + CString(__LINE__);
        DEBUG(sRetMsg);
        return HALT;
    }

    void TryAddModInfo(const CString& sName,
                       set<CModInfo>& ssMods, set<CString>& ssAlready,
                       CModInfo::EModuleType eType) {
        if (ssAlready.count(sName)) {
            return;
        }
        CModInfo ModInfo;
        bool bSuccess = false;
        CString sRetMsg;
        OnGetModInfo(ModInfo, sName, bSuccess, sRetMsg);
        if (bSuccess && ModInfo.SupportsType(eType)) {
            ssMods.insert(ModInfo);
            ssAlready.insert(sName);
        }
    }

    void OnGetAvailableMods(set<CModInfo>& ssMods,
                            CModInfo::EModuleType eType) override {
        CDir Dir;
        CModules::ModDirList dirs = CModules::GetModDirs();

        while (!dirs.empty()) {
            set<CString> already;

            Dir.Fill(dirs.front().first);
            for (unsigned int a = 0; a < Dir.size(); a++) {
                CFile& File = *Dir[a];
                CString sName = File.GetShortName();

                if (!File.IsDir()) {
                    if (sName.WildCmp("*.pyc")) {
                        sName.RightChomp(4);
                    } else if (sName.WildCmp("*.py")) {
                        sName.RightChomp(3);
                    } else {
                        continue;
                    }
                }

                TryAddModInfo(sName, ssMods, already, eType);
            }

            dirs.pop();
        }
    }

    ~CModPython() override {
        if (!m_PyZNCModule) {
            DEBUG(
                "~CModPython(): seems like CModPython::OnLoad() didn't "
                "initialize python");
            return;
        }
        PyObject* pyFunc = PyObject_GetAttrString(m_PyZNCModule, "unload_all");
        if (!pyFunc) {
            CString sRetMsg = GetPyExceptionStr();
            DEBUG("~CModPython(): couldn't find unload_all: " << sRetMsg);
            return;
        }
        PyObject* pyRes = PyObject_CallFunctionObjArgs(pyFunc, nullptr);
        if (!pyRes) {
            CString sRetMsg = GetPyExceptionStr();
            DEBUG(
                "modpython tried to unload all modules in its destructor, but: "
                << sRetMsg);
        }
        Py_CLEAR(pyRes);
        Py_CLEAR(pyFunc);

        Py_CLEAR(m_PyFormatException);
        Py_CLEAR(m_PyZNCModule);
        Py_Finalize();
        CZNC::Get().UnforceEncoding();
    }
};

CString CPyModule::GetPyExceptionStr() {
    return m_pModPython->GetPyExceptionStr();
}

#include "modpython/pyfunctions.cpp"

VWebSubPages& CPyModule::GetSubPages() {
    VWebSubPages* result = _GetSubPages();
    if (!result) {
        return CModule::GetSubPages();
    }
    return *result;
}

void CPyTimer::RunJob() {
    CPyModule* pMod = AsPyModule(GetModule());
    if (pMod) {
        PyObject* pyRes = PyObject_CallMethod(
            m_pyObj, const_cast<char*>("RunJob"), const_cast<char*>(""));
        if (!pyRes) {
            CString sRetMsg = m_pModPython->GetPyExceptionStr();
            DEBUG("python timer failed: " << sRetMsg);
            Stop();
        }
        Py_CLEAR(pyRes);
    }
}

CPyTimer::~CPyTimer() {
    CPyModule* pMod = AsPyModule(GetModule());
    if (pMod) {
        PyObject* pyRes = PyObject_CallMethod(
            m_pyObj, const_cast<char*>("OnShutdown"), const_cast<char*>(""));
        if (!pyRes) {
            CString sRetMsg = m_pModPython->GetPyExceptionStr();
            DEBUG("python timer shutdown failed: " << sRetMsg);
        }
        Py_CLEAR(pyRes);
        Py_CLEAR(m_pyObj);
    }
}

#define CHECKCLEARSOCK(Func)                                    \
    if (!pyRes) {                                               \
        CString sRetMsg = m_pModPython->GetPyExceptionStr();    \
        DEBUG("python socket failed in " Func ": " << sRetMsg); \
        Close();                                                \
    }                                                           \
    Py_CLEAR(pyRes)

#define CBSOCK(Func)                                                        \
    void CPySocket::Func() {                                                \
        PyObject* pyRes = PyObject_CallMethod(                              \
            m_pyObj, const_cast<char*>("On" #Func), const_cast<char*>("")); \
        CHECKCLEARSOCK(#Func);                                              \
    }
CBSOCK(Connected);
CBSOCK(Disconnected);
CBSOCK(Timeout);
CBSOCK(ConnectionRefused);

void CPySocket::ReadData(const char* data, size_t len) {
    PyObject* pyRes =
        PyObject_CallMethod(m_pyObj, const_cast<char*>("OnReadData"),
                            const_cast<char*>("y#"), data, (Py_ssize_t)len);
    CHECKCLEARSOCK("OnReadData");
}

void CPySocket::ReadLine(const CString& sLine) {
    PyObject* pyRes =
        PyObject_CallMethod(m_pyObj, const_cast<char*>("OnReadLine"),
                            const_cast<char*>("s"), sLine.c_str());
    CHECKCLEARSOCK("OnReadLine");
}

Csock* CPySocket::GetSockObj(const CString& sHost, unsigned short uPort) {
    CPySocket* result = nullptr;
    PyObject* pyRes =
        PyObject_CallMethod(m_pyObj, const_cast<char*>("_Accepted"),
                            const_cast<char*>("sH"), sHost.c_str(), uPort);
    if (!pyRes) {
        CString sRetMsg = m_pModPython->GetPyExceptionStr();
        DEBUG("python socket failed in OnAccepted: " << sRetMsg);
        Close();
    }
    int res = SWIG_ConvertPtr(pyRes, (void**)&result,
                              SWIG_TypeQuery("CPySocket*"), 0);
    if (!SWIG_IsOK(res)) {
        DEBUG(
            "python socket was expected to return new socket from OnAccepted, "
            "but error="
            << res);
        Close();
        result = nullptr;
    }
    if (!result) {
        DEBUG("modpython: OnAccepted didn't return new socket");
    }
    Py_CLEAR(pyRes);
    return result;
}

CPySocket::~CPySocket() {
    PyObject* pyRes = PyObject_CallMethod(
        m_pyObj, const_cast<char*>("OnShutdown"), const_cast<char*>(""));
    if (!pyRes) {
        CString sRetMsg = m_pModPython->GetPyExceptionStr();
        DEBUG("python socket failed in OnShutdown: " << sRetMsg);
    }
    Py_CLEAR(pyRes);
    Py_CLEAR(m_pyObj);
}

CPyCapability::CPyCapability(PyObject* serverCb, PyObject* clientCb)
    : m_serverCb(serverCb), m_clientCb(clientCb) {
    Py_INCREF(serverCb);
    Py_INCREF(clientCb);
}

CPyCapability::~CPyCapability() {
    Py_CLEAR(m_serverCb);
    Py_CLEAR(m_clientCb);
}

void CPyCapability::OnServerChangedSupport(CIRCNetwork* pNetwork, bool bState) {
    PyObject* pyArg_Network =
        SWIG_NewInstanceObj(pNetwork, SWIG_TypeQuery("CIRCNetwork*"), 0);
    PyObject* pyArg_bState = Py_BuildValue("l", (long int)bState);
    PyObject* pyRes = PyObject_CallFunctionObjArgs(m_serverCb, pyArg_Network,
                                                   pyArg_bState, nullptr);
    if (!pyRes) {
        CString sPyErr = ((CPyModule*)GetModule())->GetPyExceptionStr();
        DEBUG("modpython: " << GetModule()->GetModName()
                            << "/OnServerChangedSupport failed: " << sPyErr);
    }
    Py_CLEAR(pyRes);
    Py_CLEAR(pyArg_bState);
    Py_CLEAR(pyArg_Network);
}

void CPyCapability::OnClientChangedSupport(CClient* pClient, bool bState) {
    PyObject* pyArg_Client =
        SWIG_NewInstanceObj(pClient, SWIG_TypeQuery("CClient*"), 0);
    PyObject* pyArg_bState = Py_BuildValue("l", (long int)bState);
    PyObject* pyRes = PyObject_CallFunctionObjArgs(m_clientCb, pyArg_Client,
                                                   pyArg_bState, nullptr);
    if (!pyRes) {
        CString sPyErr = ((CPyModule*)GetModule())->GetPyExceptionStr();
        DEBUG("modpython: " << GetModule()->GetModName()
                            << "/OnClientChangedSupport failed: " << sPyErr);
    }
    Py_CLEAR(pyRes);
    Py_CLEAR(pyArg_bState);
    Py_CLEAR(pyArg_Client);
}

CPyModule* CPyModCommand::GetModule() {
    return this->m_pModule;
}

void CPyModCommand::operator()(const CString& sLine) {
    PyObject* pyRes = PyObject_CallMethod(
        m_pyObj, const_cast<char*>("__call__"), const_cast<char*>("s"),
        sLine.c_str());
    if (!pyRes) {
        CString sRetMsg = m_pModPython->GetPyExceptionStr();
        DEBUG("oops, something went wrong when calling command: " << sRetMsg);
    }
    Py_CLEAR(pyRes);
}

CPyModCommand::~CPyModCommand() {
    Py_CLEAR(m_pyObj);
}

template <>
void TModInfo<CModPython>(CModInfo& Info) {
    Info.SetWikiPage("modpython");
}

GLOBALMODULEDEFS(CModPython, t_s("Loads python scripts as ZNC modules"))
