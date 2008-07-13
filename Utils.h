/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _UTILS_H
#define _UTILS_H

#include "String.h"
#include <assert.h>
#include <map>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

using std::map;
using std::vector;

#ifdef _DEBUG
#define DEBUG_ONLY(f)   f
#else
#define DEBUG_ONLY(f) ((void)0)
#endif

static inline void SetFdCloseOnExec(int fd)
{
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags < 0)
		return; // Ignore errors
	// When we execve() a new process this fd is now automatically closed.
	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static const char g_HexDigits[] = "0123456789abcdef";

class CUtils {
public:
	CUtils();
	virtual ~CUtils();

	static CString GetIP(unsigned long addr);
	static unsigned long GetLongIP(const CString& sIP);
	static void SetStdoutIsTTY(bool b) { stdoutIsTTY = b; }

	static void PrintError(const CString& sMessage);
	static void PrintMessage(const CString& sMessage, bool bStrong = false);
	static void PrintPrompt(const CString& sMessage);
	static void PrintAction(const CString& sMessage);
	static void PrintStatus(bool bSuccess, const CString& sMessage = "");
	static CString GetHashPass();
	static CString GetSaltedHashPass(CString& sSalt, unsigned int uiSaltLength = 20);
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
	static bool stdoutIsTTY;
};

class CLockFile {
public:
	CLockFile() {
		Init();
	}

	CLockFile(const CString& sFile) {
		Init();
		Open(sFile);
	}

	virtual ~CLockFile() {
		Close();
	}

	void Close() {
		if (getpid() == m_pid && m_fd > -1) {
			// This UnLock() also unlocks for all of our childs!
			UnLock();
			close(m_fd);

			if (m_bCreated) {
				unlink(m_sFileName.c_str());
			}
		} else if (m_fd > -1)
			// Make sure we don't leak this fd
			close(m_fd);

		Init();
	}

	bool Open(const CString& sFile, bool bRw = false) {
		Close();

		m_fd = open(sFile.c_str(), bRw ? O_RDWR : O_RDONLY);
		m_bCreated = false;

		if (m_fd == -1) {
			// I must create the file then
			m_fd = open(sFile.c_str(), O_RDWR|O_CREAT, 0644);

			if (m_fd == -1) {
				return false;
			}

			m_bCreated = true;
		}

		// Thanks to broken POSIX, we shouldn't give this fd to anyone
		SetFdCloseOnExec(m_fd);

		m_pid = getpid();       // for destructor
		m_sFileName = sFile;

		return true;
	}

	//! timeout in milliseconds
	bool TryExLock(const CString& sLockFile, bool bRw = false) {
		Open(sLockFile, bRw);
		return TryExLock();
	}

	bool TryExLock() {
		return Lock(LOCK_EX|LOCK_NB);
	}

	bool TryShLock() {
		return(Lock(LOCK_SH|LOCK_NB));
	}

	bool LockEx() { return Lock(LOCK_EX); }
	bool LockSh() { return Lock(LOCK_SH); }
	bool UnLock() { return Lock(LOCK_UN); }

	CString GetFileName() { return m_sFileName; }
	int GetFD() { return m_fd; }

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

	void Init() {
		m_bCreated = false;
		m_fd = -1;
		m_pid = 0;
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
		EX_BadModVersion
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
	 * @param sPassword key to encrypt with
	 * @param iEncrypt encrypt method (BF_DECRYPT or BF_ENCRYPT)
	 * @param sIvec what to set the ivector to start with, default sets it all 0's
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

	while ((bytes = fread(inbuff, sizeof(char), RF_BUFF, f)) > 0) {
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

inline bool ReadLine(const CString & sData, CString & sLine, CString::size_type & iPos) {
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
	for (u_int a = 0; a < sLine.length(); a++) {
		sRet += tolower(sLine[a]);
	}

	return sRet;
}

inline CString Upper(const CString & sLine) {
	CString sRet;
	for (u_int a = 0; a < sLine.length(); a++) {
		sRet += toupper(sLine[a]);
	}

	return sRet;
}

/**
 * @class TCacheMap
 * @author prozac <prozac@rottenboy.com>
 * @brief Insert an object with a time-to-live and check later if it still exists
 */
template<typename T>
class TCacheMap {
public:
	TCacheMap(unsigned int uTTL = 5000) {
		m_uTTL = uTTL;
	}

	virtual ~TCacheMap() {}

	/**
	 * @brief This function adds an item to the cache using the default time-to-live value
	 * @param Item the item to add to the cache
	 */
	void AddItem(const T& Item) {
		AddItem(Item, m_uTTL);
	}

	/**
	 * @brief This function adds an item to the cache using a custom time-to-live value
	 * @param Item the item to add to the cache
	 * @param uTTL the time-to-live for this specific item
	 */
	void AddItem(const T& Item, unsigned int uTTL) {
		if (!uTTL) {			// If time-to-live is zero we don't want to waste our time adding it
			RemItem(Item);		// Remove the item incase it already exists
			return;
		}

		m_mItems[Item] = CUtils::GetMillTime() + uTTL;
		Cleanup();
	}

	/**
	 * @brief Performs a Cleanup() and then checks to see if your item exists
	 * @param Item The item to check for
	 * @return true if item exists
	 */
	bool HasItem(const T& Item) {
		Cleanup();
		return (m_mItems.find(Item) != m_mItems.end());
	}

	/**
	 * @brief Removes a specific item from the cache
	 * @param Item The item to be removed
	 * @return true if item existed and was removed, false if it never existed
	 */
	bool RemItem(const T& Item) {
		return m_mItems.erase(Item);
	}

	/**
	 * @brief Cycles through the queue removing all of the stale entries
	 */
	void Cleanup() {
		typename map<T, unsigned long long>::iterator it = m_mItems.begin();

		while (it != m_mItems.end()) {
			if (CUtils::GetMillTime() > (it->second)) {
				m_mItems.erase(it++);
			} else {
				++it;
			}
		}
	}

	// Setters
	void SetTTL(unsigned int u) { m_uTTL = u; }
	// !Setters
private:
	map<T, unsigned long long>	m_mItems;	//!< Map of cached items.  The value portion of the map is for the expire time
	unsigned int	m_uTTL;					//!< Default time-to-live duration
};

/**
 * @class CNoCopy
 * @author prozac <prozac@rottenboy.com>
 * @brief This class is intended to be derived from to prevent copying of your derived class, the implementations of the copy constructor and equals operator were intentionally made private and omitted
 */
class CNoCopy {
protected:
	CNoCopy() {}	// Allow construction
	~CNoCopy() {}	// and destruction of derived objects
private:
	CNoCopy(const CNoCopy&);				// Disallow copying
	CNoCopy& operator =(const CNoCopy&);	// and assignment
};

/**
 * @class CSafePtr
 * @author prozac <prozac@rottenboy.com>
 * @brief Safe deletion of a pointer.
 *
 * This class is intended to be created on the stack and hold a pointer which
 * will be deleted upon destruction.  It is useful for functions where you need
 * an allocated pointer and have many return paths.  It prevents copying to get
 * around the exclusive ownership situation which makes std::auto_ptr
 * invalidate the first pointer on copy.  This is intended to be used in
 * simplistic situations such as local variables.
 */
template<typename T>
class CSafePtr : private CNoCopy {
public:
	CSafePtr() { m_pType = new T; }
	CSafePtr(T* p) { m_pType = p; }
	virtual ~CSafePtr() { delete m_pType; }

	T& operator *() const { assert(m_pType); return *m_pType; }
	T* operator ->() const { assert(m_pType); return m_pType; }
private:
	operator T() const;
	T*	m_pType;
};

/**
 * @class CSmartPtr
 * @author prozac <prozac@rottenboy.com>
 * @brief This is a standard reference counting pointer.  Be careful not to have two of these point to the same raw pointer or one will be deleted while the other still thinks it is valid.
 */
template<typename T>
class CSmartPtr {
public:
	/**
	 * @brief Standard constructor, points to nothing
	 */
	CSmartPtr() {
		m_pType = NULL;
		m_puCount = NULL;
	}

	/**
	 * @brief Attach to an existing raw pointer, be CAREFUL not to have more than one CSmartPtr attach to the same raw pointer or bad things will happen
	 * @param pRawPtr The raw pointer to attach to
	 */
	CSmartPtr(T* pRawPtr) {
		m_pType = NULL;
		m_puCount = NULL;

		Attach(pRawPtr);
	}

	/**
	 * @brief Copy constructor, will copy the raw pointer and counter locations and increment the reference counter
	 * @param CopyFrom A reference of another CSmartPtr to copy from
	 */
	CSmartPtr(const CSmartPtr<T>& CopyFrom) {
		m_pType = NULL;
		m_puCount = NULL;

		*this = CopyFrom;
	}

	/**
	 * @brief Destructor will Release() the raw pointer and delete it if this was the last reference
	 */
	~CSmartPtr() {
		Release();
	}

	// Overloaded operators
	T& operator *() const { assert(m_pType); return *m_pType; }
	T* operator ->() const { assert(m_pType); return m_pType; }
	bool operator ==(T* rhs) { return (m_pType == rhs); }
	bool operator ==(const CSmartPtr<T>& rhs) { return (m_pType == *rhs); }

	/**
	 * @brief Attach() to a raw pointer
	 * @param p The raw pointer to keep track of, ***WARNING*** Do _NOT_ allow more than one CSmartPtr keep track of the same raw pointer
	 * @return Reference to self
	 */
	CSmartPtr<T>& operator =(T* p) { Attach(p); return *this; }

	/**
	 * @brief Copies an existing CSmartPtr adding another reference to the counter
	 * @param CopyFrom A reference to another CSmartPtr to be copied
	 * @return Reference to self
	 */
	CSmartPtr<T>& operator =(const CSmartPtr<T>& CopyFrom) {
		if (&CopyFrom != this) {				// Check for assignment to self
			Release();							// Release the current pointer

			if (CopyFrom.IsNull()) {			// If the source raw pointer is null
				return *this;					// Then just bail out
			}

			m_pType = &(*CopyFrom);				// Make our pointers reference the same raw pointer and counter
			m_puCount = CopyFrom.GetCount();

			if (m_pType) {						// If we now point to something valid, increment the counter
				assert(m_puCount);
				(*m_puCount)++;
			}
		}

		return *this;
	}
	// !Overloaded operators

	/**
	 * @brief Check to see if the underlying raw pointer is null
	 * @return Whether or not underlying raw pointer is null
	 */
	bool IsNull() const {
		return (m_pType == NULL);
	}

	/**
	 * @brief Attach to a given raw pointer, it will Release() the current raw pointer and assign the new one
	 * @param pRawPtr The raw pointer to keep track of, ***WARNING*** Do _NOT_ allow more than one CSmartPtr keep track of the same raw pointer
	 * @return Reference to self
	 */
	CSmartPtr<T>& Attach(T* pRawPtr) {
		assert(pRawPtr);

		if (pRawPtr != m_pType) {					// Check for assignment to self
			Release();								// Release the current pointer
			m_pType = pRawPtr;						// Point to the passed raw pointer

			if (m_pType) {							// If the passed pointer was valid
				m_puCount = new unsigned int(1);	// Create a new counter starting at 1 (us)
			}
		}

		return *this;
	}

	/**
	 * @brief Releases the underlying raw pointer and cleans up if we were the last reference to said pointer
	 */
	void Release() {
		if (m_pType) {				// Only release if there is something to be released
			assert(m_puCount);
			(*m_puCount)--;			// Decrement our counter

			if (!*m_puCount) {		// If we were the last reference to this pointer, then clean up
				delete m_puCount;
				delete m_pType;
			}

			m_pType = NULL;			// Get rid of our references
			m_puCount = NULL;
		}
	}

	// Getters
	T* GetPtr() const { return m_pType; }
	unsigned int* GetCount() const { return m_puCount; }
	// !Getters
private:
	T*				m_pType;	//!< Raw pointer to the class being referenced
	unsigned int*	m_puCount;	//!< Counter of how many CSmartPtr's are referencing the same raw pointer
};

#endif // !_UTILS_H

