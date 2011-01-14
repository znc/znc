/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include <iconv.h>

#ifndef ICONV_CONST
/* configure is supposed to define this, depending on whether the second
	argument to iconv is const char** or just char**, but if it isn't defined,
	we default to GNU/Linux which is non-const. */
#define ICONV_CONST
#endif

class CCharsetMod : public CModule
{
private:
	VCString m_vsClientCharsets;
	VCString m_vsServerCharsets;
	bool m_bForce; // don't check whether the input string is already a
	// valid string in the target charset. Instead, always apply conversion.

	size_t GetConversionLength(iconv_t& ic, const CString& sData)
	{
		if(sData.empty()) return 0;

		size_t uLength = 0;
		char tmpbuf[1024];
		const char *pIn = sData.c_str();
		size_t uInLen = sData.size();
		bool bBreak;

		do
		{
			char *pOut = tmpbuf;
			size_t uBufSize = 1024;
			bBreak = (uInLen < 1);

			if(iconv(ic, // this is ugly, but keeps the code short:
				(uInLen < 1 ? NULL : (ICONV_CONST char**)&pIn),
				(uInLen < 1 ? NULL : &uInLen),
				&pOut, &uBufSize) == (size_t)-1)
				// explanation: iconv needs a last call with input = NULL to
				// copy/convert possibly left data chunks into the output buffer.
			{
				if(errno == EINVAL)
				{
					// charset is not what we think it is.
					return (size_t)-1;
				}
				else if(errno != E2BIG)
				{
					// something bad happened, internal error.
					return (size_t)-2;
				}
			}

			uLength += (pOut - tmpbuf);
		} while(!bBreak);

		return uLength;
	}

	bool ConvertCharset(const CString& sFrom, const CString& sTo, CString& sData)
	{
		if(sData.empty()) return true;

		DEBUG("charset: Trying to convert [" + sData.Escape_n(CString::EURL) + "] from [" + sFrom + "] to [" + sTo + "]...");

		iconv_t ic = iconv_open(sTo.c_str(), sFrom.c_str());
		if(ic == (iconv_t)-1) return false;

		size_t uLength = GetConversionLength(ic, sData);

		if(uLength == (size_t)-1)
		{
			// incompatible input encoding.
			iconv_close(ic);
			return false;
		}
		else if(uLength == (size_t)-2)
		{
			// internal error, preserve errno from GetConversionLength:
			int tmp_errno = errno;
			iconv_close(ic);
			errno = tmp_errno;
			return false;
		}
		else
		{
			// no error, so let's do the actual conversion.

			iconv(ic, NULL, NULL, NULL, NULL); // reset

			// state vars for iconv:
			size_t uResultBufSize = uLength + 1;
			char *pResult = new char[uResultBufSize];
			memset(pResult, 0, uResultBufSize);
			char *pResultWalker = pResult;
			const char* pIn = sData.c_str();
			size_t uInLen = sData.size();

			// let's fcking do it!
			size_t uResult = iconv(ic, (ICONV_CONST char**)&pIn, &uInLen, &pResultWalker, &uResultBufSize);
			bool bResult = (uResult != (size_t)-1);

			iconv_close(ic);

			if(bResult)
			{
				sData.assign(pResult, uLength);

				DEBUG("charset: Converted: [" + sData.Escape_n(CString::EURL) + "] from [" + sFrom + "] to [" + sTo + "]!");
			}
			else
			{
				DEBUG("Conversion failed: [" << uResult << "]");
			}

			delete[] pResult;

			return bResult;
		}
	}

	bool ConvertCharset(const VCString& vsFrom, const CString& sTo, CString& sData)
	{
		CString sDataCopy(sData);

		if(!m_bForce)
		{
			// check whether sData already is encoded with the right charset:
			iconv_t icTest = iconv_open(sTo.c_str(), sTo.c_str());
			if(icTest != (iconv_t)-1)
			{
				size_t uTest = GetConversionLength(icTest, sData);
				iconv_close(icTest);

				if(uTest != (size_t)-1 && uTest != (size_t)-2)
				{
					DEBUG("charset: [" + sData.Escape_n(CString::EURL) + "] is valid [" + sTo + "] already.");
					return true;
				}
			}
		}

		bool bConverted = false;

		// try all possible source charsets:
		for(VCString::const_iterator itf = vsFrom.begin(); itf != vsFrom.end(); itf++)
		{
			if(ConvertCharset(*itf, sTo, sDataCopy))
			{
				// conversion successful!
				sData = sDataCopy;
				bConverted = true;
				break;
			}
			else
			{
				// reset string and try the next charset:
				sDataCopy = sData;
			}
		}

		return bConverted;
	}

public:
	MODCONSTRUCTOR(CCharsetMod)
	{
		m_bForce = false;
	}

	bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		size_t uIndex = 0;

		if(sArgs.Token(0).Equals("-force"))
		{
			m_bForce = true;
			++uIndex;
		}

		if(sArgs.Token(uIndex + 1).empty() || !sArgs.Token(uIndex + 2).empty())
		{
			sMessage = "This module needs two charset lists as arguments: [-force] "
				"<client_charset1[,client_charset2[,...]]> "
				"<server_charset1[,server_charset2[,...]]>";
			return false;
			// the first charset in each list is the preferred one for
			// messages to the client / to the server.
		}

		VCString vsFrom, vsTo;
		sArgs.Token(uIndex).Split(",", vsFrom);
		sArgs.Token(uIndex + 1).Split(",", vsTo);

		// probe conversions:
		for(VCString::const_iterator itf = vsFrom.begin(); itf != vsFrom.end(); itf++)
		{
			for(VCString::const_iterator itt = vsTo.begin(); itt != vsTo.end(); itt++)
			{
				iconv_t icTest = iconv_open(itt->c_str(), itf->c_str());
				if(icTest == (iconv_t)-1)
				{
					sMessage = "Conversion from '" + *itf + "' to '" + *itt + "' is not possible.";
					return false;
				}
				iconv_close(icTest);

				icTest = iconv_open(itf->c_str(), itt->c_str());
				if(icTest == (iconv_t)-1)
				{
					sMessage = "Conversion from '" + *itt + "' to '" + *itf + "' is not possible.";
					return false;
				}
				iconv_close(icTest);
			}
		}

		m_vsClientCharsets = vsFrom;
		m_vsServerCharsets = vsTo;

		return true;
	}

	EModRet OnRaw(CString& sLine)
	{
		// convert IRC server -> client
		ConvertCharset(m_vsServerCharsets, m_vsClientCharsets[0], sLine);
		return CONTINUE;
	}

	EModRet OnUserRaw(CString& sLine)
	{
		// convert client -> IRC server
		ConvertCharset(m_vsClientCharsets, m_vsServerCharsets[0], sLine);
		return CONTINUE;
	}
};

MODULEDEFS(CCharsetMod, "Normalizes character encodings.")
