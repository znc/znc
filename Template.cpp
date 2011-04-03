/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Template.h"
#include "FileUtils.h"
#include "ZNCDebug.h"
#include <sstream>
#include <algorithm>

using std::stringstream;

void CTemplateOptions::Parse(const CString& sLine) {
	CString sName = sLine.Token(0, false, "=").Trim_n().AsUpper();
	CString sValue = sLine.Token(1, true, "=").Trim_n();

	if (sName == "ESC") {
		m_eEscapeTo = CString::ToEscape(sValue);
	} else if (sName == "ESCFROM") {
		m_eEscapeFrom = CString::ToEscape(sValue);
	}
}

CTemplate* CTemplateLoopContext::GetRow(unsigned int uIndex) {
	unsigned int uSize = m_pvRows->size();

	if (uIndex < uSize) {
		if (m_bReverse) {
			return (*m_pvRows)[uSize - uIndex -1];
		} else {
			return (*m_pvRows)[uIndex];
		}
	}

	return NULL;
}

CString CTemplateLoopContext::GetValue(const CString& sName, bool bFromIf) {
	CTemplate* pTemplate = GetCurRow();

	if (!pTemplate) {
		DEBUG("Loop [" + GetName() + "] has no row index [" + CString(GetRowIndex()) + "]");
		return "";
	}

	if (sName.Equals("__ID__")) {
		return CString(GetRowIndex() +1);
	} else if (sName.Equals("__COUNT__")) {
		return CString(GetRowCount());
	} else if (sName.Equals("__ODD__")) {
		return ((GetRowIndex() %2) ? "" : "1");
	} else if (sName.Equals("__EVEN__")) {
		return ((GetRowIndex() %2) ? "1" : "");
	} else if (sName.Equals("__FIRST__")) {
		return ((GetRowIndex() == 0) ? "1" : "");
	} else if (sName.Equals("__LAST__")) {
		return ((GetRowIndex() == m_pvRows->size() -1) ? "1" : "");
	} else if (sName.Equals("__OUTER__")) {
		return ((GetRowIndex() == 0 || GetRowIndex() == m_pvRows->size() -1) ? "1" : "");
	} else if (sName.Equals("__INNER__")) {
		return ((GetRowIndex() == 0 || GetRowIndex() == m_pvRows->size() -1) ? "" : "1");
	}

	return pTemplate->GetValue(sName, bFromIf);
}

CTemplate::~CTemplate() {
	for (map<CString, vector<CTemplate*> >::iterator it = m_mvLoops.begin(); it != m_mvLoops.end(); ++it) {
		vector<CTemplate*>& vLoop = it->second;
		for (unsigned int a = 0; a < vLoop.size(); a++) {
			delete vLoop[a];
		}
	}

	for (unsigned int a = 0; a < m_vLoopContexts.size(); a++) {
		delete m_vLoopContexts[a];
	}
}

void CTemplate::Init() {
	/* We have no CConfig in znc land
	CString sPath(CConfig::GetValue("WebFilesPath"));

	if (!sPath.empty()) {
		SetPath(sPath);
	}
	*/

	ClearPaths();
	m_pParent = NULL;
}

CString CTemplate::ExpandFile(const CString& sFilename, bool bFromInc) {
	/*if (sFilename.Left(1) == "/" || sFilename.Left(2) == "./") {
		return sFilename;
	}*/

	CString sFile(ResolveLiteral(sFilename).TrimLeft_n("/"));

	for (list<pair<CString, bool> >::iterator it = m_lsbPaths.begin(); it != m_lsbPaths.end(); ++it) {
		CString& sRoot = it->first;
		CString sFilePath(CDir::ChangeDir(sRoot, sFile));

		// Make sure path ends with a slash because "/foo/pub*" matches "/foo/public_keep_out/" but "/foo/pub/*" doesn't
		if (!sRoot.empty() && sRoot.Right(1) != "/") {
			sRoot += "/";
		}

		if (it->second && !bFromInc) {
			DEBUG("\t\tSkipping path (not from INC)  [" + sFilePath + "]");
			continue;
		}

		if (CFile::Exists(sFilePath)) {
			if (sRoot.empty() || sFilePath.Left(sRoot.length()) == sRoot) {
				DEBUG("    Found  [" + sFilePath + "]");
				return sFilePath;
			} else {
				DEBUG("\t\tOutside of root [" + sFilePath + "] !~ [" + sRoot + "]");
			}
		}
	}

	switch (m_lsbPaths.size()) {
		case 0:
			DEBUG("Unable to find [" + sFile + "] using the current directory");
			break;
		case 1:
			DEBUG("Unable to find [" + sFile + "] in the defined path [" + m_lsbPaths.begin()->first + "]");
			break;
		default:
			DEBUG("Unable to find [" + sFile + "] in any of the " + CString(m_lsbPaths.size()) + " defined paths");
	}

	return "";
}

void CTemplate::SetPath(const CString& sPaths) {
	VCString vsDirs;
	sPaths.Split(":", vsDirs, false);

	for (size_t a = 0; a < vsDirs.size(); a++) {
		AppendPath(vsDirs[a], false);
	}
}

CString CTemplate::MakePath(const CString& sPath) const {
	CString sRet(CDir::ChangeDir("./", sPath + "/"));

	if (!sRet.empty() && sRet.Right(1) != "/") {
		sRet += "/";
	}

	return sRet;
}

void CTemplate::PrependPath(const CString& sPath, bool bIncludesOnly) {
	DEBUG("CTemplate::PrependPath(" + sPath + ") == [" + MakePath(sPath) + "]");
	m_lsbPaths.push_front(make_pair(MakePath(sPath), bIncludesOnly));
}

void CTemplate::AppendPath(const CString& sPath, bool bIncludesOnly) {
	DEBUG("CTemplate::AppendPath(" + sPath + ") == [" + MakePath(sPath) + "]");
	m_lsbPaths.push_back(make_pair(MakePath(sPath), bIncludesOnly));
}

void CTemplate::RemovePath(const CString& sPath) {
	DEBUG("CTemplate::RemovePath(" + sPath + ") == [" + CDir::ChangeDir("./", sPath + "/") + "]");

	for (list<pair<CString, bool> >::iterator it = m_lsbPaths.begin(); it != m_lsbPaths.end(); ++it) {
		if (it->first == sPath) {
			m_lsbPaths.remove(*it);
			RemovePath(sPath); // @todo probably shouldn't use recursion, being lazy
			return;
		}
	}
}

void CTemplate::ClearPaths() {
	m_lsbPaths.clear();
}

bool CTemplate::SetFile(const CString& sFileName) {
	m_sFileName = ExpandFile(sFileName, false);
	PrependPath(sFileName + "/..");

	if (sFileName.empty()) {
		DEBUG("CTemplate::SetFile() - Filename is empty");
		return false;
	}

	if (m_sFileName.empty()) {
		DEBUG("CTemplate::SetFile() - [" + sFileName + "] does not exist");
		return false;
	}

	DEBUG("Set template file to [" + m_sFileName + "]");

	return true;
}

class CLoopSorter {
	CString m_sType;
public:
	CLoopSorter(const CString& sType) : m_sType(sType) {}
	bool operator()(CTemplate* pTemplate1, CTemplate* pTemplate2) {
		return (pTemplate1->GetValue(m_sType, false) < pTemplate2->GetValue(m_sType, false));
	}
};

CTemplate& CTemplate::AddRow(const CString& sName) {
	CTemplate* pTmpl = new CTemplate(m_spOptions, this);
	m_mvLoops[sName].push_back(pTmpl);

	return *pTmpl;
}

CTemplate* CTemplate::GetRow(const CString& sName, unsigned int uIndex) {
	vector<CTemplate*>* pvLoop = GetLoop(sName);

	if (pvLoop) {
		if (pvLoop->size() > uIndex) {
			return (*pvLoop)[uIndex];
		}
	}

	return NULL;
}

vector<CTemplate*>* CTemplate::GetLoop(const CString& sName) {
	CTemplateLoopContext* pContext = GetCurLoopContext();

	if (pContext) {
		CTemplate* pTemplate = pContext->GetCurRow();

		if (pTemplate) {
			return pTemplate->GetLoop(sName);
		}
	}

	map<CString, vector<CTemplate*> >::iterator it = m_mvLoops.find(sName);

	if (it != m_mvLoops.end()) {
		return &(it->second);
	}

	return NULL;
}

bool CTemplate::PrintString(CString& sRet) {
	sRet.clear();
	stringstream sStream;
	bool bRet = Print(sStream);

	sRet = sStream.str();

	return bRet;
}

bool CTemplate::Print(ostream& oOut) {
	return Print(m_sFileName, oOut);
}

bool CTemplate::Print(const CString& sFileName, ostream& oOut) {
	if (sFileName.empty()) {
		DEBUG("Empty filename in CTemplate::Print()");
		return false;
	}

	CFile File(sFileName);

	if (!File.Open()) {
		DEBUG("Unable to open file [" + sFileName + "] in CTemplate::Print()");
		return false;
	}

	CString sLine;
	CString sSetBlockVar;
	bool bValidLastIf = false;
	bool bInSetBlock = false;
	unsigned long uFilePos = 0;
	unsigned long uCurPos = 0;
	unsigned int uLineNum = 0;
	unsigned int uNestedIfs = 0;
	unsigned int uSkip = 0;
	bool bLoopCont = false;
	bool bLoopBreak = false;
	bool bExit = false;

	while (File.ReadLine(sLine)) {
		CString sOutput;
		bool bFoundATag = false;
		bool bTmplLoopHasData = false;
		uLineNum++;
		CString::size_type iPos = 0;
		uCurPos = uFilePos;
		unsigned int uLineSize = sLine.size();
		bool bBroke = false;

		while (1) {
			iPos = sLine.find("<?");

			if (iPos == CString::npos) {
				break;
			}

			uCurPos += iPos;
			bFoundATag = true;

			if (!uSkip) {
				sOutput += sLine.substr(0, iPos);
			}

			sLine = sLine.substr(iPos +2);

			CString::size_type iPos2 = sLine.find("?>");

			// Make sure our tmpl tag is ended properly
			if (iPos2 == CString::npos) {
				DEBUG("Template tag not ended properly in file [" + sFileName + "] [<?" + sLine + "]");
				return false;
			}

			uCurPos += iPos2 +4;

			CString sMid = CString(sLine.substr(0, iPos2)).Trim_n();

			// Make sure we don't have a nested tag
			if (sMid.find("<?") == CString::npos) {
				sLine = sLine.substr(iPos2 +2);
				CString sAction = sMid.Token(0);
				CString sArgs = sMid.Token(1, true);
				bool bNotFound = false;

				// If we're breaking or continuing from within a loop, skip all tags that aren't ENDLOOP
				if ((bLoopCont || bLoopBreak) && !sAction.Equals("ENDLOOP")) {
					continue;
				}

				if (!uSkip) {
					if (sAction.Equals("INC")) {
						if (!Print(ExpandFile(sArgs, true), oOut)) {
							DEBUG("Unable to print INC'd file [" + sArgs + "]");
							return false;
						}
					} else if (sAction.Equals("SETOPTION")) {
						m_spOptions->Parse(sArgs);
					} else if (sAction.Equals("ADDROW")) {
						CString sLoopName = sArgs.Token(0);
						MCString msRow;

						if (sArgs.Token(1, true, " ").OptionSplit(msRow)) {
							CTemplate& NewRow = AddRow(sLoopName);

							for (MCString::iterator it = msRow.begin(); it != msRow.end(); ++it) {
								NewRow[it->first] = it->second;
							}
						}
					} else if (sAction.Equals("SET")) {
						CString sName = sArgs.Token(0);
						CString sValue = sArgs.Token(1, true);

						(*this)[sName] = sValue;
					} else if (sAction.Equals("JOIN")) {
						VCString vsArgs;
						//sArgs.Split(" ", vsArgs, false, "\"", "\"");
						sArgs.QuoteSplit(vsArgs);

						if (vsArgs.size() > 1) {
							CString sDelim = vsArgs[0];
							bool bFoundOne = false;
							CString::EEscape eEscape = CString::EASCII;

							for (unsigned int a = 1; a < vsArgs.size(); a++) {
								const CString& sArg = vsArgs[a];

								if (sArg.Equals("ESC=", false, 4)) {
									eEscape = CString::ToEscape(sArg.LeftChomp_n(4));
								} else {
									CString sValue = GetValue(sArg);

									if (!sValue.empty()) {
										if (bFoundOne) {
											sOutput += sDelim;
										}

										sOutput += sValue.Escape_n(eEscape);
										bFoundOne = true;
									}
								}
							}
						}
					} else if (sAction.Equals("SETBLOCK")) {
						sSetBlockVar = sArgs;
						bInSetBlock = true;
					} else if (sAction.Equals("EXPAND")) {
						sOutput += ExpandFile(sArgs, true);
					} else if (sAction.Equals("VAR")) {
						sOutput += GetValue(sArgs);
					} else if (sAction.Equals("LT")) {
						sOutput += "<?";
					} else if (sAction.Equals("GT")) {
						sOutput += "?>";
					} else if (sAction.Equals("CONTINUE")) {
						CTemplateLoopContext* pContext = GetCurLoopContext();

						if (pContext) {
							uSkip++;
							bLoopCont = true;

							break;
						} else {
							DEBUG("[" + sFileName + ":" + CString(uCurPos - iPos2 -4) + "] <? CONTINUE ?> must be used inside of a loop!");
						}
					} else if (sAction.Equals("BREAK")) {
						// break from loop
						CTemplateLoopContext* pContext = GetCurLoopContext();

						if (pContext) {
							uSkip++;
							bLoopBreak = true;

							break;
						} else {
							DEBUG("[" + sFileName + ":" + CString(uCurPos - iPos2 -4) + "] <? BREAK ?> must be used inside of a loop!");
						}
					} else if (sAction.Equals("EXIT")) {
						bExit = true;
					} else if (sAction.Equals("DEBUG")) {
						DEBUG("CTemplate DEBUG [" + sFileName + "@" + CString(uCurPos - iPos2 -4) + "b] -> [" + sArgs + "]");
					} else if (sAction.Equals("LOOP")) {
						CTemplateLoopContext* pContext = GetCurLoopContext();

						if (!pContext || pContext->GetFilePosition() != uCurPos) {
							// we are at a brand new loop (be it new or a first pass at an inner loop)

							CString sLoopName = sArgs.Token(0);
							bool bReverse = (sArgs.Token(1).Equals("REVERSE"));
							bool bSort = (sArgs.Token(1).Left(4).Equals("SORT"));
							vector<CTemplate*>* pvLoop = GetLoop(sLoopName);

							if (bSort && pvLoop != NULL &&  pvLoop->size() > 1) {
								CString sKey;

								if(sArgs.Token(1).TrimPrefix_n("SORT").Left(4).Equals("ASC=")) {
									sKey = sArgs.Token(1).TrimPrefix_n("SORTASC=");
								} else if(sArgs.Token(1).TrimPrefix_n("SORT").Left(5).Equals("DESC=")) {
									sKey = sArgs.Token(1).TrimPrefix_n("SORTDESC=");
									bReverse = true;
								}

								if (!sKey.empty()) {
									std::sort(pvLoop->begin(), pvLoop->end(), CLoopSorter(sKey));
								}
							}

							if (pvLoop) {
								// If we found data for this loop, add it to our context vector
								//unsigned long uBeforeLoopTag = uCurPos - iPos2 - 4;
								unsigned long uAfterLoopTag = uCurPos;

								for (CString::size_type t = 0; t < sLine.size(); t++) {
									char c = sLine[t];
									if (c == '\r' || c == '\n') {
										uAfterLoopTag++;
									} else {
										break;
									}
								}

								m_vLoopContexts.push_back(new CTemplateLoopContext(uAfterLoopTag, sLoopName, bReverse, pvLoop));
							} else {  // If we don't have data, just skip this loop and everything inside
								uSkip++;
							}
						}
					} else if (sAction.Equals("IF")) {
						if (ValidIf(sArgs)) {
							uNestedIfs++;
							bValidLastIf = true;
						} else {
							uSkip++;
							bValidLastIf = false;
						}
					} else if (sAction.Equals("REM")) {
						uSkip++;
					} else {
						bNotFound = true;
					}
				} else if (sAction.Equals("REM")) {
					uSkip++;
				} else if (sAction.Equals("IF")) {
					uSkip++;
				} else if (sAction.Equals("LOOP")) {
					uSkip++;
				}

				if (sAction.Equals("ENDIF")) {
					if (uSkip) {
						uSkip--;
					} else {
						uNestedIfs--;
					}
				} else if (sAction.Equals("ENDREM")) {
					if (uSkip) {
						uSkip--;
					}
				} else if (sAction.Equals("ENDSETBLOCK")) {
					bInSetBlock = false;
					sSetBlockVar = "";
				} else if (sAction.Equals("ENDLOOP")) {
					if (bLoopCont && uSkip == 1) {
						uSkip--;
						bLoopCont = false;
					}

					if (bLoopBreak && uSkip == 1) {
						uSkip--;
					}

					if (uSkip) {
						uSkip--;
					} else {
						// We are at the end of the loop so we need to inc the index
						CTemplateLoopContext* pContext = GetCurLoopContext();

						if (pContext) {
							pContext->IncRowIndex();

							// If we didn't go out of bounds we need to seek back to the top of our loop
							if (!bLoopBreak && pContext->GetCurRow()) {
								uCurPos = pContext->GetFilePosition();
								uFilePos = uCurPos;
								uLineSize = 0;

								File.Seek(uCurPos);
								bBroke = true;

								if (!sOutput.Trim_n().empty()) {
									pContext->SetHasData();
								}

								break;
							} else {
								if (sOutput.Trim_n().empty()) {
									sOutput.clear();
								}

								bTmplLoopHasData = pContext->HasData();
								DelCurLoopContext();
								bLoopBreak = false;
							}
						}
					}
				} else if (sAction.Equals("ELSE")) {
					if (!bValidLastIf && uSkip == 1) {
						CString sArg = sArgs.Token(0);

						if (sArg.empty() || (sArg.Equals("IF") && ValidIf(sArgs.Token(1, true)))) {
							uSkip = 0;
							bValidLastIf = true;
						}
					} else if (!uSkip) {
						uSkip = 1;
					}
				} else if (bNotFound) {
					// Unknown tag that isn't being skipped...
					vector<CSmartPtr<CTemplateTagHandler> >& vspTagHandlers = GetTagHandlers();

					if (!vspTagHandlers.empty()) { // @todo this should go up to the top to grab handlers
						CTemplate* pTmpl = GetCurTemplate();
						CString sCustomOutput;

						for (unsigned int j = 0; j < vspTagHandlers.size(); j++) {
							CSmartPtr<CTemplateTagHandler> spTagHandler = vspTagHandlers[j];

							if (spTagHandler->HandleTag(*pTmpl, sAction, sArgs, sCustomOutput)) {
								sOutput += sCustomOutput;
								bNotFound = false;
								break;
							}
						}

						if (bNotFound) {
							DEBUG("Unknown/Unhandled tag [" + sAction + "]");
						}
					}
				}

				continue;
			}

			DEBUG("Malformed tag on line " + CString(uLineNum) + " of [" << File.GetLongName() + "]");
			DEBUG("--------------- [" + sLine + "]");
		}

		if (!bBroke) {
			uFilePos += uLineSize;

			if (!uSkip) {
				sOutput += sLine;
			}
		}

		if (!bFoundATag || bTmplLoopHasData || sOutput.find_first_not_of(" \t\r\n") != CString::npos) {
			if (bInSetBlock) {
				CString sName = sSetBlockVar.Token(0);
				//CString sValue = sSetBlockVar.Token(1, true);
				(*this)[sName] += sOutput;
			} else {
				oOut << sOutput;
			}
		}

		if (bExit) {
			break;
		}
	}

	oOut.flush();

	return true;
}

void CTemplate::DelCurLoopContext() {
	if (m_vLoopContexts.empty()) {
		return;
	}

	delete m_vLoopContexts.back();
	m_vLoopContexts.pop_back();
}

CTemplateLoopContext* CTemplate::GetCurLoopContext() {
	if (!m_vLoopContexts.empty()) {
		return m_vLoopContexts.back();
	}

	return NULL;
}

bool CTemplate::ValidIf(const CString& sArgs) {
	CString sArgStr = sArgs;
	//sArgStr.Replace(" ", "", "\"", "\"", true);
	sArgStr.Replace(" &&", "&&", "\"", "\"", false);
	sArgStr.Replace("&& ", "&&", "\"", "\"", false);
	sArgStr.Replace(" ||", "||", "\"", "\"", false);
	sArgStr.Replace("|| ", "||", "\"", "\"", false);

	CString::size_type uOrPos = sArgStr.find("||");
	CString::size_type uAndPos = sArgStr.find("&&");

	while (uOrPos != CString::npos || uAndPos != CString::npos || !sArgStr.empty()) {
		bool bAnd = false;

		if (uAndPos < uOrPos) {
			bAnd = true;
		}

		CString sExpr = sArgStr.Token(0, false, ((bAnd) ? "&&" : "||"));
		sArgStr = sArgStr.Token(1, true, ((bAnd) ? "&&" : "||"));

		if (ValidExpr(sExpr)) {
			if (!bAnd) {
				return true;
			}
		} else {
			if (bAnd) {
				return false;
			}
		}

		uOrPos = sArgStr.find("||");
		uAndPos = sArgStr.find("&&");
	}

	return false;
}

bool CTemplate::ValidExpr(const CString& sExpression) {
	bool bNegate = false;
	CString sExpr(sExpression);
	CString sName;
	CString sValue;

	if (sExpr.Left(1) == "!") {
		bNegate = true;
		sExpr.LeftChomp();
	}

	if (sExpr.find("!=") != CString::npos) {
		sName = sExpr.Token(0, false, "!=").Trim_n();
		sValue = sExpr.Token(1, true, "!=", false, "\"", "\"", true).Trim_n();
		bNegate = !bNegate;
	} else if (sExpr.find("==") != CString::npos) {
		sName = sExpr.Token(0, false, "==").Trim_n();
		sValue = sExpr.Token(1, true, "==", false, "\"", "\"", true).Trim_n();
	} else if (sExpr.find(">=") != CString::npos) {
		sName = sExpr.Token(0, false, ">=").Trim_n();
		sValue = sExpr.Token(1, true, ">=", false, "\"", "\"", true).Trim_n();
		return (GetValue(sName, true).ToLong() >= sValue.ToLong());
	} else if (sExpr.find("<=") != CString::npos) {
		sName = sExpr.Token(0, false, "<=").Trim_n();
		sValue = sExpr.Token(1, true, "<=", false, "\"", "\"", true).Trim_n();
		return (GetValue(sName, true).ToLong() <= sValue.ToLong());
	} else if (sExpr.find(">") != CString::npos) {
		sName = sExpr.Token(0, false, ">").Trim_n();
		sValue = sExpr.Token(1, true, ">", false, "\"", "\"", true).Trim_n();
		return (GetValue(sName, true).ToLong() > sValue.ToLong());
	} else if (sExpr.find("<") != CString::npos) {
		sName = sExpr.Token(0, false, "<").Trim_n();
		sValue = sExpr.Token(1, true, "<", false, "\"", "\"", true).Trim_n();
		return (GetValue(sName, true).ToLong() < sValue.ToLong());
	} else {
		sName = sExpr.Trim_n();
	}

	if (sValue.empty()) {
		return (bNegate != IsTrue(sName));
	}

	sValue = ResolveLiteral(sValue);

	return (bNegate != GetValue(sName, true).Equals(sValue));
}

bool CTemplate::IsTrue(const CString& sName) {
	if (HasLoop(sName)) {
		return true;
	}

	return GetValue(sName, true).ToBool();
}

bool CTemplate::HasLoop(const CString& sName) {
	return (GetLoop(sName) != NULL);
}

CTemplate* CTemplate::GetParent(bool bRoot) {
	if (!bRoot) {
		return m_pParent;
	}

	return (m_pParent) ? m_pParent->GetParent(bRoot) : this;
}

CTemplate* CTemplate::GetCurTemplate() {
	CTemplateLoopContext* pContext = GetCurLoopContext();

	if (!pContext) {
		return this;
	}

	return pContext->GetCurRow();
}

CString CTemplate::ResolveLiteral(const CString& sString) {
	if (sString.Left(2) == "**") {
		// Allow string to start with a literal * by using two in a row
		return sString.substr(1);
	} else if (sString.Left(1) == "*") {
		// If it starts with only one * then treat it as a var and do a lookup
		return GetValue(sString.substr(1));
	}

	return sString;
}

CString CTemplate::GetValue(const CString& sArgs, bool bFromIf) {
	CTemplateLoopContext* pContext = GetCurLoopContext();
	CString sName = sArgs.Token(0);
	CString sRest = sArgs.Token(1, true);
	CString sRet;

	while (sRest.Replace(" =", "=", "\"", "\"")) {}
	while (sRest.Replace("= ", "=", "\"", "\"")) {}

	VCString vArgs;
	MCString msArgs;
	//sRest.Split(" ", vArgs, false, "\"", "\"");
	sRest.QuoteSplit(vArgs);

	for (unsigned int a = 0; a < vArgs.size(); a++) {
		const CString& sArg = vArgs[a];

		msArgs[sArg.Token(0, false, "=").AsUpper()] = sArg.Token(1, true, "=");
	}

	/* We have no CConfig in znc land
	if (msArgs.find("CONFIG") != msArgs.end()) {
		sRet = CConfig::GetValue(sName);
	} else*/ if (msArgs.find("ROWS") != msArgs.end()) {
		vector<CTemplate*>* pLoop = GetLoop(sName);
		sRet = CString((pLoop) ? pLoop->size() : 0);
	} else if (msArgs.find("TOP") == msArgs.end() && pContext) {
		sRet = pContext->GetValue(sArgs, bFromIf);

		if (!sRet.empty()) {
			return sRet;
		}
	} else {
		if (sName.Left(1) == "*") {
			sName.LeftChomp(1);
			MCString::iterator it = find(sName);
			sName = (it != end()) ? it->second : "";
		}

		MCString::iterator it = find(sName);
		sRet = (it != end()) ? it->second : "";
	}

	vector<CSmartPtr<CTemplateTagHandler> >& vspTagHandlers = GetTagHandlers();

	if (!vspTagHandlers.empty()) { // @todo this should go up to the top to grab handlers
		CTemplate* pTmpl = GetCurTemplate();

		if (sRet.empty()) {
			for (unsigned int j = 0; j < vspTagHandlers.size(); j++) {
				CSmartPtr<CTemplateTagHandler> spTagHandler = vspTagHandlers[j];
				CString sCustomOutput;

				if (!bFromIf && spTagHandler->HandleVar(*pTmpl, sArgs.Token(0), sArgs.Token(1, true), sCustomOutput)) {
					sRet = sCustomOutput;
					break;
				} else if (bFromIf && spTagHandler->HandleIf(*pTmpl, sArgs.Token(0), sArgs.Token(1, true), sCustomOutput)) {
					sRet = sCustomOutput;
					break;
				}
			}
		}

		for (unsigned int j = 0; j < vspTagHandlers.size(); j++) {
			CSmartPtr<CTemplateTagHandler> spTagHandler = vspTagHandlers[j];

			if (spTagHandler->HandleValue(*pTmpl, sRet, msArgs)) {
				break;
			}
		}
	}

	if (!bFromIf) {
		if (sRet.empty()) {
			sRet = ResolveLiteral(msArgs["DEFAULT"]);
		}

		MCString::iterator it = msArgs.find("ESC");

		if (it != msArgs.end()) {
			VCString vsEscs;
			it->second.Split(",", vsEscs, false);

			for (unsigned int a = 0; a < vsEscs.size(); a++) {
				sRet.Escape(CString::ToEscape(vsEscs[a]));
			}
		} else {
			sRet.Escape(m_spOptions->GetEscapeFrom(), m_spOptions->GetEscapeTo());
		}
	}

	return sRet;
}
