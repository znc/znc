/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <Python.h>

#include "Chan.h"
#include "FileUtils.h"
#include "IRCSock.h"
#include "Modules.h"
#include "Nick.h"
#include "User.h"
#include "znc.h"

#include "modpython/swigpyrun.h"
#include "modpython/module.h"
#include "modpython/retstring.h"

class CModPython: public CGlobalModule {

	PyObject* m_PyZNCModule;
	PyObject* m_PyFormatException;

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
			PyObject* strlist = PyObject_CallFunctionObjArgs(m_PyFormatException, ptype, pvalue, ptraceback, NULL);
			Py_CLEAR(ptype);
			Py_CLEAR(pvalue);
			Py_CLEAR(ptraceback);
			if (!strlist) {
				return "Couldn't get exact error message";
			}

			if (PySequence_Check(strlist)) {
				PyObject* strlist_fast = PySequence_Fast(strlist, "Shouldn't happen (1)");
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

	GLOBALMODCONSTRUCTOR(CModPython) {
		Py_Initialize();
		m_PyFormatException = NULL;
		m_PyZNCModule = NULL;
	}

	bool OnLoad(const CString& sArgsi, CString& sMessage) {
		CString sModPath, sTmp;
		if (!CModules::FindModPath("modpython/_znc_core.so", sModPath, sTmp)) {
			sMessage = "modpython/_znc_core.so not found.";
			return false;
		}
		sTmp = CDir::ChangeDir(sModPath, "..");

		PyObject* pyModuleTraceback = PyImport_ImportModule("traceback");
		if (!pyModuleTraceback) {
			sMessage = "Couldn't import python module traceback";
			return false;
		}
		m_PyFormatException = PyObject_GetAttrString(pyModuleTraceback, "format_exception");
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
		PyObject* pyIgnored = PyObject_CallMethod(pySysPath, const_cast<char*>("append"), const_cast<char*>("s"), sTmp.c_str());
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

	virtual EModRet OnModuleLoading(const CString& sModName, const CString& sArgs,
			bool& bSuccess, CString& sRetMsg) {
		if (!GetUser()) {
			return CONTINUE;
		}
		PyObject* pyFunc = PyObject_GetAttrString(m_PyZNCModule, "load_module");
		if (!pyFunc) {
			sRetMsg = GetPyExceptionStr();
			DEBUG("modpython: " << sRetMsg);
			bSuccess = false;
			return HALT;
		}
		PyObject* pyRes = PyObject_CallFunction(pyFunc, const_cast<char*>("ssNNN"),
				sModName.c_str(),
				sArgs.c_str(),
				SWIG_NewInstanceObj(GetUser(), SWIG_TypeQuery("CUser*"), 0),
				CPyRetString::wrap(sRetMsg),
				SWIG_NewInstanceObj(reinterpret_cast<CGlobalModule*>(this), SWIG_TypeQuery("CGlobalModule*"), 0));
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
		sRetMsg += " unknown value returned by modperl.load_module";
		return HALT;
	}

	virtual EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg) {
		CPyModule* pMod = AsPyModule(pModule);
		if (pMod) {
			CString sModName = pMod->GetModName();
			PyObject* pyFunc = PyObject_GetAttrString(m_PyZNCModule, "unload_module");
			if (!pyFunc) {
				sRetMsg = GetPyExceptionStr();
				DEBUG("modpython: " << sRetMsg);
				bSuccess = false;
				return HALT;
			}
			PyObject* pyRes = PyObject_CallFunctionObjArgs(pyFunc, pMod->GetPyObj(), NULL);
			if (!pyRes) {
				sRetMsg = GetPyExceptionStr();
				DEBUG("modpython: " << sRetMsg);
				bSuccess = false;
				Py_CLEAR(pyFunc);
				return HALT;
			}
			Py_CLEAR(pyFunc);
			Py_CLEAR(pyRes);
			bSuccess = true;
			sRetMsg = "Module [" + sModName + "] unloaded";
			return HALT;
		}
		return CONTINUE;
	}

	virtual EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
			bool& bSuccess, CString& sRetMsg) {
		PyObject* pyFunc = PyObject_GetAttrString(m_PyZNCModule, "get_mod_info");
		if (!pyFunc) {
			sRetMsg = GetPyExceptionStr();
			DEBUG("modpython: " << sRetMsg);
			bSuccess = false;
			return HALT;
		}
		PyObject* pyRes = PyObject_CallFunction(pyFunc, const_cast<char*>("sNN"),
				sModule.c_str(),
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
		sRetMsg = CString("Shouldn't happen. ") + __PRETTY_FUNCTION__ + " on " + __FILE__ + ":" + CString(__LINE__);
		DEBUG(sRetMsg);
		return HALT;
	}

	void TryAddModInfo(const CString& sPath, const CString& sName, set<CModInfo>& ssMods, set<CString>& ssAlready) {
		if (ssAlready.count(sName)) {
			return;
		}
		PyObject* pyFunc = PyObject_GetAttrString(m_PyZNCModule, "get_mod_info_path");
		if (!pyFunc) {
			CString sRetMsg = GetPyExceptionStr();
			DEBUG("modpython tried to get info about [" << sPath << "] (1) but: " << sRetMsg);
			return;
		}
		CModInfo ModInfo;
		PyObject* pyRes = PyObject_CallFunction(pyFunc, const_cast<char*>("ssN"),
				sPath.c_str(),
				sName.c_str(),
				SWIG_NewInstanceObj(&ModInfo, SWIG_TypeQuery("CModInfo*"), 0));
		if (!pyRes) {
			CString sRetMsg = GetPyExceptionStr();
			DEBUG("modpython tried to get info about [" << sPath << "] (2) but: " << sRetMsg);
			Py_CLEAR(pyFunc);
			return;
		}
		Py_CLEAR(pyFunc);
		long int x = PyLong_AsLong(pyRes);
		if (PyErr_Occurred()) {
			CString sRetMsg = GetPyExceptionStr();
			DEBUG("modpython tried to get info about [" << sPath << "] (3) but: " << sRetMsg);
			Py_CLEAR(pyRes);
			return;
		}
		Py_CLEAR(pyRes);
		if (x) {
			ssMods.insert(ModInfo);
			ssAlready.insert(sName);
		}
	}

	virtual void OnGetAvailableMods(set<CModInfo>& ssMods, bool bGlobal) {
		if (bGlobal) {
			return;
		}

		CDir Dir;
		CModules::ModDirList dirs = CModules::GetModDirs();

		while (!dirs.empty()) {
			set<CString> already;

			Dir.FillByWildcard(dirs.front().first, "*.py");
			for (unsigned int a = 0; a < Dir.size(); a++) {
				CFile& File = *Dir[a];
				CString sName = File.GetShortName();
				CString sPath = File.GetLongName();
				sPath.TrimSuffix(sName);
				sName.RightChomp(3);
				TryAddModInfo(sPath, sName, ssMods, already);
			}

			Dir.FillByWildcard(dirs.front().first, "*.pyc");
			for (unsigned int a = 0; a < Dir.size(); a++) {
				CFile& File = *Dir[a];
				CString sName = File.GetShortName();
				CString sPath = File.GetLongName();
				sPath.TrimSuffix(sName);
				sName.RightChomp(4);
				TryAddModInfo(sPath, sName, ssMods, already);
			}

			Dir.FillByWildcard(dirs.front().first, "*.so");
			for (unsigned int a = 0; a < Dir.size(); a++) {
				CFile& File = *Dir[a];
				CString sName = File.GetShortName();
				CString sPath = File.GetLongName();
				sPath.TrimSuffix(sName);
				sName.RightChomp(3);
				TryAddModInfo(sPath, sName, ssMods, already);
			}

			dirs.pop();
		}
	}

	virtual ~CModPython() {
		const map<CString, CUser*>& users = CZNC::Get().GetUserMap();
		for (map<CString, CUser*>::const_iterator i = users.begin(); i != users.end(); ++i) {
			CModules& M = i->second->GetModules();
			bool cont;
			do {
				cont = false;
				for (CModules::iterator it = M.begin(); it != M.end(); ++it) {
					CModule* m = *it;
					CPyModule* mod = AsPyModule(m);
					if (mod) {
						cont = true;
						bool bSuccess = false;
						CString sRetMsg;
						OnModuleUnloading(mod, bSuccess, sRetMsg);
						if (!bSuccess) {
							DEBUG("Error unloading python module in ~CModPython: " << sRetMsg);
						}
						break;
					}
				}
			} while (cont);
		}
		Py_CLEAR(m_PyFormatException);
		Py_CLEAR(m_PyZNCModule);
		Py_Finalize();
	}

};

CString CPyModule::GetPyExceptionStr() {
	return m_pModPython->GetPyExceptionStr();
}

#include "modpython/functions.cpp"

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
		PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("RunJob"), const_cast<char*>(""));
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
		PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("OnShutdown"), const_cast<char*>(""));
		if (!pyRes) {
			CString sRetMsg = m_pModPython->GetPyExceptionStr();
			DEBUG("python timer shutdown failed: " << sRetMsg);
		}
		Py_CLEAR(pyRes);
		Py_CLEAR(m_pyObj);
	}
}

#define CHECKCLEARSOCK(Func)\
if (!pyRes) {\
	CString sRetMsg = m_pModPython->GetPyExceptionStr();\
	DEBUG("python socket failed in " Func ": " << sRetMsg);\
	Close();\
}\
Py_CLEAR(pyRes)

#define CBSOCK(Func) void CPySocket::Func() {\
	PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("On" #Func), const_cast<char*>(""));\
	CHECKCLEARSOCK(#Func);\
}
CBSOCK(Connected);
CBSOCK(Disconnected);
CBSOCK(Timeout);
CBSOCK(ConnectionRefused);

void CPySocket::ReadData(const char *data, size_t len) {
	PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("OnReadData"), const_cast<char*>("y#"), data, (int)len);
	CHECKCLEARSOCK("OnReadData");
}

void CPySocket::ReadLine(const CString& sLine) {
	PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("OnReadLine"), const_cast<char*>("s"), sLine.c_str());
	CHECKCLEARSOCK("OnReadLine");
}

Csock* CPySocket::GetSockObj(const CString& sHost, unsigned short uPort) {
	CPySocket* result = NULL;
	PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("_Accepted"), const_cast<char*>("sH"), sHost.c_str(), uPort);
	if (!pyRes) {
		CString sRetMsg = m_pModPython->GetPyExceptionStr();
		DEBUG("python socket failed in OnAccepted: " << sRetMsg);
		Close();
	}
	int res = SWIG_ConvertPtr(pyRes, (void**)&result, SWIG_TypeQuery("CPySocket*"), 0);
	if (!SWIG_IsOK(res)) {
		DEBUG("python socket was expected to return new socket from OnAccepted, but error=" << res);
		Close();
		result = NULL;
	}
    if (!result) {
        DEBUG("modpython: OnAccepted didn't return new socket");
    }
	Py_CLEAR(pyRes);
	return result;
}

CPySocket::~CPySocket() {
	PyObject* pyRes = PyObject_CallMethod(m_pyObj, const_cast<char*>("OnShutdown"), const_cast<char*>(""));
	if (!pyRes) {
		CString sRetMsg = m_pModPython->GetPyExceptionStr();
		DEBUG("python socket failed in OnShutdown: " << sRetMsg);
	}
	Py_CLEAR(pyRes);
	Py_CLEAR(m_pyObj);
}

PyObject* CPySocket::WriteBytes(PyObject* data) {
	if (!PyBytes_Check(data)) {
		PyErr_SetString(PyExc_TypeError, "socket.WriteBytes needs bytes as argument");
		return NULL;
	}
	char* buffer;
	Py_ssize_t length;
	if (-1 == PyBytes_AsStringAndSize(data, &buffer, &length)) {
		return NULL;
	}
	if (Write(buffer, length)) {
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

template<> void TModInfo<CModPython>(CModInfo& Info) {
	Info.SetWikiPage("modpython");
}

GLOBALMODULEDEFS(CModPython, "Loads python scripts as ZNC modules")
