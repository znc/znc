/*
 * Copyright (C) 2004-2020 ZNC, see the NOTICE file for details.
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
    void testEncode(const CString& in, const CString& expectedOut,
                    const CString& sformat) {
        CString::EEscape format = CString::ToEscape(sformat);
        CString out;

        SCOPED_TRACE("Format: " + sformat);

        // Encode, then decode again and check we still got the same string
        out = in.Escape_n(CString::EASCII, format);
        EXPECT_EQ(out, expectedOut);
        out = out.Escape_n(format, CString::EASCII);
        EXPECT_EQ(out, in);
    }

    void testString(const CString& in, const CString& url, const CString& html,
                    const CString& sql, const CString& tag) {
        SCOPED_TRACE("String: " + in);

        testEncode(in, url, "URL");
        testEncode(in, html, "HTML");
        testEncode(in, sql, "SQL");
        testEncode(in, tag, "MSGTAG");
    }
};

TEST_F(EscapeTest, Test) {
    // clang-format off
    //          input     url          html             sql         msgtag
    testString("abcdefg","abcdefg",   "abcdefg",       "abcdefg",   "abcdefg");
    testString("\n\t\r", "%0A%09%0D", "\n\t\r",        "\\n\\t\\r", "\\n\t\\r");
    testString("'\"",    "%27%22",    "'&quot;",       "\\'\\\"",   "'\"");
    testString("&<>",    "%26%3C%3E", "&amp;&lt;&gt;", "&<>",       "&<>");
    testString(" ;",     "+%3B",      " ;",            " ;",        "\\s\\:");
    // clang-format on
    EXPECT_EQ(CString("a&lt.b&gt;c").Escape_n(CString::EHTML, CString::EASCII),
              "a&lt.b>c");
}

TEST(StringTest, Bool) {
    EXPECT_TRUE(CString(true).ToBool());
    EXPECT_FALSE(CString(false).ToBool());
}

#define CS(s) (CString((s), sizeof(s) - 1))

TEST(StringTest, Cmp) {
    CString s = "Bbb";

    EXPECT_EQ(s, CString("Bbb"));
    EXPECT_LT(CString("Aaa"), s);
    EXPECT_GT(CString("Ccc"), s);
    EXPECT_EQ(s.StrCmp("Bbb"), 0);
    EXPECT_GT(0, s.StrCmp("bbb"));
    EXPECT_LT(0, s.StrCmp("Aaa"));
    EXPECT_GT(0, s.StrCmp("Ccc"));
    EXPECT_EQ(s.CaseCmp("Bbb"), 0);
    EXPECT_EQ(s.CaseCmp("bbb"), 0);
    EXPECT_LT(0, s.CaseCmp("Aaa"));
    EXPECT_GT(0, s.CaseCmp("Ccc"));

    EXPECT_TRUE(s.Equals("bbb"));
    EXPECT_FALSE(s.Equals("bbb", true));
    EXPECT_FALSE(s.Equals("bb"));
}

TEST(StringTest, Wild) {
    EXPECT_TRUE(CString::WildCmp("", "", CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("", "", CString::CaseInsensitive));

    EXPECT_FALSE(CString::WildCmp("*a*b*c*", "xy", CString::CaseSensitive));
    EXPECT_FALSE(CString::WildCmp("*a*b*c*", "xy", CString::CaseInsensitive));

    EXPECT_TRUE(CString::WildCmp("*!?bar@foo", "I_am!~bar@foo",
                                 CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*!?bar@foo", "I_am!~bar@foo",
                                 CString::CaseInsensitive));

    EXPECT_FALSE(CString::WildCmp("*!?BAR@foo", "I_am!~bar@foo",
                                  CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*!?BAR@foo", "I_am!~bar@foo",
                                 CString::CaseInsensitive));

    EXPECT_TRUE(CString::WildCmp("*a*b*c*", "abc", CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*a*b*c*", "abc", CString::CaseInsensitive));

    EXPECT_FALSE(CString::WildCmp("*A*b*c*", "abc", CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*A*b*c*", "abc", CString::CaseInsensitive));

    EXPECT_FALSE(CString::WildCmp("*a*b*c*", "Abc", CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*a*b*c*", "Abc", CString::CaseInsensitive));

    EXPECT_TRUE(CString::WildCmp("*a*b*c*", "axbyc", CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*a*b*c*", "axbyc", CString::CaseInsensitive));

    EXPECT_FALSE(CString::WildCmp("*a*B*c*", "AxByC", CString::CaseSensitive));
    EXPECT_TRUE(CString::WildCmp("*a*B*c*", "AxByC", CString::CaseInsensitive));
}

TEST(StringTest, Case) {
    CString x = CS("xx");
    CString X = CS("XX");
    EXPECT_EQ(x.AsUpper(), X);
    EXPECT_EQ(X.AsLower(), x);
}

TEST(StringTest, Replace) {
    EXPECT_EQ(CString("(a()a)").Replace_n("a", "b"), "(b()b)");
    EXPECT_EQ(CString("(a()a)").Replace_n("a", "b", "(", ")"), "(a()b)");
    EXPECT_EQ(CString("(a()a)").Replace_n("a", "b", "(", ")", true), "a(b)");
}

TEST(StringTest, Misc) {
    EXPECT_EQ(CString("Hello, I'm Bob").Ellipsize(9), "Hello,...");
    EXPECT_EQ(CString("Hello, I'm Bob").Ellipsize(90), "Hello, I'm Bob");
    EXPECT_EQ(CString("Hello, I'm Bob").Ellipsize(2), "..");

    EXPECT_EQ(CS("Xyz").Left(2), "Xy");
    EXPECT_EQ(CS("Xyz").Left(20), "Xyz");

    EXPECT_EQ(CS("Xyz").Right(2), "yz");
    EXPECT_EQ(CS("Xyz").Right(20), "Xyz");
}

TEST(StringTest, Split) {
    EXPECT_EQ(CS("a b c").Token(0), "a");
    EXPECT_EQ(CS("a b c").Token(1), "b");
    EXPECT_EQ(CS("a b c").Token(100), "");
    EXPECT_EQ(CS("a b c").Token(1, true), "b c");
    EXPECT_EQ(CS("a  c").Token(1), "c");
    EXPECT_EQ(CS("a  c").Token(1, false, " ", true), "");
    EXPECT_EQ(CS("a  c").Token(1, false, "  "), "c");
    EXPECT_EQ(CS("a   c").Token(1, false, "  "), " c");
    EXPECT_EQ(CS("a    c").Token(1, false, "  "), "c");
    EXPECT_EQ(CS("a (b c) d").Token(1, false, " ", false, "(", ")"), "b c");
    EXPECT_EQ(CS("a (b c) d").Token(1, false, " ", false, "(", ")", false),
              "(b c)");
    EXPECT_EQ(CS("a (b c) d").Token(2, false, " ", false, "(", ")", false),
              "d");

    VCString vexpected;
    VCString vresult;

    vexpected.push_back("a");
    vexpected.push_back("b");
    vexpected.push_back("c");
    CS("a b c").Split(" ", vresult);
    EXPECT_EQ(vresult, vexpected);

    MCString mexpected = {{"a", "b"}, {"c", "d"}};
    MCString mresult;

    CS("a=x&c=d&a=b").URLSplit(mresult);
    EXPECT_EQ(mexpected, mresult) << "URLSplit";
}

TEST(StringTest, NamedFormat) {
    MCString m = {{"a", "b"}};
    EXPECT_EQ(CString::NamedFormat(CS("\\{x{a}y{a}"), m), "{xbyb");
}

TEST(StringTest, Hash) {
    EXPECT_EQ(CS("").MD5(), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(CS("a").MD5(), "0cc175b9c0f1b6a831c399e269772661");

    EXPECT_EQ(
        CS("").SHA256(),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(
        CS("a").SHA256(),
        "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
}

TEST(StringTest, Equals) {
    EXPECT_TRUE(CS("ABC").Equals("abc"));
    EXPECT_TRUE(CS("ABC").Equals("abc", CString::CaseInsensitive));
    EXPECT_FALSE(CS("ABC").Equals("abc", CString::CaseSensitive));
    EXPECT_TRUE(CS("ABC").Equals("abc", false));  // deprecated
    EXPECT_FALSE(CS("ABC").Equals("abc", true));  // deprecated
}

TEST(StringTest, Find) {
    EXPECT_EQ(CString("Hello, I'm Bob").Find("Hello"), 0u);
    EXPECT_EQ(CString("Hello, I'm Bob").Find("Hello", CString::CaseInsensitive),
              0u);
    EXPECT_EQ(CString("Hello, I'm Bob").Find("Hello", CString::CaseSensitive),
              0u);

    EXPECT_EQ(CString("Hello, I'm Bob").Find("i'm"), 7u);
    EXPECT_EQ(CString("Hello, I'm Bob").Find("i'm", CString::CaseInsensitive),
              7u);
    EXPECT_EQ(CString("Hello, I'm Bob").Find("i'm", CString::CaseSensitive),
              CString::npos);
}

TEST(StringTest, StartsWith) {
    EXPECT_TRUE(CString("Hello, I'm Bob").StartsWith("Hello"));
    EXPECT_TRUE(CString("Hello, I'm Bob")
                    .StartsWith("Hello", CString::CaseInsensitive));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").StartsWith("Hello", CString::CaseSensitive));

    EXPECT_TRUE(CString("Hello, I'm Bob").StartsWith("hello"));
    EXPECT_TRUE(CString("Hello, I'm Bob")
                    .StartsWith("hello", CString::CaseInsensitive));
    EXPECT_FALSE(
        CString("Hello, I'm Bob").StartsWith("hello", CString::CaseSensitive));
}

TEST(StringTest, EndsWith) {
    EXPECT_TRUE(CString("Hello, I'm Bob").EndsWith("Bob"));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").EndsWith("Bob", CString::CaseInsensitive));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").EndsWith("Bob", CString::CaseSensitive));

    EXPECT_TRUE(CString("Hello, I'm Bob").EndsWith("bob"));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").EndsWith("bob", CString::CaseInsensitive));
    EXPECT_FALSE(
        CString("Hello, I'm Bob").EndsWith("bob", CString::CaseSensitive));
}

TEST(StringTest, Contains) {
    EXPECT_TRUE(CString("Hello, I'm Bob").Contains("Hello"));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").Contains("Hello", CString::CaseInsensitive));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").Contains("Hello", CString::CaseSensitive));

    EXPECT_TRUE(CString("Hello, I'm Bob").Contains("i'm"));
    EXPECT_TRUE(
        CString("Hello, I'm Bob").Contains("i'm", CString::CaseInsensitive));
    EXPECT_FALSE(
        CString("Hello, I'm Bob").Contains("i'm", CString::CaseSensitive));

    EXPECT_TRUE(CString("Hello, I'm Bob").Contains("i'm bob"));
    EXPECT_TRUE(CString("Hello, I'm Bob")
                    .Contains("i'm bob", CString::CaseInsensitive));
    EXPECT_FALSE(
        CString("Hello, I'm Bob").Contains("i'm bob", CString::CaseSensitive));
}

TEST(StringTest, StripControls) {
    // Strips reset colours
    EXPECT_EQ(CString("\x03test").StripControls(), "test");

    // Strips reset foreground and set new background colour
    EXPECT_EQ(CString("\x03,03test").StripControls(), "test");

    // Strips foreground and background colour
    EXPECT_EQ(CString("\x03\x30,03test").StripControls(), "test");

    // Strips reset
    EXPECT_EQ(CString("\x0Ftest").StripControls(), "test");

    // Strips reverse
    EXPECT_EQ(CString("\x12test").StripControls(), "test");

    // Strips bold
    EXPECT_EQ(CString("\x02test").StripControls(), "test");

    // Strips italics
    EXPECT_EQ(CString("\x16test").StripControls(), "test");

    // Strips underline
    EXPECT_EQ(CString("\x1Ftest").StripControls(), "test");
}
