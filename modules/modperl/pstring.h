/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

class PString : public CString {
	public:
		enum EType {
			STRING,
			INT,
			UINT,
			NUM,
			BOOL
		};

		PString() : CString() { m_eType = STRING; }
		PString(const char* c) : CString(c) { m_eType = STRING; }
		PString(const CString& s) : CString(s) { m_eType = STRING; }
		PString(int i) : CString(i) { m_eType = INT; }
		PString(u_int i) : CString(i) { m_eType = UINT; }
		PString(long i) : CString(i) { m_eType = INT; }
		PString(u_long i) : CString(i) { m_eType = UINT; }
		PString(long long i) : CString(i) { m_eType = INT; }
		PString(unsigned long long i) : CString(i) { m_eType = UINT; }
		PString(double i) : CString(i) { m_eType = NUM; }
		PString(bool b) : CString((b ? "1" : "0")) { m_eType = BOOL; }
		PString(SV* sv) {
			STRLEN len = SvCUR(sv);
			char* c = SvPV(sv, len);
			char* c2 = new char[len+1];
			memcpy(c2, c, len);
			c2[len] = 0;
			*this = c2;
			delete[] c2;
		}

		virtual ~PString() {}

		EType GetType() const { return m_eType; }
		void SetType(EType e) { m_eType = e; }

		SV* GetSV(bool bMakeMortal = true) const
		{
			SV* pSV = nullptr;
			switch (GetType()) {
				case NUM:
					pSV = newSVnv(ToDouble());
					break;
				case INT:
					pSV = newSViv(ToLongLong());
					break;
				case UINT:
				case BOOL:
					pSV = newSVuv(ToULongLong());
					break;
				case STRING:
				default:
					pSV = newSVpv(data(), length());
					break;
			}

			if (bMakeMortal) {
				pSV = sv_2mortal(pSV);
			}

			return pSV;
		}

	private:
		EType m_eType;
};


