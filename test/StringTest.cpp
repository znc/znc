/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#include <gtest/gtest.h>
#include <znc/ZNCString.h>

class EscapeTest : public ::testing::Test {
protected:
	void testEncode(const CString& in, const CString& expectedOut, const CString& sformat, CString::EEscape format) {
		CString out;

		SCOPED_TRACE("Format: " + sformat);

		// Encode, then decode again and check we still got the same string
		out = in.Escape_n(CString::EASCII, format);
		EXPECT_EQ(expectedOut, out);
		out = out.Escape_n(format, CString::EASCII);
		EXPECT_EQ(in, out);
	}

	void testString(const CString& in, const CString& url,
			const CString& html, const CString& sql) {
		SCOPED_TRACE("String: " + in);

		testEncode(in, url,  "url",  CString::EURL);
		testEncode(in, html, "html", CString::EHTML);
		testEncode(in, sql,  "sql",  CString::ESQL);
	}
};

TEST_F(EscapeTest, Test) {
	//          input      url          html             sql
	testString("abcdefg", "abcdefg",   "abcdefg",       "abcdefg");
	testString("\n\t\r",  "%0A%09%0D", "\n\t\r",        "\\n\\t\\r");
	testString("'\"",     "%27%22",    "'&quot;",       "\\'\\\"");
	testString("&<>",     "%26%3C%3E", "&amp;&lt;&gt;", "&<>");
}

