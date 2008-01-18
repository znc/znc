/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifdef _MODULES

#include "Template.h"
#include "FileUtils.h"

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
	if (uIndex < m_pvRows->size()) {
		return (*m_pvRows)[uIndex];
	}

	return NULL;
}

CString CTemplateLoopContext::GetValue(const CString& sName) {
	CTemplate* pTemplate = GetCurRow();

	if (!pTemplate) {
		DEBUG_ONLY(cerr << "Loop [" << GetName() << "] has no row index [" << GetCurRow() << "]" << endl);
		return "";
	}

	if (sName.CaseCmp("__ID__") == 0) {
		return CString(GetRowIndex() +1);
	} else if (sName.CaseCmp("__COUNT__") == 0) {
		return CString(GetRowCount());
	} else if (sName.CaseCmp("__ODD__") == 0) {
		return ((GetRowIndex() %2) ? "" : "1");
	} else if (sName.CaseCmp("__EVEN__") == 0) {
		return ((GetRowIndex() %2) ? "1" : "");
	} else if (sName.CaseCmp("__FIRST__") == 0) {
		return ((GetRowIndex() == 0) ? "1" : "");
	} else if (sName.CaseCmp("__LAST__") == 0) {
		return ((GetRowIndex() == m_pvRows->size() -1) ? "1" : "");
	} else if (sName.CaseCmp("__OUTER__") == 0) {
		return ((GetRowIndex() == 0 || GetRowIndex() == m_pvRows->size() -1) ? "1" : "");
	} else if (sName.CaseCmp("__INNER__") == 0) {
		return ((GetRowIndex() == 0 || GetRowIndex() == m_pvRows->size() -1) ? "" : "1");
	}

	return pTemplate->GetValue(sName);
}

CTemplate::~CTemplate() {
	for (map<CString, vector<CTemplate*> >::iterator it = m_mvLoops.begin(); it != m_mvLoops.end(); it++) {
		vector<CTemplate*>& vLoop = it->second;
		for (unsigned int a = 0; a < vLoop.size(); a++) {
			delete vLoop[a];
		}
	}

	for (unsigned int a = 0; a < m_vLoopContexts.size(); a++) {
		delete m_vLoopContexts[a];
	}
}

bool CTemplate::SetFile(const CString& sFileName) {
	if (sFileName.empty()) {
		DEBUG_ONLY(cerr << "CTemplate::SetFile() - Filename is empty" << endl);
		return false;
	}

	if (!CFile::Exists(sFileName)) {
		DEBUG_ONLY(cerr << "CTemplate::SetFile() - [" << sFileName << "] does not exist" << endl);
		return false;
	}

	m_sFileName = sFileName;
	return true;
}

CTemplate& CTemplate::AddRow(const CString& sName) {
	CTemplate* pTmpl = new CTemplate(m_spOptions);
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

bool CTemplate::Print(ostream& oOut) {
	return Print(m_sFileName, oOut);
}

bool CTemplate::Print(const CString& sFileName, ostream& oOut) {
	if (sFileName.empty()) {
		DEBUG_ONLY(cerr << "Empty filename in CTemplate::Print()" << endl);
		return false;
	}

	CFile File(sFileName);

	if (!File.Open(O_RDONLY)) {
		DEBUG_ONLY(cerr << "Unable to open file [" << sFileName << "] in CTemplate::Print()" << endl);
		return false;
	}

	CString sLine;
	CString sOutput;
	bool bValidLastIf = false;
	unsigned long uFilePos = 0;
	unsigned long uCurPos = 0;
	unsigned int uLineNum = 0;
	unsigned int uNestedIfs = 0;
	unsigned int uSkip = 0;

	while (File.ReadLine(sLine)) {
		bool bFoundOneTag = false;
		uLineNum++;
		CString::size_type iPos = 0;
		uCurPos = uFilePos;
		unsigned int uLineSize = sLine.size();

		do {
			iPos = sLine.find("<?");

			if (iPos != CString::npos) {
				uCurPos += iPos;
				bFoundOneTag = true;

				if (!uSkip) {
					sOutput += sLine.substr(0, iPos);
				}

				sLine = sLine.substr(iPos +2);

				CString::size_type iPos2 = sLine.find("?>");

				// Make sure our tmpl tag is ended properly
				if (iPos2 != CString::npos) {
					CString sMid = CString(sLine.substr(0, iPos2)).Trim_n();

					// Make sure we don't have a nested tag
					if (sMid.find("<?") == CString::npos) {
						sLine = sLine.substr(iPos2 +2);
						CString sAction = sMid.Token(0);
						CString sArgs = sMid.Token(1, true);

						if (!uSkip) {
							if (sAction.CaseCmp("INC") == 0) {
								if (!Print(File.GetDir() + sArgs, oOut)) {
									return false;
								}
							} else if (sAction.CaseCmp("SETOPTION") == 0) {
								m_spOptions->Parse(sArgs);
							} else if (sAction.CaseCmp("ADDROW") == 0) {
								CString sLoopName = sArgs.Token(0);
								MCString msRow;

								if (sArgs.Token(1).URLSplit(msRow)) {
									CTemplate& NewRow = AddRow(sLoopName);

									for (MCString::iterator it = msRow.begin(); it != msRow.end(); it++) {
										NewRow[it->first] = it->second;
									}
								}
							} else if (sAction.CaseCmp("SET") == 0) {
								CString sName = sArgs.Token(0);
								CString sValue = sArgs.Token(1, true);

								(*this)[sName] = sValue;
							} else if (sAction.CaseCmp("JOIN") == 0) {
								VCString vsArgs;
								sArgs.Split(" ", vsArgs, false, "\"", "\"");

								if (vsArgs.size() > 1) {
									CString sDelim = vsArgs[0];
									bool bFoundOne = false;
									CString::EEscape eEscape = CString::EASCII;

									for (unsigned int a = 1; a < vsArgs.size(); a++) {
										const CString& sArg = vsArgs[a];

										if (sArg.Left(4).CaseCmp("ESC=") == 0) {
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
							} else if (sAction.CaseCmp("VAR") == 0) {
								sOutput += GetValue(sArgs);
							} else if (sAction.CaseCmp("LOOP") == 0) {
								CTemplateLoopContext* pContext = GetCurLoopContext();

								if (!pContext || pContext->GetFilePosition() != uCurPos) {
									// we are at a brand new loop (be it new or a first pass at an inner loop)

									CString sLoopName = sArgs.Token(0);
									vector<CTemplate*>* pvLoop = GetLoop(sLoopName);

									if (pvLoop) {
										// If we found data for this loop, add it to our context vector
										m_vLoopContexts.push_back(new CTemplateLoopContext(uCurPos, sLoopName, pvLoop));
									} else {  // If we don't have data, just skip this loop and everything inside
										uSkip++;
									}
								}
							} else if (sAction.CaseCmp("IF") == 0) {
								if (ValidIf(sArgs)) {
									uNestedIfs++;
									bValidLastIf = true;
								} else {
									uSkip++;
									bValidLastIf = false;
								}
							}
						} else if (sAction.CaseCmp("IF") == 0) {
							uSkip++;
						} else if (sAction.CaseCmp("LOOP") == 0) {
							uSkip++;
						}

						if (sAction.CaseCmp("ENDIF") == 0) {
							if (uSkip) {
								uSkip--;
							} else {
								uNestedIfs--;
							}
						} else if (sAction.CaseCmp("ENDLOOP") == 0) {
							if (uSkip) {
								uSkip--;
							} else {
								// We are at the end of the loop so we need to inc the index
								CTemplateLoopContext* pContext = GetCurLoopContext();

								if (pContext) {
									pContext->IncRowIndex();

									// If we didn't go out of bounds we need to seek back to the top of our loop
									if (pContext->GetCurRow()) {
										uCurPos = pContext->GetFilePosition();
										uFilePos = uCurPos;
										uLineSize = 0;

										File.Seek(uCurPos);
									} else {
										DelCurLoopContext();
									}
								}
							}
						} else if (sAction.CaseCmp("ELSE") == 0) {
							if (!bValidLastIf && uSkip == 1) {
								CString sArg = sArgs.Token(0);

								if (sArg.empty() || (sArg.CaseCmp("IF") == 0 && ValidIf(sArgs.Token(1, true)))) {
									uSkip = 0;
									bValidLastIf = true;
								}
							} else if (!uSkip) {
								uSkip = 1;
							}
						}

						continue;
					}
				}

				DEBUG_ONLY(cerr << "Malformed tag on line " << uLineNum << " of [" << File.GetLongName() << "]" << endl);
				DEBUG_ONLY(cerr << "--------------- [" << sLine << "]" << endl);
			}
		} while (iPos != CString::npos);

		uFilePos += uLineSize;

		if (!uSkip) {
			sOutput += sLine;
		}

		if (!bFoundOneTag || sOutput.find_first_not_of(" \t\r\n") != CString::npos) {
			oOut << sOutput;
		}

		sOutput.clear();
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
	sArgStr.Replace(" ", "", "\"", "\"", true);

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

bool CTemplate::ValidExpr(const CString& sExpr) {
	bool bNegate = false;
	CString sName;
	CString sValue;

	if (sExpr.find("!=") != CString::npos) {
		sName = sExpr.Token(0, false, "!=").Trim_n();
		sValue = sExpr.Token(1, true, "!=").Trim_n();
		bNegate = true;
	} else if (sExpr.find("==") != CString::npos) {
		sName = sExpr.Token(0, false, "==").Trim_n();
		sValue = sExpr.Token(1, true, "==").Trim_n();
		bNegate = false;
	} else {
		sName = sExpr.Trim_n();
	}

	if (sName.Left(1) == "!") {
		bNegate = true;
		sName.LeftChomp();
	}

	if (sValue.empty()) {
		return (bNegate != IsTrue(sName));
	}

	return (bNegate != (GetValue(sName).CaseCmp(sValue) == 0));
}

bool CTemplate::IsTrue(const CString& sName) {
	if (HasLoop(sName)) {
		return true;
	}

	return GetValue(sName).ToBool();
}

bool CTemplate::HasLoop(const CString& sName) {
	return (GetLoop(sName) != NULL);
}

CTemplate* CTemplate::GetCurTemplate() {
	CTemplateLoopContext* pContext = GetCurLoopContext();

	if (!pContext) {
		return this;
	}

	return pContext->GetCurRow();
}

CString CTemplate::GetValue(const CString& sArgs) {
	CTemplateLoopContext* pContext = GetCurLoopContext();
	CString sName = sArgs.Token(0);
	CString sRest = sArgs.Token(1, true);
	CString sRet;

	if (pContext) {
		sRet = pContext->GetValue(sName);
	} else {
		MCString::iterator it = find(sName);
		sRet = (it != end()) ? it->second : "";
	}

	while (sRest.Replace(" =", "=", "\"", "\"")) ;
	while (sRest.Replace("= ", "=", "\"", "\"")) ;

	VCString vArgs;
	MCString msArgs;
	sRest.Split(" ", vArgs, false, "\"", "\"");

	for (unsigned int a = 0; a < vArgs.size(); a++) {
		const CString& sArg = vArgs[a];

		msArgs[sArg.Token(0, false, "=").AsUpper()] = sArg.Token(1, true, "=");
	}

	if (sRet.empty()) {
		sRet = msArgs["DEFAULT"];
	}

	MCString::iterator it = msArgs.find("ESC");

	if (it != msArgs.end()) {
		sRet.Escape(CString::ToEscape(it->second));
	} else {
		sRet.Escape(m_spOptions->GetEscapeFrom(), m_spOptions->GetEscapeTo());
	}

	return sRet;
}

#endif // _MODULES
