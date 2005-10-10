#ifndef _UTILS_H
#define _UTILS_H

#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/file.h>
#include <stdio.h>

#include "String.h"
#include <vector>
#include <map>
using std::vector;
using std::map;

#ifdef _DEBUG
#define DEBUG_ONLY(f)   f
#else
#define DEBUG_ONLY(f) ((void)0)
#endif

static const char g_HexDigits[] = "0123456789abcdef";

class CUtils {
public:
	CUtils();
	virtual ~CUtils();

	static CString GetIP(unsigned long addr);
	static unsigned long GetLongIP(const CString& sIP);
	static CString ChangeDir(const CString& sPath, const CString& sAdd, const CString& sHomeDir);
	static int MakeDir(const CString& sPath, mode_t iMode = 0700);
	static void PrintError(const CString& sMessage);
	static void PrintMessage(const CString& sMessage, bool bStrong = false);
	static void PrintPrompt(const CString& sMessage);
	static void PrintAction(const CString& sMessage);
	static void PrintStatus(bool bSuccess, const CString& sMessage = "");
	static CString GetHashPass();
	static char* GetPass(const CString& sPrompt);
	static bool GetInput(const CString& sPrompt, CString& sRet, const CString& sDefault = "", const CString& sHint = "");
	static bool GetBoolInput(const CString& sPrompt, bool bDefault);
	static bool GetBoolInput(const CString& sPrompt, bool *pbDefault = NULL);
	static bool GetNumInput(const CString& sPrompt, unsigned int& uRet, unsigned int uMin = 0, unsigned int uMax = ~0, unsigned int uDefault = ~0);

	static unsigned long long GetMillTime() {
		struct timeval tv;
		unsigned long long iTime = 0;
		gettimeofday(&tv, NULL);
		iTime = (unsigned long long) tv.tv_sec * 1000;
		iTime += ((unsigned long long) tv.tv_usec / 1000);
		return iTime;
	}
#ifdef HAVE_LIBSSL
	static void GenerateCert(FILE *pOut, bool bEncPrivKey = false, const CString& sHost = "");
#endif /* HAVE_LIBSSL */

private:
protected:
};

class CLockFile {
public:
	CLockFile() {
		m_bCreated = false;
		m_fd = 0;
		m_pid = 0;
	}

	CLockFile(const CString& sFile) {
		Open(sFile);
	}

	virtual ~CLockFile() {
		if (getpid() == m_pid) {
			if (m_fd > -1) {
				UnLock();
				close(m_fd);
				if (m_bCreated) {
					unlink(m_sFileName.c_str());
				}
			}
		}
	}

	void Open(const CString& sFile) {
		m_fd = open(sFile.c_str(), O_RDONLY);
		m_bCreated = false;

		if (m_fd == -1) {
			// i must create the file then
			m_fd = open(sFile.c_str(), O_RDWR|O_CREAT, 0644);
			m_bCreated = true;
		}

		m_pid = getpid();       // for destructor
		m_sFileName = sFile;
	}

	//! timeout in milliseconds
	bool TryExLock(const CString& sLockFile, unsigned long long iTimeout = 0) {
		Open(sLockFile);
		return TryExLock(iTimeout);
	}

	bool TryExLock(unsigned long long iTimeout = 0) {
		if (iTimeout == 0) {
			return Lock(LOCK_EX|LOCK_NB);
		}

		unsigned long long iNow = CUtils::GetMillTime();

		while(true) {
			if (Lock(LOCK_EX|LOCK_NB)) {
				return true;
			}

			if ((CUtils::GetMillTime() - iNow) > iTimeout) {
				break;
			}

			usleep(100);
		}

		return(false);
	}

	bool TryShLock(unsigned long long iTimeout = 0) {
		if (iTimeout == 0) {
			return(Lock(LOCK_SH|LOCK_NB));
		}

		unsigned long long iNow = CUtils::GetMillTime();

		while(true) {
			if (Lock(LOCK_SH|LOCK_NB)) {
				return true;
			}

			if ((CUtils::GetMillTime() - iNow) > iTimeout) {
				break;
			}

			usleep(100);
		}

		return false;
	}

	bool LockEx() { return Lock(LOCK_EX); }
	bool LockSh() { return Lock(LOCK_SH); }
	bool UnLock() { return Lock(LOCK_UN); }

private:
	bool Lock(int iOperation) {
		if (m_fd == -1) {
			return false;
		}

		if (::flock(m_fd, iOperation) != 0) {
			return false;
		}

		return true;
	}

	int			m_fd;
	int			m_pid;
	bool		m_bCreated;
	CString		m_sFileName;
};

class CException {
public:
	typedef enum {
		EX_Shutdown,
		EX_BadModVersion,
	} EType;

	CException(EType e) {
		m_eType = e;
	}
	virtual ~CException() {}

	EType GetType() const { return m_eType; }
private:
protected:
	EType	m_eType;
};


class CTable : public vector<map<CString, CString>* > {
public:
	CTable();
	virtual ~CTable();

	bool AddColumn(const CString& sName);
	unsigned int AddRow();
	bool SetCell(const CString& sColumn, const CString& sValue, unsigned int uRowIdx = ~0);
	bool GetLine(unsigned int uIdx, CString& sLine);

	unsigned int GetColumnWidth(unsigned int uIdx);
private:
protected:
	vector<CString>				m_vsHeaders;
	map<CString, unsigned int>	m_msuWidths;	// Used to cache the width of a column
};


#ifdef HAVE_LIBSSL
#include <openssl/blowfish.h>
#include <openssl/md5.h>
//! does Blowfish w/64 bit feedback, no padding
class CBlowfish {
public:
	/**
	 * @sPassword key to encrypt with
	 * @iEncrypt encrypt method (BF_DECRYPT or BF_ENCRYPT)
	 * @sIvec what to set the ivector to start with, default sets it all 0's
	 */
	CBlowfish(const CString & sPassword, int iEncrypt, const CString & sIvec = "");
	~CBlowfish();

	//! output must be freed
	static unsigned char *MD5(const unsigned char *input, u_int ilen);

	//! returns an md5 of the CString (not hex encoded)
	static CString MD5(const CString & sInput, bool bHexEncode = false);

	//! output must be the same size as input
	void Crypt(unsigned char *input, unsigned char *output, u_int ibytes);

	//! must free result
	unsigned char * Crypt(unsigned char *input, u_int ibytes);
	CString Crypt(const CString & sData);

private:
	unsigned char		*m_ivec;
	BF_KEY 				m_bkey;
	int					m_iEncrypt, m_num;
};

#endif /* HAVE_LIBSSL */

#define RF_BUFF 4096
inline bool ReadFile(const CString & sFilename, CString & sLine) {
	char inbuff[RF_BUFF];
	int bytes;
	// clear ourselves out
	sLine.clear();

	FILE *f = fopen(sFilename.c_str(), "r");

	if (!f) {
		return false;
	}

	while((bytes = fread(inbuff, sizeof(char), RF_BUFF, f)) > 0) {
		sLine.append(inbuff, bytes);
	}

	fclose(f);
	if (bytes < 0) {
		return false;
	}

	return true;
}

inline bool WriteFile(const CString & sFilename, const CString & sData) {
	FILE *f = fopen(sFilename.c_str(), "w");
	if (!f) {
		return false;
	}

	int iRet = fwrite(sData.data(), sizeof(char), sData.length(), f);

	fclose(f);

	if (iRet <= 0) {
		return false;
	}

	return true;
}

inline bool ReadLine(const CString & sData, CString & sLine, u_int & iPos) {
	sLine.clear();

	if (iPos >= sData.length()) {
		return false;
	}

	CString::size_type iFind = sData.find("\n", iPos);

	if (iFind == CString::npos) {
		sLine = sData.substr(iPos, (sData.length() - iPos));
		iPos = CString::npos;
		return true;
	}

	sLine = sData.substr(iPos, (iFind - iPos)) + "\n";
	iPos = iFind + 1;

	return true;
}

inline CString Lower(const CString & sLine) {
	CString sRet;
	for(u_int a = 0; a < sLine.length(); a++) {
		sRet += tolower(sLine[a]);
	}

	return sRet;
}

inline CString Upper(const CString & sLine) {
	CString sRet;
	for(u_int a = 0; a < sLine.length(); a++) {
		sRet += toupper(sLine[a]);
	}

	return sRet;
}

#endif // !_UTILS_H

