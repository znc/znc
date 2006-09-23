//! @author prozac@rottenboy.com

#include "FileUtils.h"
#include <iostream>
#include <sys/wait.h>

using std::cout;
using std::cerr;
using std::endl;

CFile::CFile() {
	m_iFD = -1;
}

CFile::CFile(const CString& sLongName) {
	m_iFD = -1;

	SetFileName(sLongName);
}

CFile::~CFile() {
	if (m_iFD != -1) {
		Close();
	}
}

void CFile::SetFileName(const CString& sLongName) {
	m_sLongName = sLongName;
	m_iFD = -1;

	m_sShortName = sLongName;
	m_sShortName.TrimRight("/");

	CString::size_type uPos = m_sShortName.rfind('/');
	if (uPos != CString::npos) {
		m_sShortName = m_sShortName.substr(uPos +1);
	}
}

bool CFile::IsReg(const CString& sLongName, bool bUseLstat) { return CFile::FType(sLongName, FT_REGULAR, bUseLstat); }
bool CFile::IsDir(const CString& sLongName, bool bUseLstat) { return CFile::FType(sLongName, FT_DIRECTORY, bUseLstat); }
bool CFile::IsChr(const CString& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_CHARACTER, bUseLstat); }
bool CFile::IsBlk(const CString& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_BLOCK, bUseLstat); }
bool CFile::IsFifo(const CString& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_FIFO, bUseLstat); }
bool CFile::IsLnk(const CString& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_LINK, bUseLstat); }
bool CFile::IsSock(const CString& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_SOCK, bUseLstat); }

bool CFile::IsReg(bool bUseLstat) const { return CFile::IsReg(m_sLongName, bUseLstat); }
bool CFile::IsDir(bool bUseLstat) const { return CFile::IsDir(m_sLongName, bUseLstat); }
bool CFile::IsChr(bool bUseLstat) const { return CFile::IsChr(m_sLongName, bUseLstat); }
bool CFile::IsBlk(bool bUseLstat) const { return CFile::IsBlk(m_sLongName, bUseLstat); }
bool CFile::IsFifo(bool bUseLstat) const { return CFile::IsFifo(m_sLongName, bUseLstat); }
bool CFile::IsLnk(bool bUseLstat) const { return CFile::IsLnk(m_sLongName, bUseLstat); }
bool CFile::IsSock(bool bUseLstat) const { return CFile::IsSock(m_sLongName, bUseLstat); }

bool CFile::access(int mode) { return (::access(m_sLongName.c_str(), mode) == 0); }

// for gettin file types, using fstat instead
bool CFile::FType(const CString sFileName, EFileTypes eType, bool bUseLstat) {
	struct stat st;

	if (!bUseLstat) {
		if (stat(sFileName.c_str(), &st) != 0) {
			return false;
		}
	} else {
		if (lstat(sFileName.c_str(), &st) != 0) {
			return false;
		}
	}

	switch (eType) {
		case FT_REGULAR:
			return S_ISREG(st.st_mode);
		case FT_DIRECTORY:
			return S_ISDIR(st.st_mode);
		case FT_CHARACTER:
			return S_ISCHR(st.st_mode);
		case FT_BLOCK:
			return S_ISBLK(st.st_mode);
		case FT_FIFO:
			return S_ISFIFO(st.st_mode);
		case FT_LINK:
			return S_ISLNK(st.st_mode);
		case FT_SOCK:
			return S_ISSOCK(st.st_mode);
		default:
			return false;
	}
	return false;
}

//
// Functions to retrieve file information
//
bool CFile::Exists() const { return CFile::Exists(m_sLongName); }
unsigned long long CFile::GetSize() const { return CFile::GetSize(m_sLongName); }
unsigned int CFile::GetATime() const { return CFile::GetATime(m_sLongName); }
unsigned int CFile::GetMTime() const { return CFile::GetMTime(m_sLongName); }
unsigned int CFile::GetCTime() const { return CFile::GetCTime(m_sLongName); }
int CFile::GetUID() const { return CFile::GetUID(m_sLongName); }
int CFile::GetGID() const { return CFile::GetGID(m_sLongName); }
bool CFile::Exists(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) == 0);
}

unsigned long long CFile::GetSize(const CString& sFile) {
	struct stat st;
	if(stat(sFile.c_str(), &st) != 0) {
		return 0;
	}

	return (S_ISREG(st.st_mode)) ? st.st_size : 0;
}

unsigned int CFile::GetATime(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_atime;
}

unsigned int CFile::GetMTime(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_mtime;
}

unsigned int CFile::GetCTime(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_ctime;
}

int CFile::GetUID(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? -1 : (int) st.st_uid;
}

int CFile::GetGID(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? -1 : (int) st.st_gid;
}
int CFile::GetInfo(const CString& sFile, struct stat& st) {
	return stat(sFile.c_str(), &st);
}

//
// Functions to manipulate the file on the filesystem
//
int CFile::Delete() { return CFile::Delete(m_sLongName); }
int CFile::Move(const CString& sNewFileName, bool bOverwrite) {
	return CFile::Move(m_sLongName, sNewFileName, bOverwrite);
}

int CFile::Copy(const CString& sNewFileName, bool bOverwrite) {
	return CFile::Copy(m_sLongName, sNewFileName, bOverwrite);
}

bool CFile::Delete(const CString& sFileName) {
	if(!CFile::Exists(sFileName)) {
		return false;
	}

	return (unlink(sFileName.c_str()) == 0) ? true : false;
}

bool CFile::Move(const CString& sOldFileName, const CString& sNewFileName, bool bOverwrite) {
	if((!bOverwrite) && (CFile::Exists(sNewFileName))) {
		return false;
	}

	//CString sNewLongName = (sNewFileName[0] == '/') ? sNewFileName : m_sPath + "/" + sNewFileName;
	return (rename(sOldFileName.c_str(), sNewFileName.c_str()) == 0) ? true : false;
}

bool CFile::Copy(const CString& sOldFileName, const CString& sNewFileName, bool bOverwrite) {
	if((!bOverwrite) && (CFile::Exists(sNewFileName))) {
		return false;
	}

	CFile OldFile(sOldFileName);
	CFile NewFile(sNewFileName);

	if (!OldFile.Open(O_RDONLY)) {
		return false;
	}

	if (!NewFile.Open(O_WRONLY | O_CREAT | O_TRUNC)) {
		return false;
	}

	char szBuf[8192];
	unsigned int len = 0;

	while ((len = OldFile.Read(szBuf, 8192))) {
		NewFile.Write(szBuf, len);
	}

	OldFile.Close();
	NewFile.Close();

	return true;
}

bool CFile::Chmod(mode_t mode) {
	return CFile::Chmod(m_sLongName, mode);
}

bool CFile::Chmod(const CString& sFile, mode_t mode) {
	return (chmod(sFile.c_str(), mode) == 0);
}

bool CFile::Seek(unsigned long uPos) {
	if (m_iFD != -1 && (unsigned int) lseek(m_iFD, uPos, SEEK_SET) == uPos) {
		ClearBuffer();
		return true;
	}

	return false;
}

bool CFile::Open(const CString& sFileName, int iFlags, mode_t iMode) {
	SetFileName(sFileName);
	return Open(iFlags, iMode);
}

bool CFile::Open(int iFlags, mode_t iMode) {
	if (m_iFD != -1) {
		return false;
	}

	m_iFD = open(m_sLongName.c_str(), iFlags, iMode);
	return  (m_iFD > -1);
}

int CFile::Read(char *pszBuffer, int iBytes) {
	if (m_iFD == -1) {
		return -1;
	}

	return read(m_iFD, pszBuffer, iBytes);
}

bool CFile::ReadLine(CString& sData, const CString & sDelimiter) {
	char buff[64];
	sData.clear();

	if (m_iFD == -1) {
		return false;
	}

	bool bEOF = false;

	while(true) {
		CString::size_type iFind = m_sBuffer.find(sDelimiter);
		if (iFind != CString::npos) {
			sData = m_sBuffer.substr(0, (iFind + 1));
			m_sBuffer.erase(0, (iFind + 1));
			break;
		}

		memset((char *)buff, '\0', 64);
		int iBytes = read(m_iFD, buff, 64);

		switch(iBytes) {
			case -1: {
				bEOF = true;
				break;
			}
			case 0: {
				bEOF = true;
				break;
			}
			default: {
				m_sBuffer.append(buff, iBytes);
				break;
			}
		}

		if (bEOF) {
			break;
		}
	}

	CString::size_type iFind = m_sBuffer.find(sDelimiter);
	if (iFind != CString::npos) {
		return true;
	}

	if( bEOF ) {
		sData = m_sBuffer;
		m_sBuffer.clear();
		break;
	}

	return !bEOF;
}

int CFile::Write(const char *pszBuffer, u_int iBytes) {
	if (m_iFD == -1) {
		return -1;
	}

	return write(m_iFD, pszBuffer, iBytes);
}

int CFile::Write(const CString & sData) {
	return Write(sData.data(), sData.size());
}
void CFile::Close() {
	if (m_iFD >= 0) {
		close(m_iFD); m_iFD = -1;
	}
}
void CFile::ClearBuffer() { m_sBuffer.clear(); }

bool CFile::IsOpen() const { return (m_iFD != -1); }
CString CFile::GetLongName() const { return m_sLongName; }
CString CFile::GetShortName() const { return m_sShortName; }
CString CFile::GetDir() const {
	CString sDir(m_sLongName);

	while (!sDir.empty() && sDir.Right(1) != "/" && sDir.Right(1) != "\\") {
		sDir.RightChomp();
	}

	return sDir;
}

void CFile::SetFD(int iFD) { m_iFD = iFD; }

int CExecSock::popen2(int & iReadFD, int & iWriteFD, const CString & sCommand) {
	int rpipes[2] = { -1, -1 };
	int wpipes[2] = { -1, -1 };
	iReadFD = -1;
	iWriteFD = -1;

	pipe(rpipes);
	pipe(wpipes);
	
	int iPid = fork();

	if (iPid == -1) {
		return -1;
	}

	if (iPid == 0) {
		close(wpipes[1]);
		close(rpipes[0]);
		dup2(wpipes[0], 0);
		dup2(rpipes[1], 1);
		dup2(rpipes[1], 2);
		close(wpipes[0]);
		close(rpipes[1]);
		char * const pArgv[] =
		{
			"sh",
			"-c",
			(char *)sCommand.c_str(),
			NULL
		};
		execvp( "sh", pArgv );
		exit(0);
	}

	close(wpipes[0]);
	close(rpipes[1]);

	iWriteFD = wpipes[1];
	iReadFD = rpipes[0];

	return iPid;
}

void CExecSock::close2(int iPid, int iReadFD, int iWriteFD) {
	close( iReadFD );
	close( iWriteFD );
	u_int iNow = time( NULL );
	while( waitpid( iPid, NULL, WNOHANG ) == 0 )
	{
		if ( ( time( NULL ) - iNow ) > 5 )
			break;	// giveup
		usleep( 100 );
	}
	return;
}
