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

// GTest uses this function to output objects
void PrintTo(const CString& s, std::ostream* o) {
	*o << '"' << s.Escape_n(CString::EASCII, CString::EDEBUG) << '"';
}

class EscapeTest : public ::testing::Test {
protected:
	void testEncode(const CString& in, const CString& expectedOut, const CString& sformat) {
		CString::EEscape format = CString::ToEscape(sformat);
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

		testEncode(in, url,  "URL");
		testEncode(in, html, "HTML");
		testEncode(in, sql,  "SQL");
	}
};

TEST_F(EscapeTest, Test) {
	//          input      url          html             sql
	testString("abcdefg", "abcdefg",   "abcdefg",       "abcdefg");
	testString("\n\t\r",  "%0A%09%0D", "\n\t\r",        "\\n\\t\\r");
	testString("'\"",     "%27%22",    "'&quot;",       "\\'\\\"");
	testString("&<>",     "%26%3C%3E", "&amp;&lt;&gt;", "&<>");
}

TEST(StringTest, Bool) {
	EXPECT_EQ(true, CString(true).ToBool());
	EXPECT_EQ(false, CString(false).ToBool());
}

#define CS(s) (CString((s), sizeof(s)-1))

TEST(StringTest, Cmp) {
	CString s = "Bbb";

	EXPECT_EQ(CString("Bbb"), s);
	EXPECT_LT(CString("Aaa"), s);
	EXPECT_GT(CString("Ccc"), s);
	EXPECT_EQ(0, s.StrCmp("Bbb"));
	EXPECT_GT(0, s.StrCmp("bbb"));
	EXPECT_LT(0, s.StrCmp("Aaa"));
	EXPECT_GT(0, s.StrCmp("Ccc"));
	EXPECT_EQ(0, s.CaseCmp("Bbb"));
	EXPECT_EQ(0, s.CaseCmp("bbb"));
	EXPECT_LT(0, s.CaseCmp("Aaa"));
	EXPECT_GT(0, s.CaseCmp("Ccc"));

	EXPECT_TRUE(s.Equals("bbb"));
	EXPECT_FALSE(s.Equals("bbb", true));
	EXPECT_FALSE(s.Equals("bb"));
}

TEST(StringTest, Wild) {
	EXPECT_TRUE(CString::WildCmp("*!?bar@foo", "I_am!~bar@foo"));
	EXPECT_TRUE(CString::WildCmp("", ""));
	EXPECT_TRUE(CString::WildCmp("*a*b*c*", "abc"));
	EXPECT_TRUE(CString::WildCmp("*a*b*c*", "axbyc"));
	EXPECT_FALSE(CString::WildCmp("*a*b*c*", "xy"));
}

TEST(StringTest, Case) {
	CString x = CS("xx");
	CString X = CS("XX");
	EXPECT_EQ(X, x.AsUpper());
	EXPECT_EQ(x, X.AsLower());
}

TEST(StringTest, Replace) {
	EXPECT_EQ("(b()b)", CString("(a()a)").Replace_n("a", "b"));
	EXPECT_EQ("(a()b)", CString("(a()a)").Replace_n("a", "b", "(", ")"));
	EXPECT_EQ("a(b)", CString("(a()a)").Replace_n("a", "b", "(", ")", true));
}

TEST(StringTest, Misc) {
	EXPECT_EQ("Hello,...", CString("Hello, I'm Bob").Ellipsize(9));
	EXPECT_EQ("Hello, I'm Bob", CString("Hello, I'm Bob").Ellipsize(90));
	EXPECT_EQ("..", CString("Hello, I'm Bob").Ellipsize(2));

	EXPECT_EQ("Xy", CS("Xyz").Left(2));
	EXPECT_EQ("Xyz", CS("Xyz").Left(20));

	EXPECT_EQ("yz", CS("Xyz").Right(2));
	EXPECT_EQ("Xyz", CS("Xyz").Right(20));
}

TEST(StringTest, Split) {
	EXPECT_EQ("a", CS("a b c").Token(0));
	EXPECT_EQ("b", CS("a b c").Token(1));
	EXPECT_EQ("", CS("a b c").Token(100));
	EXPECT_EQ("b c", CS("a b c").Token(1, true));
	EXPECT_EQ("c", CS("a  c").Token(1));
	EXPECT_EQ("", CS("a  c").Token(1, false, " ", true));
	EXPECT_EQ("c", CS("a  c").Token(1, false, "  "));
	EXPECT_EQ(" c", CS("a   c").Token(1, false, "  "));
	EXPECT_EQ("c", CS("a    c").Token(1, false, "  "));
	EXPECT_EQ("b c", CS("a (b c) d").Token(1, false, " ", false, "(", ")"));
	EXPECT_EQ("(b c)", CS("a (b c) d").Token(1, false, " ", false, "(", ")", false));
	EXPECT_EQ("d", CS("a (b c) d").Token(2, false, " ", false, "(", ")", false));

	VCString vexpected;
	VCString vresult;

	vexpected.push_back("a");
	vexpected.push_back("b");
	vexpected.push_back("c");
	CS("a b c").Split(" ", vresult);
	EXPECT_EQ(vexpected, vresult);

	MCString mexpected;
	MCString mresult;

	mexpected["a"] = "b";
	mexpected["c"] = "d";
	CS("a=x&c=d&a=b").URLSplit(mresult);
	EXPECT_EQ(mexpected, mresult) << "URLSplit";
}

TEST(StringTest, NamedFormat) {
	MCString m;
	m["a"] = "b";
	EXPECT_EQ("{xbyb", CString::NamedFormat(CS("\\{x{a}y{a}"), m));
}

TEST(StringTest, Hash) {
	EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", CS("").MD5());
	EXPECT_EQ("0cc175b9c0f1b6a831c399e269772661", CS("a").MD5());

	EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", CS("").SHA256());
	EXPECT_EQ("ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb", CS("a").SHA256());
}
