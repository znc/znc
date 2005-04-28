#ifndef _UTILS_H
#define _UTILS_H

#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/file.h>

#include <string>
#include <vector>
#include <map>
using std::string;
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

	static string GetIP(unsigned long addr);
	static unsigned long GetLongIP(const string& sIP);
	static string ChangeDir(const string& sPath, const string& sAdd, const string& sHomeDir);
	static int MakeDir(const string& sPath, mode_t iMode = 0700);
	static void PrintError(const string& sMessage);
	static void PrintMessage(const string& sMessage);
	static void PrintPrompt(const string& sMessage);
	static void PrintAction(const string& sMessage);
	static void PrintStatus(bool bSuccess, const string& sMessage = "");
	static char* GetPass(const string& sPrompt);
	static bool GetInput(const string& sPrompt, string& sRet);
	static bool GetBoolInput(const string& sPrompt);

	static string ToString(short i);
	static string ToString(unsigned short i);
	static string ToString(int i);
	static string ToString(unsigned int i);
	static string ToString(long i);
	static string ToString(unsigned long i);
	static string ToString(unsigned long long i);
	static string ToString(double i);
	static string ToString(float i);
	static string ToPercent(double d);
	static string ToKBytes(double d);

	static string Left(const string& s, unsigned int u);
	static string Right(const string& s, unsigned int u);
	static string& Trim(string& s);
	static string& LeftChomp(string& s, unsigned int uLen = 1);
	static string& RightChomp(string& s, unsigned int uLen = 1);
	static string Token(const string& s, unsigned int uPos, bool bRest = false, char cSep = ' ');
	static string Ellipsize(const string& s, unsigned int uLen);
	static bool wildcmp(const string& sWild, const string& sString);

	static unsigned long long GetMillTime() {
		struct timeval tv;
		unsigned long long iTime = 0;
		gettimeofday(&tv, NULL);
		iTime = (unsigned long long) tv.tv_sec * 1000;
		iTime += ((unsigned long long) tv.tv_usec / 1000);
		return iTime;
	}
#ifdef HAVE_LIBSSL
	static void GenerateCert(FILE *pOut, bool bEncPrivKey = false);
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

	CLockFile(const string& sFile) {
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

	void Open(const string& sFile) {
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
	bool TryExLock(const string& sLockFile, unsigned long long iTimeout = 0) {
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
	string		m_sFileName;
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


class CTable : public vector<map<string, string>* > {
public:
	CTable();
	virtual ~CTable();

	bool AddColumn(const string& sName);
	unsigned int AddRow();
	bool SetCell(const string& sColumn, const string& sValue, unsigned int uRowIdx = ~0);
	bool GetLine(unsigned int uIdx, string& sLine);

	unsigned int GetColumnWidth(unsigned int uIdx);
private:
protected:
	vector<string>				m_vsHeaders;
	map<string, unsigned int>	m_msuWidths;	// Used to cache the width of a column
};


class CFile {
public:
	CFile(const string& sLongName);
	virtual ~CFile();

	enum EFileTypes {
		FT_REGULAR,
		FT_DIRECTORY,
		FT_CHARACTER,
		FT_BLOCK,
		FT_FIFO,
		FT_LINK,
		FT_SOCK
	};

	static bool IsReg(const string& sLongName, bool bUseLstat = false);
	static bool IsDir(const string& sLongName, bool bUseLstat = false);
	static bool IsChr(const string& sLongName, bool bUseLstat = false);
	static bool IsBlk(const string& sLongName, bool bUseLstat = false);
	static bool IsFifo(const string& sLongName, bool bUseLstat = false);
	static bool IsLnk(const string& sLongName, bool bUseLstat = true);
	static bool IsSock(const string& sLongName, bool bUseLstat = false);

	bool IsReg(bool bUseLstat = false);
	bool IsDir(bool bUseLstat = false);
	bool IsChr(bool bUseLstat = false);
	bool IsBlk(bool bUseLstat = false);
	bool IsFifo(bool bUseLstat = false);
	bool IsLnk(bool bUseLstat = true);
	bool IsSock(bool bUseLstat = false);

	bool access(int mode);

	// for gettin file types, using fstat instead
	static bool FType(const string sFileName, EFileTypes eType, bool bUseLstat = false);

	enum EFileAttr {
		FA_Name,
		FA_Size,
		FA_ATime,
		FA_MTime,
		FA_CTime,
		FA_UID
	};

	//
	// Functions to retrieve file information
	//
	bool Exists() const;
	unsigned long long GetSize() const;
	unsigned int GetATime() const;
	unsigned int GetMTime() const;
	unsigned int GetCTime() const;
	int GetUID() const;
	int GetGID() const;
	static bool Exists(const string& sFile);

	static unsigned long long GetSize(const string& sFile);
	static unsigned int GetATime(const string& sFile);
	static unsigned int GetMTime(const string& sFile);
	static unsigned int GetCTime(const string& sFile);
	static int GetUID(const string& sFile);
	static int GetGID(const string& sFile);
	static int GetInfo(const string& sFile, struct stat& st);

	//
	// Functions to manipulate the file on the filesystem
	//
	int Delete();
	int Move(const string& sNewFileName, bool bOverwrite = false);

	static bool Delete(const string& sFileName);
	static bool Move(const string& sOldFileName, const string& sNewFileName, bool bOverwrite = false);
	bool Chmod(mode_t mode);
	static bool Chmod(const string& sFile, mode_t mode);
	bool Seek(unsigned long uPos);
	bool Open(int iFlags, mode_t iMode = 0644);
	int Read(char *pszBuffer, int iBytes);
	bool ReadLine(string & sData);
	int Write(const char *pszBuffer, u_int iBytes);
	int Write(const string & sData);
	void Close();

	string GetLongName() const;
	string GetShortName() const;
	void SetFD(int iFD);

private:
	string	m_sBuffer;
	int		m_iFD;

protected:
	string	m_sLongName;	//!< Absolute filename (m_sPath + "/" + m_sShortName)
	string	m_sShortName;	//!< Filename alone, without path
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
	CBlowfish(const string & sPassword, int iEncrypt, const string & sIvec = "");
	~CBlowfish();

	//! output must be freed
	static unsigned char *MD5(const unsigned char *input, u_int ilen);

	//! returns an md5 of the string (not hex encoded)
	static string MD5(const string & sInput, bool bHexEncode = false);

	//! output must be the same size as input
	void Crypt(unsigned char *input, unsigned char *output, u_int ibytes);

	//! must free result
	unsigned char * Crypt(unsigned char *input, u_int ibytes);
	string Crypt(const string & sData);

private:
	unsigned char		*m_ivec;
	BF_KEY 				m_bkey;
	int					m_iEncrypt, m_num;
};

#endif /* HAVE_LIBSSL */

#define RF_BUFF 4096
inline bool ReadFile(const string & sFilename, string & sLine) {
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

inline bool WriteFile(const string & sFilename, const string & sData) {
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

inline bool ReadLine(const string & sData, string & sLine, u_int & iPos) {
	sLine.clear();

	if (iPos >= sData.length()) {
		return false;
	}

	u_int iFind = sData.find("\n", iPos);

	if (iFind == string::npos) {
		sLine = sData.substr(iPos, (sData.length() - iPos));    
		iPos = string::npos;
		return true;
	}

	sLine = sData.substr(iPos, (iFind - iPos)) + "\n";
	iPos = iFind + 1;

	return true;
}

inline string Lower(const string & sLine) {
	string sRet;
	for(u_int a = 0; a < sLine.length(); a++) {
		sRet += tolower(sLine[a]);
	}

	return sRet;
}

inline string Upper(const string & sLine) {
	string sRet;
	for(u_int a = 0; a < sLine.length(); a++) {
		sRet += toupper(sLine[a]);
	}

	return sRet;
}

#endif // !_UTILS_H

