/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef ZNCSTRING_H
#define ZNCSTRING_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>
#include <sys/types.h>

using std::map;
using std::set;
using std::string;
using std::vector;
using std::list;
using std::pair;

#define _SQL(s) CString("'" + CString(s).Escape_n(CString::ESQL) + "'")
#define _URL(s) CString(s).Escape_n(CString::EURL)
#define _HTML(s) CString(s).Escape_n(CString::EHTML)

class CString;
class MCString;

typedef set<CString> SCString;
typedef set<CString> SPair;

typedef vector<CString>                 VCString;
typedef vector<pair<CString, CString> > VPair;

typedef list<CString>                   LCString;
typedef list<pair<CString, CString> >   LPair;

static const unsigned char XX = 0xff;
static const unsigned char base64_table[256] = {
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
	52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
	XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
	15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
	XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
	41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

class CString : public string {
public:
	typedef enum {
		EASCII,
		EURL,
		EHTML,
		ESQL
	} EEscape;

	explicit CString(bool b) : string(b ? "true" : "false") {}
	explicit CString(char c);
	explicit CString(unsigned char c);
	explicit CString(short i);
	explicit CString(unsigned short i);
	explicit CString(int i);
	explicit CString(unsigned int i);
	explicit CString(long i);
	explicit CString(unsigned long i);
	explicit CString(long long i);
	explicit CString(unsigned long long i);
	explicit CString(double i, int precision = 2);
	explicit CString(float i, int precision = 2);

	CString() : string() {}
	CString(const char* c) : string(c) {}
	CString(const char* c, size_t l) : string(c, l) {}
	CString(const string& s) : string(s) {}
	~CString() {}

	inline unsigned char* strnchr(const unsigned char* src, unsigned char c, unsigned int iMaxBytes, unsigned char* pFill = NULL, unsigned int* piCount = NULL) const;
	int CaseCmp(const CString& s, unsigned long uLen = CString::npos) const;
	int StrCmp(const CString& s, unsigned long uLen = CString::npos) const;
	bool Equals(const CString& s, bool bCaseSensitive = false, unsigned long uLen = CString::npos) const;
	static bool WildCmp(const CString& sWild, const CString& sString);
	bool WildCmp(const CString& sWild) const;

	CString& MakeUpper();
	CString& MakeLower();
	CString AsUpper() const;
	CString AsLower() const;

	static EEscape ToEscape(const CString& sEsc);
	CString Escape_n(EEscape eFrom, EEscape eTo) const;
	CString Escape_n(EEscape eTo) const;
	CString& Escape(EEscape eFrom, EEscape eTo);
	CString& Escape(EEscape eTo);

	static unsigned int Replace(CString& sStr, const CString& sReplace, const CString& sWith, const CString& sLeft = "", const CString& sRight = "", bool bRemoveDelims = false);
	CString Replace_n(const CString& sReplace, const CString& sWith, const CString& sLeft = "", const CString& sRight = "", bool bRemoveDelims = false) const;
	unsigned int Replace(const CString& sReplace, const CString& sWith, const CString& sLeft = "", const CString& sRight = "", bool bRemoveDelims = false);
	CString Ellipsize(unsigned int uLen) const;
	CString Left(unsigned int uCount) const;
	CString Right(unsigned int uCount) const;

	CString FirstLine() const { return Token(0, false, "\n"); }
	CString Token(unsigned int uPos, bool bRest = false, const CString& sSep = " ", bool bAllowEmpty = false) const;
	CString Token(unsigned int uPos, bool bRest, const CString& sSep, bool bAllowEmpty, const CString& sLeft, const CString& sRight, bool bTrimQuotes = true) const;

	unsigned int URLSplit(MCString& msRet) const;
	unsigned int OptionSplit(MCString& msRet, bool bUpperKeys = false) const;
	unsigned int QuoteSplit(VCString& vsRet) const;

	unsigned int Split(const CString& sDelim, VCString& vsRet, bool bAllowEmpty = true,
					   const CString& sLeft = "", const CString& sRight = "", bool bTrimQuotes = true,
					   bool bTrimWhiteSpace = false) const;

	unsigned int Split(const CString& sDelim, SCString& ssRet, bool bAllowEmpty = true,
					   const CString& sLeft = "", const CString& sRight = "", bool bTrimQuotes = true,
					   bool bTrimWhiteSpace = false) const;

	static CString RandomString(unsigned int uLength);

	CString MD5() const;
	CString SHA256() const;
	unsigned long Base64Decode(CString& sRet) const;
	unsigned long Base64Decode();
	CString Base64Decode_n() const;
	bool Base64Encode(CString& sRet, unsigned int uWrap = 0) const;
	bool Base64Encode(unsigned int uWrap = 0);
	CString Base64Encode_n(unsigned int uWrap = 0) const;

#ifdef HAVE_LIBSSL
	CString Encrypt_n(const CString& sPass, const CString& sIvec = "") const;
	CString Decrypt_n(const CString& sPass, const CString& sIvec = "") const;
	void Encrypt(const CString& sPass, const CString& sIvec = "");
	void Decrypt(const CString& sPass, const CString& sIvec = "");
	void Crypt(const CString& sPass, bool bEncrypt, const CString& sIvec = "");
#endif

	static CString ToPercent(double d);
	static CString ToByteStr(unsigned long long d);
	static CString ToTimeStr(unsigned long s);

	bool ToBool() const;
	short ToShort() const;
	unsigned short ToUShort() const;
	int ToInt() const;
	long ToLong() const;
	unsigned int ToUInt() const;
	unsigned long ToULong() const;
	unsigned long long ToULongLong() const;
	long long ToLongLong() const;
	double ToDouble() const;

	bool Trim(const CString& s = " \t\r\n");
	bool TrimLeft(const CString& s = " \t\r\n");
	bool TrimRight(const CString& s = " \t\r\n");
	CString Trim_n(const CString& s = " \t\r\n") const;
	CString TrimLeft_n(const CString& s = " \t\r\n") const;
	CString TrimRight_n(const CString& s = " \t\r\n") const;

	bool TrimPrefix(const CString& sPrefix);
	bool TrimSuffix(const CString& sSuffix);
	CString TrimPrefix_n(const CString& sPrefix) const;
	CString TrimSuffix_n(const CString& sSuffix) const;

	bool LeftChomp(unsigned int uLen = 1);
	bool RightChomp(unsigned int uLen = 1);
	CString LeftChomp_n(unsigned int uLen = 1) const;
	CString RightChomp_n(unsigned int uLen = 1) const;

private:
protected:
};

class MCString : public map<CString, CString> {
public:
	MCString() : map<CString, CString>() {}
	virtual ~MCString() { clear(); }

	enum
	{
		MCS_SUCCESS   = 0,
		MCS_EOPEN     = 1,
		MCS_EWRITE    = 2,
		MCS_EWRITEFIL = 3,
		MCS_EREADFIL  = 4
	};

	int WriteToDisk(const CString& sPath, mode_t iMode = 0644) const;
	int ReadFromDisk(const CString& sPath, mode_t iMode = 0644);

	virtual bool WriteFilter(CString& sKey, CString& sValue) const { return true; }
	virtual bool ReadFilter(CString& sKey, CString& sValue) const { return true; }

	//! make them parse safe, right now using hex encoding on anything !isalnum
	virtual CString& Encode(CString& sValue) const;
	virtual CString& Decode(CString& sValue) const;
};

#endif // !ZNCSTRING_H
