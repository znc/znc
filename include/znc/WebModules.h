/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#ifndef _WEBMODULES_H
#define _WEBMODULES_H

#include <znc/zncconfig.h>
#include <znc/Template.h>
#include <znc/HTTPSock.h>
#include <znc/Utils.h>

class CAuthBase;
class CUser;
class CWebSock;
class CModule;
class CWebSubPage;

typedef std::shared_ptr<CWebSubPage> TWebSubPage;
typedef std::vector<TWebSubPage> VWebSubPages;

class CZNCTagHandler : public CTemplateTagHandler {
public:
	CZNCTagHandler(CWebSock& pWebSock);
	virtual ~CZNCTagHandler() {}

	bool HandleTag(CTemplate& Tmpl, const CString& sName, const CString& sArgs, CString& sOutput) override;
private:
	CWebSock& m_WebSock;
};


class CWebSession {
public:
	CWebSession(const CString& sId, const CString& sIP);
	~CWebSession();

	CWebSession(const CWebSession&) = delete;
	CWebSession& operator=(const CWebSession&) = delete;

	const CString& GetId() const { return m_sId; }
	const CString& GetIP() const { return m_sIP; }
	CUser* GetUser() const { return m_pUser; }
	time_t GetLastActive() const { return m_tmLastActive; }
	bool IsLoggedIn() const { return m_pUser != nullptr; }
	bool IsAdmin() const;
	void UpdateLastActive();

	CUser* SetUser(CUser* p) { m_pUser = p; return m_pUser; }

	void ClearMessageLoops();
	void FillMessageLoops(CTemplate& Tmpl);
	size_t AddError(const CString& sMessage);
	size_t AddSuccess(const CString& sMessage);
private:
	CString         m_sId;
	CString         m_sIP;
	CUser*          m_pUser;
	VCString        m_vsErrorMsgs;
	VCString        m_vsSuccessMsgs;
	time_t          m_tmLastActive;
};


class CWebSubPage {
public:
	CWebSubPage(const CString& sName, const CString& sTitle = "", unsigned int uFlags = 0) : m_sName(sName), m_sTitle(sTitle) {
		m_uFlags = uFlags;
	}

	CWebSubPage(const CString& sName, const CString& sTitle, const VPair& vParams, unsigned int uFlags = 0) : m_sName(sName), m_sTitle(sTitle), m_vParams(vParams) {
		m_uFlags = uFlags;
	}

	virtual ~CWebSubPage() {}

	enum {
		F_ADMIN = 1
	};

	void SetName(const CString& s) { m_sName = s; }
	void SetTitle(const CString& s) { m_sTitle = s; }
	void AddParam(const CString& sName, const CString& sValue) { m_vParams.push_back(make_pair(sName, sValue)); }

	bool RequiresAdmin() const { return m_uFlags & F_ADMIN; }

	const CString& GetName() const { return m_sName; }
	const CString& GetTitle() const { return m_sTitle; }
	const VPair& GetParams() const { return m_vParams; }
private:
	unsigned int    m_uFlags;
	CString         m_sName;
	CString         m_sTitle;
	VPair           m_vParams;
};

class CWebSessionMap : public TCacheMap<CString, std::shared_ptr<CWebSession> > {
	public:
		CWebSessionMap(unsigned int uTTL = 5000) : TCacheMap<CString, std::shared_ptr<CWebSession> >(uTTL) {}
		void FinishUserSessions(const CUser& User);
};

class CWebSock : public CHTTPSock {
public:
	enum EPageReqResult {
		PAGE_NOTFOUND, // print 404 and Close()
		PAGE_PRINT,    // print page contents and Close()
		PAGE_DEFERRED, // async processing, Close() will be called from a different place
		PAGE_DONE      // all stuff has been done
	};

	CWebSock(const CString& sURIPrefix);
	virtual ~CWebSock();

	bool ForceLogin() override;
	bool OnLogin(const CString& sUser, const CString& sPass, bool bBasic) override;
	void OnPageRequest(const CString& sURI) override;

	EPageReqResult PrintTemplate(const CString& sPageName, CString& sPageRet, CModule* pModule = nullptr);
	EPageReqResult PrintStaticFile(const CString& sPath, CString& sPageRet, CModule* pModule = nullptr);

	CString FindTmpl(CModule* pModule, const CString& sName);

	void PrintErrorPage(const CString& sMessage);

	std::shared_ptr<CWebSession> GetSession();

	Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
	static CString GetSkinPath(const CString& sSkinName);
	void GetAvailSkins(VCString& vRet) const;
	CString GetSkinName();

	CString GetRequestCookie(const CString& sKey);
	bool SendCookie(const CString& sKey, const CString& sValue);

	static void FinishUserSessions(const CUser& User);

protected:
	using CHTTPSock::PrintErrorPage;

	bool AddModLoop(const CString& sLoopName, CModule& Module, CTemplate *pTemplate = nullptr);
	VCString GetDirs(CModule* pModule, bool bIsTemplate);
	void SetPaths(CModule* pModule, bool bIsTemplate = false);
	void SetVars();
	CString GetCSRFCheck();

private:
	EPageReqResult OnPageRequestInternal(const CString& sURI, CString& sPageRet);

	bool                    m_bPathsSet;
	CTemplate               m_Template;
	std::shared_ptr<CAuthBase> m_spAuth;
	CString                 m_sModName;
	CString                 m_sPath;
	CString                 m_sPage;
	std::shared_ptr<CWebSession> m_spSession;

	static const unsigned int m_uiMaxSessions;
};

#endif // !_WEBMODULES_H
