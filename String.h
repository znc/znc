#ifndef X_STRING_H
#define X_STRING_H

#include <string>
#include <sstream>
#include <map>
#include <vector>

using std::map;
using std::string;
using std::vector;
using std::stringstream;

class CString;
typedef vector<CString> VCString;

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

extern const char* g_szHTMLescapes[256];

class CString : public string {
public:
	typedef enum {
		EAscii,
		EURL,
		EHTML,
	} EEscape;

	CString() : string() {}
	CString(const char* c) : string(c) {}
	CString(const string& s) : string(s) {}
	virtual ~CString() {}

	inline unsigned char* strnchr(const unsigned char* src, unsigned char c, unsigned int iMaxBytes, unsigned char* pFill = NULL, unsigned int* piCount = NULL) const;
	int CaseCmp(const CString& s) const;
	int StrCmp(const CString& s) const;
	static bool WildCmp(const CString& sWild, const CString& sString);
	bool WildCmp(const CString& sWild) const;

	CString& MakeUpper();
	CString& MakeLower();
	CString AsUpper() const;
	CString AsLower() const;

	CString Escape_n(EEscape eFrom, EEscape eTo) const;
	CString Escape_n(EEscape eTo) const;
	CString& Escape(EEscape eFrom, EEscape eTo);
	CString& Escape(EEscape eTo);

	static unsigned int Replace(CString& sStr, const CString& sReplace, const CString& sWith);
	unsigned int Replace(const CString& sReplace, const CString& sWith);
	CString Ellipsize(unsigned int uLen) const;
	CString Left(unsigned int uCount) const;
	CString Right(unsigned int uCount) const;

	CString Token(unsigned int uPos, bool bRest = false, const CString& sSep = " ") const;
	VCString Split(const CString& sDelim, bool bKeepEmpty = true) const;
	unsigned int Split(const CString& sDelim, VCString& vsRet, bool bAllowEmpty = true) const;
	static CString Format(const CString& sFormatStr, ...);

	CString MD5() const;
	unsigned long Base64Decode(CString& sRet) const;
	unsigned long Base64Decode();
	CString Base64Decode_n() const;
	bool Base64Encode(CString& sRet, unsigned int uWrap = 0) const;
	bool Base64Encode(unsigned int uWrap = 0);
	CString Base64Encode_n(unsigned int uWrap = 0) const;

#ifdef HAVE_LIBSSL
	CString Encrypt_n(const CString& sPass, const CString& sIvec = "");
	CString Decrypt_n(const CString& sPass, const CString& sIvec = "");
	void Encrypt(const CString& sPass, const CString& sIvec = "");
	void Decrypt(const CString& sPass, const CString& sIvec = "");
	void Crypt(const CString& sPass, bool bEncrypt, const CString& sIvec = "");
#endif

	static CString ToString(char c);
	static CString ToString(unsigned char c);
	static CString ToString(short i);
	static CString ToString(unsigned short i);
	static CString ToString(int i);
	static CString ToString(unsigned int i);
	static CString ToString(long i);
	static CString ToString(unsigned long i);
	static CString ToString(long long i);
	static CString ToString(unsigned long long i);
	static CString ToString(double i);
	static CString ToString(float i);
	static CString ToPercent(double d);
	static CString ToKBytes(double d);

	bool ToBool() const;
	short ToShort() const;
	unsigned short ToUShort() const;
	int ToInt() const;
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

	bool LeftChomp(unsigned int uLen = 1);
	bool RightChomp(unsigned int uLen = 1);
	CString LeftChomp_n(unsigned int uLen = 1);
	CString RightChomp_n(unsigned int uLen = 1);

private:
protected:
};

class MCString : public map<CString, CString> {
public:
	MCString() : map<CString, CString>() {}
	virtual ~MCString() { clear(); }

	enum
	{
		MCS_SUCCESS 	= 0,
		MCS_EOPEN 		= 1,
		MCS_EWRITE 		= 2,
		MCS_EWRITEFIL 	= 3,
		MCS_EREADFIL	= 4
	};

	int WriteToDisk(const CString& sPath, mode_t iMode = 0644);
	int ReadFromDisk(const CString& sPath, mode_t iMode = 0644);

	virtual bool WriteFilter(CString& sKey, CString& sValue) { return true; }
	virtual bool ReadFilter(CString& sKey, CString& sValue) { return true; }

	//! make them parse safe, right now using hex encoding on anything !isalnum
	virtual CString& Encode(CString& sValue);
	virtual CString& Decode(CString& sValue);
};

#endif // !X_STRING_H
