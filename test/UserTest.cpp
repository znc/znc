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
#include <znc/User.h>
#include <znc/znc.h>

class UserTest : public ::testing::Test {
  protected:
    UserTest() { CZNC::CreateInstance(); }
    ~UserTest() { CZNC::DestroyInstance(); }
};

TEST_F(UserTest, IsHostAllowed) {
    CUser* m_pTestUser;

    struct hostTest {
        CString sTestHost;
        CString sIP;
        bool bExpectedResult;
    };

    hostTest aHostTests[] = {
        {"127.0.0.1", "127.0.0.1", true},
        {"127.0.0.20", "127.0.0.1", false},
        {"127.0.0.20/24", "127.0.0.1", true},
        {"127.0.0.20/0", "127.0.0.1", true},
        {"127.0.0.20/32", "127.0.0.1", false},

        {"127.0.0.1", "127.0.0.0", false},
        {"127.0.0.20", "127.0.0.0", false},
        {"127.0.0.20/24", "127.0.0.0", true},
        {"127.0.0.20/0", "127.0.0.0", true},
        {"127.0.0.20/32", "127.0.0.0", false},

        {"127.0.0.1", "127.0.0.255", false},
        {"127.0.0.20", "127.0.0.255", false},
        {"127.0.0.20/24", "127.0.0.255", true},
        {"127.0.0.20/0", "127.0.0.255", true},
        {"127.0.0.20/32", "127.0.0.255", false},

        {"127.0.0.1", "127.0.1.1", false},
        {"127.0.0.20", "127.0.1.1", false},
        {"127.0.0.20/24", "127.0.1.1", false},
        {"127.0.0.20/16", "127.0.1.1", true},
        {"127.0.0.20/0", "127.0.1.1", true},
        {"127.0.0.20/32", "127.0.1.1", false},

        {"127.0.0.1", "0.0.0.0", false},
        {"127.0.0.20", "0.0.0.0", false},
        {"127.0.0.20/24", "0.0.0.0", false},
        {"127.0.0.20/16", "0.0.0.0", false},
        {"127.0.0.20/0", "0.0.0.0", true},
        {"127.0.0.20/32", "0.0.0.0", false},

        {"127.0.0.1", "255.255.255.255", false},
        {"127.0.0.20", "255.255.255.255", false},
        {"127.0.0.20/24", "255.255.255.255", false},
        {"127.0.0.20/16", "255.255.255.255", false},
        {"127.0.0.20/0", "255.255.255.255", true},
        {"127.0.0.20/32", "255.255.255.255", false},

        {"127.0.0.1", "::1", false},
        {"::1", "::1", true},
        {"::20/120", "::1", true},
        {"::20/120", "::ffff", false},
        {"::ff/0", "::1", true},
        {"::ff/0", "127.0.0.1", false},
        {"127.0.0.20/0", "::1", false},

        {"127.0.0.1/-1", "127.0.0.1", false},
        {"::0/-1", "::0", false},
        {"127.0.0.a/0", "127.0.0.1", false},
        {"::g/0", "::0", false},
        {"127.0.0.1/0", "127.0.0.a", false},
        {"::0/0", "::g", false},

        {"::0/0/0", "::0", false},
        {"2001:db8::/33", "2001:db9:0::", false},
        {"2001:db8::/32", "2001:db8:8000::", true},
        {"2001:db8::/33", "2001:db8:8000::", false},
    };
    for (int i = 0; i < (int)(sizeof(aHostTests) / sizeof(aHostTests[0]));
         ++i) {
        m_pTestUser = new CUser("user");
        hostTest* h = aHostTests + i;
        m_pTestUser->AddAllowedHost(h->sTestHost);
        CString should = h->bExpectedResult ? "" : " not";
        CString error = "Allow-host string \"" + h->sTestHost + "\" should" +
                        should + " allow host \"" + h->sIP + "\"";
        EXPECT_EQ(m_pTestUser->IsHostAllowed(aHostTests[i].sIP),
                  aHostTests[i].bExpectedResult)
            << error;
        delete m_pTestUser;
    }
}
