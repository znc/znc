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
#include <znc/Client.h>
#include <znc/znc.h>

class ClientTest : public ::testing::Test {
protected:
	void SetUp() { CZNC::CreateInstance(); }
	void TearDown() { CZNC::DestroyInstance(); }
	void testPass(const CString& sInput, const CString& sUser, const CString& sIdentifier, const CString& sNetwork, const CString& sPass) const {
		CClient client;
		client.ParsePass(sInput);
		EXPECT_EQ(sUser, client.m_sUser);
		EXPECT_EQ(sIdentifier, client.m_sIdentifier);
		EXPECT_EQ(sNetwork, client.m_sNetwork);
		EXPECT_EQ(sPass, client.m_sPass);
	}

	void testUser(const CString& sInput, const CString& sUser, const CString& sIdentifier, const CString& sNetwork) const {
		CClient client;
		client.ParseUser(sInput);
		EXPECT_EQ(sUser, client.m_sUser);
		EXPECT_EQ(sIdentifier, client.m_sIdentifier);
		EXPECT_EQ(sNetwork, client.m_sNetwork);
	}
};

TEST_F(ClientTest, Pass) {
	testPass("p@ss#w0rd",                                 "",            "",           "",         "p@ss#w0rd");
	testPass("user:p@ss#w0rd",                            "user",        "",           "",         "p@ss#w0rd");
	testPass("user/net-work:p@ss#w0rd",                   "user",        "",           "net-work", "p@ss#w0rd");
	testPass("user@identifier:p@ss#w0rd",                 "user",        "identifier", "",         "p@ss#w0rd");
	testPass("user@identifier/net-work:p@ss#w0rd",        "user",        "identifier", "net-work", "p@ss#w0rd");

	testPass("user@znc.in:p@ss#w0rd",                     "user@znc.in", "",           "",         "p@ss#w0rd");
	testPass("user@znc.in/net-work:p@ss#w0rd",            "user@znc.in", "",           "net-work", "p@ss#w0rd");
	testPass("user@znc.in@identifier:p@ss#w0rd",          "user@znc.in", "identifier", "",         "p@ss#w0rd");
	testPass("user@znc.in@identifier/net-work:p@ss#w0rd", "user@znc.in", "identifier", "net-work", "p@ss#w0rd");
}

TEST_F(ClientTest, User) {
	testUser("user/net-work",                   "user",        "",           "net-work");
	testUser("user@identifier",                 "user",        "identifier", "");
	testUser("user@identifier/net-work",        "user",        "identifier", "net-work");

	testUser("user@znc.in/net-work",            "user@znc.in", "",           "net-work");
	testUser("user@znc.in@identifier",          "user@znc.in", "identifier", "");
	testUser("user@znc.in@identifier/net-work", "user@znc.in", "identifier", "net-work");
}
