/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include "HTTPSock.h"
#include "znc.h"

#define MAX_POST_SIZE	1024 * 1024

CHTTPSock::CHTTPSock(CModule *pMod) : CSocket(pMod) {
	Init();
}

CHTTPSock::CHTTPSock(CModule *pMod, const CString& sHostname, unsigned short uPort, int iTimeout) : CSocket(pMod, sHostname, uPort, iTimeout) {
	Init();
}

void CHTTPSock::Init() {
	m_bSentHeader = false;
	m_bGotHeader = false;
	m_bLoggedIn = false;
	m_bPost = false;
	m_bDone = false;
	m_bHTTP10Client = false;
	m_uPostLen = 0;
	EnableReadLine();
	SetMaxBufferThreshold(10240);
}

CHTTPSock::~CHTTPSock() {}

void CHTTPSock::ReadData(const char* data, size_t len) {
	if (!m_bDone && m_bGotHeader && m_bPost) {
		m_sPostData.append(data, len);
		CheckPost();
	}
}

bool CHTTPSock::SendCookie(const CString& sKey, const CString& sValue) {
	if (!sKey.empty() && !sValue.empty()) {
		if (m_msRequestCookies.find(sKey) == m_msRequestCookies.end() ||
			m_msRequestCookies[sKey].StrCmp(sValue) != 0)
		{
			m_msResponseCookies[sKey] = sValue;
		}
		return true;
	}

	return false;
}

const MCString& CHTTPSock::GetRequestCookies() const {
	return m_msRequestCookies;
}

CString CHTTPSock::GetRequestCookie(const CString& sKey) const {
	MCString::const_iterator it = m_msRequestCookies.find(sKey);

	return it != m_msRequestCookies.end() ? it->second : "";
}

void CHTTPSock::CheckPost() {
	if (m_sPostData.size() >= m_uPostLen) {
		ParseParams(m_sPostData.Left(m_uPostLen), m_msvsPOSTParams);
		GetPage();
		m_sPostData.clear();
		m_bDone = true;
	}
}

void CHTTPSock::ReadLine(const CString& sData) {
	if (m_bGotHeader) {
		return;
	}

	CString sLine = sData;
	sLine.TrimRight("\r\n");

	CString sName = sLine.Token(0);

	if (sName.Equals("GET")) {
		m_bPost = false;
		m_sURI = sLine.Token(1);
		m_bHTTP10Client = sLine.Token(2).Equals("HTTP/1.0");
		ParseURI();
	} else if (sName.Equals("POST")) {
		m_bPost = true;
		m_sURI = sLine.Token(1);
		ParseURI();
	} else if (sName.Equals("Cookie:")) {
		VCString vsNV;

		sLine.Token(1, true).Split(";", vsNV, false, "", "", true, true);

		for (unsigned int a = 0; a < vsNV.size(); a++) {
			CString s(vsNV[a]);

			m_msRequestCookies[s.Token(0, false, "=").Escape_n(CString::EURL, CString::EASCII)] =
				s.Token(1, true, "=").Escape_n(CString::EURL, CString::EASCII);
		}
	} else if (sName.Equals("Authorization:")) {
		CString sUnhashed;
		sLine.Token(2).Base64Decode(sUnhashed);
		m_sUser = sUnhashed.Token(0, false, ":");
		m_sPass = sUnhashed.Token(1, true, ":");
		m_bLoggedIn = OnLogin(m_sUser, m_sPass);
	} else if (sName.Equals("Content-Length:")) {
		m_uPostLen = sLine.Token(1).ToULong();
		if (m_uPostLen > MAX_POST_SIZE)
			PrintErrorPage(413, "Request Entity Too Large", "The request you sent was too large.");
	} else if (sName.Equals("If-None-Match:")) {
		// this is for proper client cache support (HTTP 304) on static files:
		m_sIfNoneMatch = sLine.Token(1, true);
	} else if (sLine.empty()) {
		m_bGotHeader = true;
		DisableReadLine();

		if (m_bPost) {
			m_sPostData = GetInternalReadBuffer();
			CheckPost();
		} else {
			GetPage();
		}
	}
}

void CHTTPSock::GetPage() {
	DEBUG("Page Request [" << m_sURI << "] ");

	OnPageRequest(m_sURI);
}

void CHTTPSock::PrintPage(const CString& sPage) {
	if (!SentHeader()) {
		PrintHeader(sPage.length());
	} else {
		DEBUG("PrintPage(): Header was already sent");
	}

	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);
}

bool CHTTPSock::PrintFile(const CString& sFileName, CString sContentType) {
	CString sFilePath = sFileName;

	if (!m_sDocRoot.empty()) {
		sFilePath.TrimLeft("/");

		sFilePath = CDir::CheckPathPrefix(m_sDocRoot, sFilePath, m_sDocRoot);

		if (sFilePath.empty()) {
			PrintErrorPage(403, "Forbidden", "You don't have permission to access that file on this server.");
			DEBUG("THIS FILE:     [" << sFilePath << "] does not live in ...");
			DEBUG("DOCUMENT ROOT: [" << m_sDocRoot << "]");
			return false;
		}
	}

	CFile File(sFilePath);

	if (!File.Open()) {
		PrintNotFound();
		return false;
	}

	if (sContentType.empty()) {
		if (sFileName.Right(5).Equals(".html") || sFileName.Right(4).Equals(".htm")) {
			sContentType = "text/html";
		} else if (sFileName.Right(4).Equals(".css")) {
			sContentType = "text/css";
		} else if (sFileName.Right(3).Equals(".js")) {
			sContentType = "application/x-javascript";
		} else if (sFileName.Right(4).Equals(".jpg")) {
			sContentType = "image/jpeg";
		} else if (sFileName.Right(4).Equals(".gif")) {
			sContentType = "image/gif";
		} else if (sFileName.Right(4).Equals(".ico")) {
			sContentType = "image/x-icon";
		} else if (sFileName.Right(4).Equals(".png")) {
			sContentType = "image/png";
		} else if (sFileName.Right(4).Equals(".bmp")) {
			sContentType = "image/bmp";
		} else {
			sContentType = "text/plain";
		}
	}

	const time_t iMTime = File.GetMTime();
	bool bNotModified = false;
	CString sETag;

	if (iMTime > 0 && !m_bHTTP10Client) {
		sETag = "-" + CString(iMTime); // lighttpd style ETag

		AddHeader("ETag", "\"" + sETag + "\"");
		AddHeader("Cache-Control", "public");

		if (!m_sIfNoneMatch.empty()) {
			m_sIfNoneMatch.Trim("\\\"'");
			bNotModified = (m_sIfNoneMatch.Equals(sETag, true));
		}
	}

	if (bNotModified) {
		PrintHeader(0, sContentType, 304, "Not Modified");
	} else {
		off_t iSize = File.GetSize();

		// Don't try to send files over 16 MiB, because it might block
		// the whole process and use huge amounts of memory.
		if (iSize > 16 * 1024 * 1024) {
			DEBUG("- Abort: File is over 16 MiB big: " << iSize);
			PrintErrorPage(500, "Internal Server Error", "File too big");
			return true;
		}

		char szBuf[4096];
		off_t iLen = 0;
		int i = 0;

		PrintHeader(iSize, sContentType);

		// while we haven't reached iSize and read() succeeds...
		while (iLen < iSize && (i = File.Read(szBuf, sizeof(szBuf))) > 0) {
			Write(szBuf, i);
			iLen += i;
		}

		if (i < 0) {
			DEBUG("- Error while reading file: " << strerror(errno));
		}
	}

	DEBUG("- ETag: [" << sETag << "] / If-None-Match [" << m_sIfNoneMatch << "]");

	Close(Csock::CLT_AFTERWRITE);

	return true;
}

void CHTTPSock::ParseURI() {
	ParseParams(m_sURI.Token(1, true, "?"), m_msvsGETParams);
	m_sURI = m_sURI.Token(0, false, "?");
}

CString CHTTPSock::GetPath() const {
	return m_sURI.Token(0, false, "?");
}

void CHTTPSock::ParseParams(const CString& sParams, map<CString, VCString> &msvsParams) {
	msvsParams.clear();

	VCString vsPairs;
	sParams.Split("&", vsPairs, true);

	for (unsigned int a = 0; a < vsPairs.size(); a++) {
		const CString& sPair = vsPairs[a];
		CString sName = sPair.Token(0, false, "=").Escape_n(CString::EURL, CString::EASCII);
		CString sValue = sPair.Token(1, true, "=").Escape_n(CString::EURL, CString::EASCII);

		msvsParams[sName].push_back(sValue);
	}
}

void CHTTPSock::SetDocRoot(const CString& s) {
	m_sDocRoot = s + "/";
	m_sDocRoot.Replace("//", "/");
}

const CString& CHTTPSock::GetDocRoot() const {
	return m_sDocRoot;
}

const CString& CHTTPSock::GetUser() const {
	return m_sUser;
}

const CString& CHTTPSock::GetPass() const {
	return m_sPass;
}

const CString& CHTTPSock::GetContentType() const {
	return m_sContentType;
}

const CString& CHTTPSock::GetParamString() const {
	return m_sPostData;
}

bool CHTTPSock::HasParam(const CString& sName, bool bPost) const {
	if (bPost)
		return (m_msvsPOSTParams.find(sName) != m_msvsPOSTParams.end());
	return (m_msvsGETParams.find(sName) != m_msvsGETParams.end());
}

CString CHTTPSock::GetRawParam(const CString& sName, bool bPost) const {
	if (bPost)
		return GetRawParam(sName, m_msvsPOSTParams);
	return GetRawParam(sName, m_msvsGETParams);
}

CString CHTTPSock::GetRawParam(const CString& sName, const map<CString, VCString>& msvsParams) {
	CString sRet;

	map<CString, VCString>::const_iterator it = msvsParams.find(sName);

	if (it != msvsParams.end() && it->second.size() > 0) {
		sRet = it->second[0];
	}

	return sRet;
}

CString CHTTPSock::GetParam(const CString& sName, bool bPost, const CString& sFilter) const {
	if (bPost)
		return GetParam(sName, m_msvsPOSTParams, sFilter);
	return GetParam(sName, m_msvsGETParams, sFilter);
}

CString CHTTPSock::GetParam(const CString& sName, const map<CString, VCString>& msvsParams, const CString& sFilter) {
	CString sRet = GetRawParam(sName, msvsParams);
	sRet.Trim();

	for (size_t i = 0; i < sFilter.length(); i++) {
		sRet.Replace(CString(sFilter.at(i)), "");
	}

	return sRet;
}

unsigned int CHTTPSock::GetParamValues(const CString& sName, set<CString>& ssRet, bool bPost, const CString& sFilter) const {
	if (bPost)
		return GetParamValues(sName, ssRet, m_msvsPOSTParams, sFilter);
	return GetParamValues(sName, ssRet, m_msvsGETParams, sFilter);
}

unsigned int CHTTPSock::GetParamValues(const CString& sName, set<CString>& ssRet, const map<CString, VCString>& msvsParams, const CString& sFilter) {
	ssRet.clear();

	map<CString, VCString>::const_iterator it = msvsParams.find(sName);

	if (it != msvsParams.end()) {
		for (unsigned int a = 0; a < it->second.size(); a++) {
			CString sParam = it->second[a];
			sParam.Trim();

			for (size_t i = 0; i < sFilter.length(); i++) {
				sParam.Replace(CString(sFilter.at(i)), "");
			}
			ssRet.insert(sParam);
		}
	}

	return ssRet.size();
}

unsigned int CHTTPSock::GetParamValues(const CString& sName, VCString& vsRet, bool bPost, const CString& sFilter) const {
	if (bPost)
		return GetParamValues(sName, vsRet, m_msvsPOSTParams, sFilter);
	return GetParamValues(sName, vsRet, m_msvsGETParams, sFilter);
}

unsigned int CHTTPSock::GetParamValues(const CString& sName, VCString& vsRet, const map<CString, VCString>& msvsParams, const CString& sFilter) {
	vsRet.clear();

	map<CString, VCString>::const_iterator it = msvsParams.find(sName);

	if (it != msvsParams.end()) {
		for (unsigned int a = 0; a < it->second.size(); a++) {
			CString sParam = it->second[a];
			sParam.Trim();

			for (size_t i = 0; i < sFilter.length(); i++) {
				sParam.Replace(CString(sFilter.at(i)), "");
			}
			vsRet.push_back(sParam);
		}
	}

	return vsRet.size();
}

const map<CString, VCString>& CHTTPSock::GetParams(bool bPost) const {
	if (bPost)
		return m_msvsPOSTParams;
	return m_msvsGETParams;
}

bool CHTTPSock::IsPost() const {
	return m_bPost;
}

bool CHTTPSock::PrintNotFound() {
	return PrintErrorPage(404, "Not Found", "The requested URL was not found on this server.");
}

bool CHTTPSock::PrintErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage) {
	if (SentHeader()) {
		DEBUG("PrintErrorPage(): Header was already sent");
		return false;
	}

	CString sPage = GetErrorPage(uStatusId, sStatusMsg, sMessage);
	PrintHeader(sPage.length(), "text/html", uStatusId, sStatusMsg);
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return true;
}

CString CHTTPSock::GetErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage) {
	return "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html><head>\r\n<title>" + CString(uStatusId) + " " + sStatusMsg.Escape_n(CString::EHTML) + "</title>\r\n"
		"</head><body>\r\n<h1>" + sStatusMsg.Escape_n(CString::EHTML) + "</h1>\r\n"
		"<p>" + sMessage.Escape_n(CString::EHTML) + "</p>\r\n"
		"<hr />\r\n<address>" + CZNC::GetTag(false).Escape_n(CString::EHTML) + " at " + GetLocalIP().Escape_n(CString::EHTML) + " Port " + CString(GetLocalPort()) + "</address>\r\n"
		"</body></html>\r\n";
}

bool CHTTPSock::ForceLogin() {
	if (m_bLoggedIn) {
		return true;
	}

	if (SentHeader()) {
		DEBUG("ForceLogin(): Header was already sent!");
		return false;
	}

	CString sPage = GetErrorPage(401, "Unauthorized", "You need to login to view this page.");
	AddHeader("WWW-Authenticate", "Basic realm=\"" + CZNC::GetTag(false) + "\"");
	PrintHeader(sPage.length(), "text/html", 401, "Unauthorized");
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return false;
}

bool CHTTPSock::OnLogin(const CString& sUser, const CString& sPass) {
	return false;
}

bool CHTTPSock::SentHeader() const {
	return m_bSentHeader;
}

bool CHTTPSock::PrintHeader(off_t uContentLength, const CString& sContentType, unsigned int uStatusId, const CString& sStatusMsg) {
	if (SentHeader()) {
		DEBUG("PrintHeader(): Header was already sent!");
		return false;
	}

	if (!sContentType.empty()) {
		m_sContentType = sContentType;
	}

	if (m_sContentType.empty()) {
		m_sContentType = "text/html";
	}

	DEBUG("- " << uStatusId << " (" << sStatusMsg << ") [" << m_sContentType << "]");

	Write("HTTP/" + CString(m_bHTTP10Client ? "1.0 " : "1.1 ") + CString(uStatusId) + " " + sStatusMsg + "\r\n");
	//Write("Date: Tue, 28 Jun 2005 20:45:36 GMT\r\n");
	Write("Server: " + CZNC::GetTag(false) + "\r\n");
	if (uContentLength > 0) {
		Write("Content-Length: " + CString(uContentLength) + "\r\n");
	}
	Write("Content-Type: " + m_sContentType + "\r\n");

	MCString::iterator it;

	for (it = m_msResponseCookies.begin(); it != m_msResponseCookies.end(); ++it) {
		Write("Set-Cookie: " + it->first.Escape_n(CString::EURL) + "=" + it->second.Escape_n(CString::EURL) + "; path=/;\r\n");
	}

	for (it = m_msHeaders.begin(); it != m_msHeaders.end(); ++it) {
		Write(it->first + ": " + it->second + "\r\n");
	}

	Write("Connection: Close\r\n");

	Write("\r\n");
	m_bSentHeader = true;

	return true;
}

void CHTTPSock::SetContentType(const CString& sContentType) {
	m_sContentType = sContentType;
}

void CHTTPSock::AddHeader(const CString& sName, const CString& sValue) {
	m_msHeaders[sName] = sValue;
}

bool CHTTPSock::Redirect(const CString& sURL) {
	if (SentHeader()) {
		DEBUG("Redirect() - Header was already sent");
		return false;
	}

	DEBUG("- Redirect to [" << sURL << "]");
	CString sPage = GetErrorPage(302, "Found", "The document has moved <a href=\"" + sURL.Escape_n(CString::EHTML) + "\">here</a>.");
	AddHeader("Location", sURL);
	PrintHeader(sPage.length(), "text/html", 302, "Found");
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return true;
}

void CHTTPSock::Timeout() {
}

void CHTTPSock::SockError(int iErrno) {
}

void CHTTPSock::Connected() {
	SetTimeout(120);
}

void CHTTPSock::Disconnected() {
}

void CHTTPSock::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");
	Close();
}
