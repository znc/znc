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

#include <znc/FileUtils.h>
#include <znc/znc.h>
#include <iomanip>


#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

using std::map;
using std::set;

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
	m_bAcceptGzip = false;
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
			// only queue a Set-Cookie to be sent if the client didn't send a Cookie header of the same name+value.
			m_msResponseCookies[sKey] = sValue;
		}
		return true;
	}

	return false;
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
	} else if (sName.Equals("X-Forwarded-For:")) {
		// X-Forwarded-For: client, proxy1, proxy2
		if (m_sForwardedIP.empty()) {
			const VCString& vsTrustedProxies = CZNC::Get().GetTrustedProxies();
			CString sIP = GetRemoteIP();

			VCString vsIPs;
			sLine.Token(1, true).Split(",", vsIPs, false, "", "", false, true);

			while (!vsIPs.empty()) {
				// sIP told us that it got connection from vsIPs.back()
				// check if sIP is trusted proxy
				bool bTrusted = false;
				for (VCString::const_iterator it = vsTrustedProxies.begin(); it != vsTrustedProxies.end(); ++it) {
					if (sIP.WildCmp(*it)) {
						bTrusted = true;
						break;
					}
				}
				if (bTrusted) {
					// sIP is trusted proxy, so use vsIPs.back() as new sIP
					sIP = vsIPs.back();
					vsIPs.pop_back();
				} else {
					break;
				}
			}

			// either sIP is not trusted proxy, or it's in the beginning of the X-Forwarded-For list
			// in both cases use it as the endpoind
			m_sForwardedIP = sIP;
		}
	} else if (sName.Equals("If-None-Match:")) {
		// this is for proper client cache support (HTTP 304) on static files:
		m_sIfNoneMatch = sLine.Token(1, true);
	} else if (sName.Equals("Accept-Encoding:") && !m_bHTTP10Client) {
		SCString ssEncodings;
		// trimming whitespace from the tokens is important:
		sLine.Token(1, true).Split(",", ssEncodings, false, "", "", false, true);
		m_bAcceptGzip = (ssEncodings.find("gzip") != ssEncodings.end());
	} else if (sLine.empty()) {
		m_bGotHeader = true;

		if (m_bPost) {
			m_sPostData = GetInternalReadBuffer();
			CheckPost();
		} else {
			GetPage();
		}

		DisableReadLine();
	}
}

CString CHTTPSock::GetRemoteIP() {
	if (!m_sForwardedIP.empty()) {
		return m_sForwardedIP;
	}

	return CSocket::GetRemoteIP();
}

CString CHTTPSock::GetDate(time_t stamp) {
	struct tm tm;
	std::stringstream stream;
	const char *wkday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	const char *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	if (stamp == 0)
		time(&stamp);
	gmtime_r(&stamp, &tm);

	stream << wkday[tm.tm_wday] << ", ";
	stream << std::setfill('0') << std::setw(2) << tm.tm_mday << " ";
	stream << month[tm.tm_mon] << " ";
	stream << std::setfill('0') << std::setw(4) << tm.tm_year + 1900 << " ";
	stream << std::setfill('0') << std::setw(2) << tm.tm_hour << ":";
	stream << std::setfill('0') << std::setw(2) << tm.tm_min << ":";
	stream << std::setfill('0') << std::setw(2) << tm.tm_sec << " GMT";

	return stream.str();
}

void CHTTPSock::GetPage() {
	DEBUG("Page Request [" << m_sURI << "] ");

	OnPageRequest(m_sURI);
}

#ifdef HAVE_ZLIB
static bool InitZlibStream(z_stream *zStrm, const char* buf) {
	memset(zStrm, 0, sizeof(z_stream));
	zStrm->next_in = (Bytef*)buf;

	// "15" is the default value for good compression,
	// the weird "+ 16" means "please generate a gzip header and trailer".
	const int WINDOW_BITS = 15 + 16;
	const int MEMLEVEL = 8;

	return (deflateInit2(zStrm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
		WINDOW_BITS, MEMLEVEL, Z_DEFAULT_STRATEGY) == Z_OK);
}
#endif

void CHTTPSock::PrintPage(const CString& sPage) {
#ifdef HAVE_ZLIB
	if (m_bAcceptGzip && !SentHeader()) {
		char szBuf[4096];
		z_stream zStrm;
		int zStatus, zFlush = Z_NO_FLUSH;

		if (InitZlibStream(&zStrm, sPage.c_str())) {
			DEBUG("- Sending gzip-compressed.");
			AddHeader("Content-Encoding", "gzip");
			PrintHeader(0); // we do not know the compressed data's length

			zStrm.avail_in = sPage.size();
			do {
				if (zStrm.avail_in == 0) {
					zFlush = Z_FINISH;
				}

				zStrm.next_out = (Bytef*)szBuf;
				zStrm.avail_out = sizeof(szBuf);

				zStatus = deflate(&zStrm, zFlush);

				if((zStatus == Z_OK || zStatus == Z_STREAM_END) && zStrm.avail_out < sizeof(szBuf)) {
					Write(szBuf, sizeof(szBuf) - zStrm.avail_out);	
				}
			} while(zStatus == Z_OK);

			Close(Csock::CLT_AFTERWRITE);
			return;
		}

	} // else: fall through
#endif
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
			sContentType = "text/html; charset=utf-8";
		} else if (sFileName.Right(4).Equals(".css")) {
			sContentType = "text/css; charset=utf-8";
		} else if (sFileName.Right(3).Equals(".js")) {
			sContentType = "application/x-javascript; charset=utf-8";
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
			sContentType = "text/plain; charset=utf-8";
		}
	}

	const time_t iMTime = File.GetMTime();
	bool bNotModified = false;
	CString sETag;

	if (iMTime > 0 && !m_bHTTP10Client) {
		sETag = "-" + CString(iMTime); // lighttpd style ETag

		AddHeader("Last-Modified", GetDate(iMTime));
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

#ifdef HAVE_ZLIB
		bool bGzip = m_bAcceptGzip && (sContentType.Left(5).Equals("text/") || sFileName.Right(3).Equals(".js"));

		if (bGzip) {
			DEBUG("- Sending gzip-compressed.");
			AddHeader("Content-Encoding", "gzip");
			PrintHeader(0, sContentType); // we do not know the compressed data's length
			WriteFileGzipped(File);
		} else
#endif
		{
			PrintHeader(iSize, sContentType);
			WriteFileUncompressed(File);
		}
	}

	DEBUG("- ETag: [" << sETag << "] / If-None-Match [" << m_sIfNoneMatch << "]");

	Close(Csock::CLT_AFTERWRITE);

	return true;
}

void CHTTPSock::WriteFileUncompressed(CFile& File) {
	char szBuf[4096];
	off_t iLen = 0;
	ssize_t i = 0;
	off_t iSize = File.GetSize();

	// while we haven't reached iSize and read() succeeds...
	while (iLen < iSize && (i = File.Read(szBuf, sizeof(szBuf))) > 0) {
		Write(szBuf, i);
		iLen += i;
	}

	if (i < 0) {
		DEBUG("- Error while reading file: " << strerror(errno));
	}
}

#ifdef HAVE_ZLIB
void CHTTPSock::WriteFileGzipped(CFile& File) {
	char szBufIn[8192];
	char szBufOut[8192];
	off_t iFileSize = File.GetSize(), iFileReadTotal = 0;
	z_stream zStrm;
	int zFlush = Z_NO_FLUSH;
	int zStatus;

	if (!InitZlibStream(&zStrm, szBufIn)) {
		DEBUG("- Error initializing zlib!");
		return;
	}

	do {
		ssize_t iFileRead = 0;

		if (zStrm.avail_in == 0) {
			// input buffer is empty, try to read more data from file.
			// if there is no more data, finish the stream.

			if (iFileReadTotal < iFileSize) {
				iFileRead = File.Read(szBufIn, sizeof(szBufIn));

				if (iFileRead < 1) {
					// wtf happened? better quit compressing.
					iFileReadTotal = iFileSize;
					zFlush = Z_FINISH;
				} else {
					iFileReadTotal += iFileRead;

					zStrm.next_in = (Bytef*)szBufIn;
					zStrm.avail_in = iFileRead;
				}
			} else {
				zFlush = Z_FINISH;
			}
		}

		zStrm.next_out = (Bytef*)szBufOut;
		zStrm.avail_out = sizeof(szBufOut);

		zStatus = deflate(&zStrm, zFlush);

		if ((zStatus == Z_OK || zStatus == Z_STREAM_END) && zStrm.avail_out < sizeof(szBufOut)) {
			// there's data in the buffer:
			Write(szBufOut, sizeof(szBufOut) - zStrm.avail_out);
		}

	} while (zStatus == Z_OK);

	deflateEnd(&zStrm);
}
#endif

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

size_t CHTTPSock::GetParamValues(const CString& sName, set<CString>& ssRet, bool bPost, const CString& sFilter) const {
	if (bPost)
		return GetParamValues(sName, ssRet, m_msvsPOSTParams, sFilter);
	return GetParamValues(sName, ssRet, m_msvsGETParams, sFilter);
}

size_t CHTTPSock::GetParamValues(const CString& sName, set<CString>& ssRet, const map<CString, VCString>& msvsParams, const CString& sFilter) {
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

size_t CHTTPSock::GetParamValues(const CString& sName, VCString& vsRet, bool bPost, const CString& sFilter) const {
	if (bPost)
		return GetParamValues(sName, vsRet, m_msvsPOSTParams, sFilter);
	return GetParamValues(sName, vsRet, m_msvsGETParams, sFilter);
}

size_t CHTTPSock::GetParamValues(const CString& sName, VCString& vsRet, const map<CString, VCString>& msvsParams, const CString& sFilter) {
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

	CString sPage =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
		"<!DOCTYPE html>\r\n"
		"<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">\r\n"
			"<head>\r\n"
				"<meta charset=\"UTF-8\"/>\r\n"
				"<title>" + CString(uStatusId) + " " + sStatusMsg.Escape_n(CString::EHTML) + "</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
				"<h1>" + sStatusMsg.Escape_n(CString::EHTML) + "</h1>\r\n"
				"<p>" + sMessage.Escape_n(CString::EHTML) + "</p>\r\n"
				"<hr/>\r\n"
				"<address>" +
					CZNC::GetTag(false, /* bHTML = */ true) +
					" at " + GetLocalIP().Escape_n(CString::EHTML) + " Port " + CString(GetLocalPort()) +
				"</address>\r\n"
			"</body>\r\n"
		"</html>\r\n";

	PrintHeader(sPage.length(), "text/html; charset=utf-8", uStatusId, sStatusMsg);
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return true;
}

bool CHTTPSock::ForceLogin() {
	if (m_bLoggedIn) {
		return true;
	}

	if (SentHeader()) {
		DEBUG("ForceLogin(): Header was already sent!");
		return false;
	}

	AddHeader("WWW-Authenticate", "Basic realm=\"" + CZNC::GetTag(false) + "\"");
	PrintErrorPage(401, "Unauthorized", "You need to login to view this page.");

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
		m_sContentType = "text/html; charset=utf-8";
	}

	DEBUG("- " << uStatusId << " (" << sStatusMsg << ") [" << m_sContentType << "]");

	Write("HTTP/" + CString(m_bHTTP10Client ? "1.0 " : "1.1 ") + CString(uStatusId) + " " + sStatusMsg + "\r\n");
	Write("Date: " + GetDate() + "\r\n");
	Write("Server: " + CZNC::GetTag(false) + "\r\n");
	if (uContentLength > 0) {
		Write("Content-Length: " + CString(uContentLength) + "\r\n");
	}
	Write("Content-Type: " + m_sContentType + "\r\n");

	MCString::iterator it;

	for (it = m_msResponseCookies.begin(); it != m_msResponseCookies.end(); ++it) {
		Write("Set-Cookie: " + it->first.Escape_n(CString::EURL) + "=" + it->second.Escape_n(CString::EURL) + "; path=/;" + (GetSSL() ? "Secure;" : "") + "\r\n");
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
	AddHeader("Location", sURL);
	PrintErrorPage(302, "Found", "The document has moved <a href=\"" + sURL.Escape_n(CString::EHTML) + "\">here</a>.");

	return true;
}

void CHTTPSock::Connected() {
	SetTimeout(120);
}
