#ifndef _FILEUTILS_H
#define _FILEUTILS_H

#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/file.h>
#include <dirent.h>
#include <stdio.h>

#include "Utils.h"

#include <string>
#include <vector>
#include <map>
using std::string;
using std::vector;
using std::map;

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

class CDir : public vector<CFile*> {
public:

	CDir(const string& sDir) {
		m_bDesc = false;
		m_eSortAttr = CFile::FA_Name;
		Fill(sDir);
	}

	CDir() {
		m_bDesc = false;
		m_eSortAttr = CFile::FA_Name;
	}

	virtual ~CDir() {
		CleanUp();
	}

	virtual void CleanUp() {
		for (unsigned int a = 0; a < size(); a++) {
			delete (*this)[a];
		}

		clear();
	}

	int Fill(const string& sDir) {
		return FillByWildcard(sDir, "*");
	}

	/*void Sort(CFile::EFileAttr eSortAttr, bool bDesc = false) {
		m_eSortAttr = eSortAttr;
		m_bDesc = bDesc;
		sort(begin(), end(), TPtrCmp<CFile>);
	}*/

	static bool Exists(const string& sDir) {
		CFile cFile(sDir);
		return (cFile.Exists()) && (cFile.IsDir());
	}

/*	static bool Create(const string& sDir, mode_t mode = 0755) {
		VCstring vSubDirs = sDir.split("[/\\\\]+");
		Cstring sCurDir;

		for (unsigned int a = 0; a < vSubDirs.size(); a++) {
			sCurDir += vSubDirs[a] + "/";
			if ((!CDir::Exists(sCurDir)) && (mkdir(sCurDir.c_str(), mode) != 0)) {
				return false;
			}
		}

		return true;
	}

	int FillByRegex(const Cstring& sDir, const Cstring& sRegex, const Cstring& sFlags = "") {
		CleanUp();
		DIR* dir = opendir((sDir.empty()) ? "." : sDir.c_str());

		if (!dir) {
			return 0;
		}

		struct dirent * de;

		while ((de = readdir(dir)) != 0) {
			if ((strcmp(de->d_name, ".") == 0) || (strcmp(de->d_name, "..") == 0)) {
				continue;
			}
			if ((!sRegex.empty()) && (!Cstring::search(de->d_name, sRegex, sFlags))) {
				continue;
			}

			CFile *file = new CFile(sDir, de->d_name, this);
			push_back(file);
		}

		closedir(dir);
		return size();
	}*/

	int FillByWildcard(const string& sDir, const string& sWildcard) {
		CleanUp();
		DIR* dir = opendir((sDir.empty()) ? "." : sDir.c_str());

		if (!dir) {
			return 0;
		}

		struct dirent * de;

		while ((de = readdir(dir)) != 0) {
			if ((strcmp(de->d_name, ".") == 0) || (strcmp(de->d_name, "..") == 0)) {
				continue;
			}
			if ((!sWildcard.empty()) && (!CUtils::wildcmp(sWildcard.c_str(), de->d_name))) {
				continue;
			}

			CFile *file = new CFile(sDir + "/" + de->d_name/*, this*/);	// @todo need to pass pointer to 'this' if we want to do Sort()
			push_back(file);
		}

		closedir(dir);
		return size();
	}

	static unsigned int Chmod(mode_t mode, const string& sWildcard, const string& sDir = ".") {
		CDir cDir;
		cDir.FillByWildcard(sDir, sWildcard);
		return cDir.Chmod(mode);
	}

	unsigned int Chmod(mode_t mode) {
		unsigned int uRet = 0;
		for (unsigned int a = 0; a < size(); a++) {
			if ((*this)[a]->Chmod(mode)) {
				uRet++;
			}
		}

		return uRet;
	}

	static unsigned int Delete(mode_t mode, const string& sWildcard, const string& sDir = ".") {
		CDir cDir;
		cDir.FillByWildcard(sDir, sWildcard);
		return cDir.Delete();
	}

	unsigned int Delete() {
		unsigned int uRet = 0;
		for (unsigned int a = 0; a < size(); a++) {
			if ((*this)[a]->Delete()) {
				uRet++;
			}
		}

		return uRet;
	}

	CFile::EFileAttr GetSortAttr() { return m_eSortAttr; }
	bool IsDescending() { return m_bDesc; }

/*	static bool MkDir(const string & sPath, mode_t iMode, bool bBuildParents = false, bool bApplyModToParents = false) {
		if (sPath.empty()) {
			WARN("empty path!");
			return false;
		}

		if (!bBuildParents) {	// only building target
			mode_t uMask = umask(0000);
			int iRet = mkdir(sPath.c_str(), iMode);
			umask(uMask);

			if (iRet != 0) {
				return false;
			}
		}


		VCstring vPath = sPath.TrimRight_n("/").split("/+");

		if (vPath.empty()) {
			return false;
		}

		if (sPath[0] == '/');
			vPath[0] = "/" + vPath[0];

		Cstring sCurDir = GetCWD();

		mode_t uMask = 0000;
		if (bApplyModToParents) {
			uMask = umask(0000);
		}

		for (unsigned int a = 0; a < (vPath.size() - 1); a++) {
			if ((mkdir(vPath[a].c_str(), iMode) != 0) && (errno != EEXIST)) {
				if (bApplyModToParents) {
					umask(uMask);
				}

				return false;
			}

			if (chdir(vPath[a].c_str()) != 0) {
				chdir(sCurDir.c_str());
				if (bApplyModToParents) {
					umask(uMask);
				}

				return false;
			}
		}

		if (!bApplyModToParents) {
			uMask = umask(0000);
		}

		int iRet = mkdir(vPath[vPath.size() - 1].c_str(), iMode);
		umask(uMask);
		chdir(sCurDir.c_str());
		return (iRet == 0);
	}*/

	static string GetCWD() {
		string sRet;
		char * pszCurDir = getcwd(NULL, 0);
		if (pszCurDir) {
			sRet = pszCurDir;
			free(pszCurDir);
		}

		return sRet;
	}

private:
protected:
	CFile::EFileAttr	m_eSortAttr;
	bool				m_bDesc;
};

#endif // !_FILEUTILS_H
