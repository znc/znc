/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include <iconv.h>

class CCharsetMod : public CModule
{
private:
	VCString m_vsClientCharsets;
	VCString m_vsServerCharsets;

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
				(uInLen < 1 ? NULL : const_cast<char**>(&pIn)),
				(uInLen < 1 ? NULL : &uInLen),
				&pOut, &uBufSize) == (size_t)-1)
				// explanation: iconv needs a last call with input = NULL to
				// copy/convert possibly left data chunks into the output buffer.
			{
				if(errno == EINVAL)
				{
					iconv_close(ic);
					return (size_t)-1;
				}
				else if(errno != E2BIG)
				{
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

		iconv_t ic = iconv_open(sTo.c_str(), sFrom.c_str());
		if(ic == (iconv_t)-1) return false;

		size_t uLength = GetConversionLength(ic, sData);

		if(uLength == (size_t)-1)
		{
			// incompatible input encoding.
			iconv_close(ic);
			return false;
		}
		else if(uLength != (size_t)-2)
		{
			iconv(ic, NULL, NULL, NULL, NULL); // reset

			size_t uResultBufSize = uLength + 1;
			char *pResult = new char[uResultBufSize];
			memset(pResult, 0, uResultBufSize);
			char *pResultWalker = pResult;

			const char* pIn = sData.c_str();
			size_t uInLen = sData.size();

			size_t uResult = iconv(ic, const_cast<char**>(&pIn), &uInLen, &pResultWalker, &uResultBufSize);

			iconv_close(ic);

			if(uResult != (size_t)-1)
			{
				sData.erase();
				sData.append(pResult, uLength);

				delete[] pResult;
				return true;
			}
			else
			{
				delete[] pResult;
			}
		}

		int tmp_errno = errno;
		iconv_close(ic);
		errno = tmp_errno;
		return false;
	}

	bool ConvertCharset(const VCString& vsFrom, const CString& sTo, CString& sData)
	{
		CString sDataCopy(sData);

		iconv_t icTest = iconv_open(sTo.c_str(), sTo.c_str());
		if(icTest != (iconv_t)-1)
		{
			size_t uTest = GetConversionLength(icTest, sData);

			if(uTest != (size_t)-1 && uTest != (size_t)-2)
			{
				return true;
			}

			iconv_close(icTest);
		}

		bool bConverted = false;

		for(VCString::const_iterator itf = vsFrom.begin(); itf != vsFrom.end(); itf++)
		{
			if(ConvertCharset(*itf, sTo, sDataCopy))
			{
				sData = sDataCopy;
				bConverted = true;
				break;
			}
			else
			{
				sDataCopy = sData;
			}
		}

		return bConverted;
	}

public:
	MODCONSTRUCTOR(CCharsetMod) {}

	bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		if(sArgs.Token(1).empty() || !sArgs.Token(2).empty())
		{
			sMessage = "This module needs two charset lists as arguments: "
				"<client_charset1[,client_charset2[,...]]> "
				"<server_charset1[,server_charset2[,...]]>";
			return false;
		}

		VCString vsFrom, vsTo;
		sArgs.Token(0).Split(",", vsFrom);
		sArgs.Token(1).Split(",", vsTo);

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
		ConvertCharset(m_vsServerCharsets, m_vsClientCharsets[0], sLine);
		return CONTINUE;
	}

	EModRet OnUserRaw(CString& sLine)
	{
		ConvertCharset(m_vsClientCharsets, m_vsServerCharsets[0], sLine);
		return CONTINUE;
	}
};

MODULEDEFS(CCharsetMod, "Normalizes character encodings.")
