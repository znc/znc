/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "FileUtils.h"
#include "ExecSock.h"
#include "Utils.h"
#include "ZNCDebug.h"
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef HAVE_LSTAT
#  define lstat(a, b)	stat(a, b)
#endif

#ifndef O_BINARY
#  define O_BINARY	0
#endif

CString CFile::m_sHomePath;

CFile::CFile() {
	m_iFD = -1;
	ResetError();
}

CFile::CFile(const CString& sLongName) {
	m_iFD = -1;

	ResetError();
	SetFileName(sLongName);
}

CFile::~CFile() {
	Close();
}

void CFile::SetFileName(const CString& sLongName) {
	if (sLongName.Left(2) == "~/") {
		m_sLongName = CFile::GetHomePath() + sLongName.substr(1);
	} else
		m_sLongName = sLongName;

	m_sShortName = sLongName;
	m_sShortName.TrimRight("/");

	CString::size_type uPos = m_sShortName.rfind('/');
	if (uPos != CString::npos) {
		m_sShortName = m_sShortName.substr(uPos +1);
	}
}

bool CFile::IsDir(const CString& sLongName, bool bUseLstat) {
	if (sLongName.Equals("/"))
		return CFile::FType(sLongName, FT_DIRECTORY, bUseLstat);

	// Some OS don't like trailing slashes for directories
	return CFile::FType(sLongName.TrimRight_n("/"),
			FT_DIRECTORY, bUseLstat);
}

bool CFile::IsReg(const CString& sLongName, bool bUseLstat) { return CFile::FType(sLongName, FT_REGULAR, bUseLstat); }
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

// for gettin file types, using fstat instead
bool CFile::FType(const CString& sFileName, EFileTypes eType, bool bUseLstat) {
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
			break;
	}
	return false;
}

//
// Functions to retrieve file information
//
bool CFile::Exists() const { return CFile::Exists(m_sLongName); }
off_t CFile::GetSize() const { return CFile::GetSize(m_sLongName); }
time_t CFile::GetATime() const { return CFile::GetATime(m_sLongName); }
time_t CFile::GetMTime() const { return CFile::GetMTime(m_sLongName); }
time_t CFile::GetCTime() const { return CFile::GetCTime(m_sLongName); }
uid_t CFile::GetUID() const { return CFile::GetUID(m_sLongName); }
gid_t CFile::GetGID() const { return CFile::GetGID(m_sLongName); }
bool CFile::Exists(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) == 0);
}

off_t CFile::GetSize(const CString& sFile) {
	struct stat st;
	if (stat(sFile.c_str(), &st) != 0) {
		return 0;
	}

	return (S_ISREG(st.st_mode)) ? st.st_size : 0;
}

time_t CFile::GetATime(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_atime;
}

time_t CFile::GetMTime(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_mtime;
}

time_t CFile::GetCTime(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_ctime;
}

uid_t CFile::GetUID(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? -1 : (int) st.st_uid;
}

gid_t CFile::GetGID(const CString& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? -1 : (int) st.st_gid;
}
int CFile::GetInfo(const CString& sFile, struct stat& st) {
	return stat(sFile.c_str(), &st);
}

//
// Functions to manipulate the file on the filesystem
//
bool CFile::Delete() {
	if (CFile::Delete(m_sLongName))
		return true;
	m_bHadError = true;
	return false;
}

bool CFile::Move(const CString& sNewFileName, bool bOverwrite) {
	if (CFile::Move(m_sLongName, sNewFileName, bOverwrite))
		return true;
	m_bHadError = true;
	return false;
}

bool CFile::Copy(const CString& sNewFileName, bool bOverwrite) {
	if (CFile::Copy(m_sLongName, sNewFileName, bOverwrite))
		return true;
	m_bHadError = true;
	return false;
}

bool CFile::Delete(const CString& sFileName) {
	if (!CFile::Exists(sFileName)) {
		return false;
	}

	return (unlink(sFileName.c_str()) == 0) ? true : false;
}

bool CFile::Move(const CString& sOldFileName, const CString& sNewFileName, bool bOverwrite) {
	if ((!bOverwrite) && (CFile::Exists(sNewFileName))) {
		return false;
	}

	return (rename(sOldFileName.c_str(), sNewFileName.c_str()) == 0);
}

bool CFile::Copy(const CString& sOldFileName, const CString& sNewFileName, bool bOverwrite) {
	if ((!bOverwrite) && (CFile::Exists(sNewFileName))) {
		return false;
	}

	CFile OldFile(sOldFileName);
	CFile NewFile(sNewFileName);

	if (!OldFile.Open()) {
		return false;
	}

	if (!NewFile.Open(O_WRONLY | O_CREAT | O_TRUNC)) {
		return false;
	}

	char szBuf[8192];
	int len = 0;

	while ((len = OldFile.Read(szBuf, 8192))) {
		if (len < 0) {
			DEBUG("CFile::Copy() failed: " << strerror(errno));
			OldFile.Close();

			// That file is only a partial copy, get rid of it
			NewFile.Close();
			NewFile.Delete();

			return false;
		}
		NewFile.Write(szBuf, len);
	}

	OldFile.Close();
	NewFile.Close();

	return true;
}

bool CFile::Chmod(mode_t mode) {
	if (m_iFD == -1) {
		return false;
	}
	if (fchmod(m_iFD, mode) != 0) {
		m_bHadError = true;
		return false;
	}
	return true;
}

bool CFile::Chmod(const CString& sFile, mode_t mode) {
	return (chmod(sFile.c_str(), mode) == 0);
}

bool CFile::Seek(off_t uPos) {
	if (m_iFD != -1 && lseek(m_iFD, uPos, SEEK_SET) == uPos) {
		ClearBuffer();
		return true;
	}

	m_bHadError = true;

	return false;
}

bool CFile::Truncate() {
	if (m_iFD != -1 && ftruncate(m_iFD, 0) == 0) {
		ClearBuffer();
		return true;
	}

	m_bHadError = true;

	return false;
}

bool CFile::Sync() {
	if (m_iFD != -1 && fsync(m_iFD) == 0)
		return true;
	m_bHadError = true;
	return false;
}

bool CFile::Open(const CString& sFileName, int iFlags, mode_t iMode) {
	SetFileName(sFileName);
	return Open(iFlags, iMode);
}

bool CFile::Open(int iFlags, mode_t iMode) {
	if (m_iFD != -1) {
		m_bHadError = true;
		return false;
	}

	// We never want to get a controlling TTY through this -> O_NOCTTY
	iMode |= O_NOCTTY;

	// Some weird OS from MS needs O_BINARY or else it generates fake EOFs
	// when reading ^Z from a file.
	iMode |= O_BINARY;

	m_iFD = open(m_sLongName.c_str(), iFlags, iMode);
	if (m_iFD < 0) {
		m_bHadError = true;
		return false;
	}

	/* Make sure this FD isn't given to childs */
	SetFdCloseOnExec(m_iFD);

	return true;
}

int CFile::Read(char *pszBuffer, int iBytes) {
	if (m_iFD == -1) {
		return -1;
	}

	int res = read(m_iFD, pszBuffer, iBytes);
	if (res != iBytes)
		m_bHadError = true;
	return res;
}

bool CFile::ReadLine(CString& sData, const CString & sDelimiter) {
	char buff[4096];
	int iBytes;

	if (m_iFD == -1) {
		return false;
	}

	do {
		CString::size_type iFind = m_sBuffer.find(sDelimiter);
		if (iFind != CString::npos) {
			// We found a line, return it
			sData = m_sBuffer.substr(0, iFind + sDelimiter.length());
			m_sBuffer.erase(0, iFind + sDelimiter.length());
			return true;
		}

		iBytes = read(m_iFD, buff, sizeof(buff));

		if (iBytes > 0) {
			m_sBuffer.append(buff, iBytes);
		}
	} while (iBytes > 0);

	// We are at the end of the file or an error happened

	if (!m_sBuffer.empty()) {
		// ..but there is still some partial line in the buffer
		sData = m_sBuffer;
		m_sBuffer.clear();
		return true;
	}

	// Nothing left for reading :(
	return false;
}

bool CFile::ReadFile(CString& sData, size_t iMaxSize) {
	char buff[4096];
	size_t iBytesRead = 0;

	sData.clear();

	while (iBytesRead < iMaxSize) {
		int iBytes = Read(buff, sizeof(buff));

		if (iBytes < 0)
			// Error
			return false;

		if (iBytes == 0)
			// EOF
			return true;

		sData.append(buff, iBytes);
		iBytesRead += iBytes;
	}

	// Buffer limit reached
	return false;
}

int CFile::Write(const char *pszBuffer, u_int iBytes) {
	if (m_iFD == -1) {
		return -1;
	}

	u_int res = write(m_iFD, pszBuffer, iBytes);
	if (res != iBytes)
		m_bHadError = true;
	return res;
}

int CFile::Write(const CString & sData) {
	return Write(sData.data(), sData.size());
}
void CFile::Close() {
	if (m_iFD >= 0) {
		if (close(m_iFD) < 0) {
			m_bHadError = true;
			DEBUG("CFile::Close(): close() failed with ["
					<< strerror(errno) << "]");
		}
	}
	m_iFD = -1;
	ClearBuffer();
}
void CFile::ClearBuffer() { m_sBuffer.clear(); }

bool CFile::TryExLock(const CString& sLockFile, int iFlags) {
	Open(sLockFile, iFlags);
	return TryExLock();
}

bool CFile::TryExLock() {
	return Lock(F_WRLCK, false);
}

bool CFile::ExLock() {
	return Lock(F_WRLCK, true);
}

bool CFile::UnLock() {
	return Lock(F_UNLCK, true);
}

bool CFile::Lock(int iType, bool bBlocking) {
	struct flock fl;

	if (m_iFD == -1) {
		return false;
	}

	fl.l_type   = iType;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 0;
	return (fcntl(m_iFD, (bBlocking ? F_SETLKW : F_SETLK), &fl) != -1);
}

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

void CFile::InitHomePath(const CString& sFallback) {
	const char *home = getenv("HOME");

	m_sHomePath.clear();
	if (home) {
		m_sHomePath = home;
	}

	if (m_sHomePath.empty()) {
		const struct passwd* pUserInfo = getpwuid(getuid());

		if (pUserInfo) {
			m_sHomePath = pUserInfo->pw_dir;
		}
	}

	if (m_sHomePath.empty()) {
		m_sHomePath = sFallback;
	}
}

CString CDir::ChangeDir(const CString& sPath, const CString& sAdd, const CString& sHome) {
	CString sHomeDir(sHome);

	if (sHomeDir.empty()) {
		sHomeDir = CFile::GetHomePath();
	}

	if (sAdd == "~") {
		return sHomeDir;
	}

	CString sAddDir(sAdd);

	if (sAddDir.Left(2) == "~/") {
		sAddDir.LeftChomp();
		sAddDir = sHomeDir + sAddDir;
	}

	CString sRet = ((sAddDir.size()) && (sAddDir[0] == '/')) ? "" : sPath;
	sAddDir += "/";
	CString sCurDir;

	if (sRet.Right(1) == "/") {
		sRet.RightChomp();
	}

	for (unsigned int a = 0; a < sAddDir.size(); a++) {
		switch (sAddDir[a]) {
			case '/':
				if (sCurDir == "..") {
					sRet = sRet.substr(0, sRet.rfind('/'));
				} else if ((sCurDir != "") && (sCurDir != ".")) {
					sRet += "/" + sCurDir;
				}

				sCurDir = "";
				break;
			default:
				sCurDir += sAddDir[a];
				break;
		}
	}

	return (sRet.empty()) ? "/" : sRet;
}

CString CDir::CheckPathPrefix(const CString& sPath, const CString& sAdd, const CString& sHomeDir) {
	CString sPrefix = sPath.Replace_n("//", "/").TrimRight_n("/") + "/";
	CString sAbsolutePath = ChangeDir(sPrefix, sAdd, sHomeDir);

	if (sAbsolutePath.Left(sPrefix.length()) != sPrefix)
		return "";
	return sAbsolutePath;
}

bool CDir::MakeDir(const CString& sPath, mode_t iMode) {
	CString sDir;
	VCString dirs;
	VCString::iterator it;

	// Just in case someone tries this...
	if (sPath.empty())
		return false;

	// If this is an absolute path, we need to handle this now!
	if (sPath.Left(1) == "/")
		sDir = "/";

	// For every single subpath, do...
	sPath.Split("/", dirs, false);
	for (it = dirs.begin(); it != dirs.end(); ++it) {
		// Add this to the path we already created
		sDir += *it;

		int i = mkdir(sDir.c_str(), iMode);

		if (i != 0) {
			// All errors except EEXIST are fatal
			if (errno != EEXIST)
				return false;

			// If it's EEXIST we have to make sure it's a dir
			if (!CFile::IsDir(sDir))
				return false;
		}

		sDir += "/";
	}

	// All went well
	return true;
}

int CExecSock::popen2(int & iReadFD, int & iWriteFD, const CString & sCommand) {
	int rpipes[2] = { -1, -1 };
	int wpipes[2] = { -1, -1 };
	iReadFD = -1;
	iWriteFD = -1;

	if (pipe(rpipes) < 0)
		return -1;

	if (pipe(wpipes) < 0) {
		close(rpipes[0]);
		close(rpipes[1]);
		return -1;
	}

	int iPid = fork();

	if (iPid == -1) {
		close(rpipes[0]);
		close(rpipes[1]);
		close(wpipes[0]);
		close(wpipes[1]);
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
		const char * pArgv[] =
		{
			"sh",
			"-c",
			sCommand.c_str(),
			NULL
		};
		execvp("sh", (char * const *) pArgv);
		// if execvp returns, there was an error
		perror("execvp");
		exit(1);
	}

	close(wpipes[0]);
	close(rpipes[1]);

	iWriteFD = wpipes[1];
	iReadFD = rpipes[0];

	return iPid;
}

void CExecSock::close2(int iPid, int iReadFD, int iWriteFD) {
	close(iReadFD);
	close(iWriteFD);
	u_int iNow = time(NULL);
	while (waitpid(iPid, NULL, WNOHANG) == 0) {
		if ((time(NULL) - iNow) > 5)
			break;  // giveup
		usleep(100);
	}
	return;
}
