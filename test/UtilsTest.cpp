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

#include <gtest/gtest.h>
#include <znc/FileUtils.h>
#include <znc/Utils.h>

TEST(IRC32, GetMessageTags) {
	EXPECT_EQ(MCString(), CUtils::GetMessageTags(""));
	EXPECT_EQ(MCString(), CUtils::GetMessageTags(":nick!ident@host PRIVMSG #chan :hello world"));

	MCString exp = { {"a","b"} };
	EXPECT_EQ(exp, CUtils::GetMessageTags("@a=b"));
	EXPECT_EQ(exp, CUtils::GetMessageTags("@a=b :nick!ident@host PRIVMSG #chan :hello world"));
	EXPECT_EQ(exp, CUtils::GetMessageTags("@a=b :rest"));
	exp.clear();

	exp = { {"ab","cdef"}, {"znc.in/gh-ij","klmn,op"} };
	EXPECT_EQ(exp, CUtils::GetMessageTags("@ab=cdef;znc.in/gh-ij=klmn,op :rest"));

	exp = { {"a","==b=="} };
	EXPECT_EQ(exp, CUtils::GetMessageTags("@a===b== :rest"));
	exp.clear();

	exp = { {"a",""}, {"b","c"}, {"d",""} };
	EXPECT_EQ(exp, CUtils::GetMessageTags("@a;b=c;d :rest"));

	exp = { {"semi-colon",";"}, {"space"," "}, {"NUL",{'\0'}}, {"backslash","\\"}, {"CR",{'\r'}}, {"LF",{'\n'}} };
	EXPECT_EQ(exp, CUtils::GetMessageTags(R"(@semi-colon=\:;space=\s;NUL=\0;backslash=\\;CR=\r;LF=\n :rest)"));
	exp.clear();

	exp = { {"a","; \\\r\n"} };
	EXPECT_EQ(exp, CUtils::GetMessageTags(R"(@a=\:\s\\\r\n :rest)"));
	exp.clear();
}

TEST(IRC32, SetMessageTags) {
	CString sLine;

	sLine = ":rest";
	CUtils::SetMessageTags(sLine, MCString());
	EXPECT_EQ(":rest", sLine);

	MCString tags = { {"a","b"} };
	CUtils::SetMessageTags(sLine, tags);
	EXPECT_EQ("@a=b :rest", sLine);

	tags = { {"a","b"}, {"c","d"} };
	CUtils::SetMessageTags(sLine, tags);
	EXPECT_EQ("@a=b;c=d :rest", sLine);

	tags = { {"a","b"}, {"c","d"}, {"e",""} };
	CUtils::SetMessageTags(sLine, tags);
	EXPECT_EQ("@a=b;c=d;e :rest", sLine);

	tags = { {"semi-colon",";"}, {"space"," "}, {"NUL",{'\0'}}, {"backslash","\\"}, {"CR",{'\r'}}, {"LF",{'\n'}} };
	CUtils::SetMessageTags(sLine, tags);
	EXPECT_EQ(R"(@CR=\r;LF=\n;NUL=\0;backslash=\\;semi-colon=\:;space=\s :rest)", sLine);

	tags = { {"a","; \\\r\n"} };
	CUtils::SetMessageTags(sLine, tags);
	EXPECT_EQ(R"(@a=\:\s\\\r\n :rest)", sLine);
}

