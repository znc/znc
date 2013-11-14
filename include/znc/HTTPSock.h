/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#ifndef _HTTPSOCK_H
#define _HTTPSOCK_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>
#include <znc/FileUtils.h>

class CModule;

class CHTTPSock : public CSocket {
public:
	CHTTPSock(CModule *pMod);
	CHTTPSock(CModule *pMod, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CHTTPSock();

	// Csocket derived members
	virtual void ReadData(const char* data, size_t len);
	virtual void ReadLine(const CString& sData);
	virtual void Connected();
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort) = 0;
	// !Csocket derived members

	// Hooks
	virtual bool ForceLogin();
	virtual bool OnLogin(const CString& sUser, const CString& sPass);
	virtual void OnPageRequest(const CString& sURI) = 0;
	virtual bool PrintFile(const CString& sFileName, CString sContentType = "");
	// !Hooks

	void CheckPost();
	bool SentHeader() const;
	bool PrintHeader(off_t uContentLength, const CString& sContentType = "", unsigned int uStatusId = 200, const CString& sStatusMsg = "OK");
	void AddHeader(const CString& sName, const CString& sValue);
	void SetContentType(const CString& sContentType);

	bool PrintNotFound();
	bool Redirect(const CString& sURL);
	bool PrintErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage);
	static void ParseParams(const CString& sParams, std::map<CString, VCString>& msvsParams);
	void ParseURI();
	void GetPage();
	static CString GetDate(time_t tm = 0);
	virtual CString GetRemoteIP();

	// Cookies
	CString GetRequestCookie(const CString& sKey) const;
	bool SendCookie(const CString& sKey, const CString& sValue);
	// Cookies

	// Setters
	void SetDocRoot(const CString& s);
	void SetLoggedIn(bool b) { m_bLoggedIn = b; }
	// !Setters

	// Getters
	CString GetPath() const;
	bool IsLoggedIn() const { return m_bLoggedIn; }
	const CString& GetDocRoot() const;
	const CString& GetUser() const;
	const CString& GetPass() const;
	const CString& GetParamString() const;
	const CString& GetContentType() const;
	bool IsPost() const;
	// !Getters

	// Parameter access
	CString GetParam(const CString& sName, bool bPost = true, const CString& sFilter = "\r\n") const;
	CString GetRawParam(const CString& sName, bool bPost = true) const;
	bool HasParam(const CString& sName, bool bPost = true) const;
	const std::map<CString, VCString>& GetParams(bool bPost = true) const;
	size_t GetParamValues(const CString& sName, VCString& vsRet, bool bPost = true, const CString& sFilter = "\r\n") const;
	size_t GetParamValues(const CString& sName, std::set<CString>& ssRet, bool bPost = true, const CString& sFilter = "\r\n") const;
	// !Parameter access
private:
	static CString GetRawParam(const CString& sName, const std::map<CString, VCString>& msvsParams);
	static CString GetParam(const CString& sName, const std::map<CString, VCString>& msvsParams, const CString& sFilter);
	static size_t GetParamValues(const CString& sName, VCString& vsRet, const std::map<CString, VCString>& msvsParams, const CString& sFilter);
	static size_t GetParamValues(const CString& sName, std::set<CString>& ssRet, const std::map<CString, VCString>& msvsParams, const CString& sFilter);

	void WriteFileUncompressed(CFile& File);
	void WriteFileGzipped(CFile& File);

protected:
	void PrintPage(const CString& sPage);
	void Init();

	bool                     m_bSentHeader;
	bool                     m_bGotHeader;
	bool                     m_bLoggedIn;
	bool                     m_bPost;
	bool                     m_bDone;
	unsigned long            m_uPostLen;
	CString                  m_sPostData;
	CString                  m_sURI;
	CString                  m_sUser;
	CString                  m_sPass;
	CString                  m_sContentType;
	CString                  m_sDocRoot;
	CString                  m_sForwardedIP;
	std::map<CString, VCString>   m_msvsPOSTParams;
	std::map<CString, VCString>   m_msvsGETParams;
	MCString                 m_msHeaders;
	bool                     m_bHTTP10Client;
	CString                  m_sIfNoneMatch;
	bool                     m_bAcceptGzip;
	MCString                 m_msRequestCookies;
	MCString                 m_msResponseCookies;
};

#endif // !_HTTPSOCK_H
