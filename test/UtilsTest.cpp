/*
 * Copyright (C) 2004-2022 ZNC, see the NOTICE file for details.
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
#include <znc/FileUtils.h>
#include <znc/Utils.h>

TEST(IRC32, GetMessageTags) {
    EXPECT_EQ(CUtils::GetMessageTags(""), MCString());
    EXPECT_EQ(CUtils::GetMessageTags(
                  ":nick!ident@host PRIVMSG #chan :hello world"), MCString());

    MCString exp = {{"a", "b"}};
    EXPECT_EQ(CUtils::GetMessageTags("@a=b"), exp);
    EXPECT_EQ(CUtils::GetMessageTags(
                  "@a=b :nick!ident@host PRIVMSG #chan :hello world"), exp);
    EXPECT_EQ(CUtils::GetMessageTags("@a=b :rest"), exp);
    exp.clear();

    exp = {{"ab", "cdef"}, {"znc.in/gh-ij", "klmn,op"}};
    EXPECT_EQ(CUtils::GetMessageTags("@ab=cdef;znc.in/gh-ij=klmn,op :rest"),
              exp);

    exp = {{"a", "==b=="}};
    EXPECT_EQ(CUtils::GetMessageTags("@a===b== :rest"), exp);
    exp.clear();

    exp = {{"a", ""}, {"b", "c"}, {"d", ""}};
    EXPECT_EQ(CUtils::GetMessageTags("@a;b=c;d :rest"), exp);

    exp = {{"semi-colon", ";"}, {"space", " "}, {"NUL", {'\0'}},
           {"backslash", "\\"}, {"CR", {'\r'}}, {"LF", {'\n'}}};
    EXPECT_EQ(
        CUtils::GetMessageTags(
            R"(@semi-colon=\:;space=\s;NUL=\0;backslash=\\;CR=\r;LF=\n :rest)"),
        exp);
    exp.clear();

    exp = {{"a", "; \\\r\n"}};
    EXPECT_EQ(CUtils::GetMessageTags(R"(@a=\:\s\\\r\n :rest)"), exp);
    exp.clear();
}

TEST(IRC32, SetMessageTags) {
    CString sLine;

    sLine = ":rest";
    CUtils::SetMessageTags(sLine, MCString());
    EXPECT_EQ(sLine, ":rest");

    MCString tags = {{"a", "b"}};
    CUtils::SetMessageTags(sLine, tags);
    EXPECT_EQ(sLine, "@a=b :rest");

    tags = {{"a", "b"}, {"c", "d"}};
    CUtils::SetMessageTags(sLine, tags);
    EXPECT_EQ(sLine, "@a=b;c=d :rest");

    tags = {{"a", "b"}, {"c", "d"}, {"e", ""}};
    CUtils::SetMessageTags(sLine, tags);
    EXPECT_EQ(sLine, "@a=b;c=d;e :rest");

    tags = {{"semi-colon", ";"}, {"space", " "}, {"NUL", {'\0'}},
            {"backslash", "\\"}, {"CR", {'\r'}}, {"LF", {'\n'}}};
    CUtils::SetMessageTags(sLine, tags);
    EXPECT_EQ(
        sLine,
        R"(@CR=\r;LF=\n;NUL=\0;backslash=\\;semi-colon=\:;space=\s :rest)");

    tags = {{"a", "; \\\r\n"}};
    CUtils::SetMessageTags(sLine, tags);
    EXPECT_EQ(sLine, R"(@a=\:\s\\\r\n :rest)");
}

TEST(UtilsTest, ServerTime) {
    char* oldTZ = getenv("TZ");
    if (oldTZ) oldTZ = strdup(oldTZ);
    setenv("TZ", "UTC", 1);
    tzset();

    timeval tv1 = CUtils::ParseServerTime("2011-10-19T16:40:51.620Z");
    CString str1 = CUtils::FormatServerTime(tv1);
    EXPECT_EQ(str1, "2011-10-19T16:40:51.620Z");

    timeval now = CUtils::GetTime();

    // Strip microseconds, server time is ms only
    now.tv_usec = (now.tv_usec / 1000) * 1000;

    CString str2 = CUtils::FormatServerTime(now);
    timeval tv2 = CUtils::ParseServerTime(str2);
    EXPECT_EQ(tv2.tv_sec, now.tv_sec);
    EXPECT_EQ(tv2.tv_usec, now.tv_usec);

    timeval tv3 = CUtils::ParseServerTime("invalid");
    CString str3 = CUtils::FormatServerTime(tv3);
    EXPECT_EQ(str3, "1970-01-01T00:00:00.000Z");

    if (oldTZ) {
        setenv("TZ", oldTZ, 1);
        free(oldTZ);
    } else {
        unsetenv("TZ");
    }
    tzset();

}

TEST(UtilsTest, ParseServerTime) {
    char* oldTZ = getenv("TZ");
    if (oldTZ) oldTZ = strdup(oldTZ);
    setenv("TZ", "America/Montreal", 1);
    tzset();

    timeval tv4 = CUtils::ParseServerTime("2011-10-19T16:40:51.620Z");
    CString str4 = CUtils::FormatServerTime(tv4);
    EXPECT_EQ(str4, "2011-10-19T16:40:51.620Z");


    if (oldTZ) {
        setenv("TZ", oldTZ, 1);
        free(oldTZ);
    } else {
        unsetenv("TZ");
    }
    tzset();
}

class TimeTest : public testing::TestWithParam<
                     std::tuple<timeval, CString, CString, CString>> {};

TEST_P(TimeTest, FormatTime) {
    timeval tv = std::get<0>(GetParam());
    EXPECT_EQ(std::get<1>(GetParam()), CUtils::FormatTime(tv, "%s.%f", "UTC"));
    EXPECT_EQ(std::get<2>(GetParam()), CUtils::FormatTime(tv, "%s.%6f", "UTC"));
    EXPECT_EQ(std::get<3>(GetParam()), CUtils::FormatTime(tv, "%s.%9f", "UTC"));
}

INSTANTIATE_TEST_CASE_P(
    TimeTest, TimeTest,
    testing::Values(
        // leading zeroes
        std::make_tuple(timeval{42, 12345}, "42.012", "42.012345", "42.012345000"),
        // (no) rounding
        std::make_tuple(timeval{42, 999999}, "42.999", "42.999999", "42.999999000"),
        // no tv_usec part
        std::make_tuple(timeval{42, 0}, "42.000", "42.000000", "42.000000000")));

TEST(UtilsTest, FormatTime) {
    // Test passthrough
    timeval tv1;
    tv1.tv_sec = 42;
    tv1.tv_usec = 123456;
    CString str1 = CUtils::FormatTime(tv1, "%s", "UTC");
    EXPECT_EQ(str1, "42");

    // Test escapes
    timeval tv2;
    tv2.tv_sec = 42;
    tv2.tv_usec = 123456;
    CString str2 = CUtils::FormatTime(tv2, "%%f", "UTC");
    EXPECT_EQ(str2, "%f");

    // Test suffix
    CString str3 = CUtils::FormatTime(tv2, "a%fb", "UTC");
    EXPECT_EQ(str3, "a123b");
}
