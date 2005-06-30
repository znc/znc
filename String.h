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

class CString : public string {
public:
	typedef enum {
		EAscii,
		EURL,
	} EEscape;

	CString() : string() {}
	CString(const char* c) : string(c) {}
	CString(const string& s) : string(s) {}
	virtual ~CString() {}

	int CaseCmp(const CString& s) const;
	int StrCmp(const CString& s) const;
	static bool WildCmp(const CString& sWild, const CString& sString);
	bool WildCmp(const CString& sWild) const;

	CString& MakeUpper();
	CString& MakeLower();
	CString AsUpper() const;
	CString AsLower() const;

	CString Escape_n(EEscape eFrom, EEscape eTo);
	CString Escape_n(EEscape eTo);
	CString& Escape(EEscape eFrom, EEscape eTo);
	CString& Escape(EEscape eTo);
	static unsigned int Replace(CString& sStr, const CString& sReplace, const CString& sWith);
	unsigned int Replace(const CString& sReplace, const CString& sWith);
	CString Token(unsigned int uPos, bool bRest = false, const CString& sSep = " ") const;
	CString Ellipsize(unsigned int uLen) const;
	CString Left(unsigned int uCount) const;
	CString Right(unsigned int uCount) const;
	VCString Split(const CString& sDelim, bool bKeepEmpty = true);
	unsigned int Split(const CString& sDelim, VCString& vsRet, bool bKeepEmpty = true);

	static CString Format(const CString& sFormatStr, ...);

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

	short ToShort() const;
	unsigned short ToUShort() const;
	int ToInt() const;
	unsigned int ToUInt() const;
	unsigned long long ToULongLong() const;
	long long ToLongLong() const;
	double ToDouble() const;

	bool Trim(const CString& s = " \t\r\n");
	bool LeftTrim(const CString& s = " \t\r\n");
	bool RightTrim(const CString& s = " \t\r\n");
	bool LeftChomp(unsigned int uLen = 1);
	bool RightChomp(unsigned int uLen = 1);

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
