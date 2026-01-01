/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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
#include <znc/IRCNetwork.h>
#include <znc/User.h>
#include <znc/znc.h>

class NetworkTest : public ::testing::Test {
  protected:
    void SetUp() override { CZNC::CreateInstance(); }
    void TearDown() override { CZNC::DestroyInstance(); }
};

TEST_F(NetworkTest, FindChan) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    EXPECT_TRUE(network.AddChan("#foo", false));
    EXPECT_TRUE(network.AddChan("#Bar", false));
    EXPECT_TRUE(network.AddChan("#BAZ", false));

    EXPECT_TRUE(network.FindChan("#foo"));
    EXPECT_TRUE(network.FindChan("#Bar"));
    EXPECT_TRUE(network.FindChan("#BAZ"));

    EXPECT_TRUE(network.FindChan("#Foo"));
    EXPECT_TRUE(network.FindChan("#BAR"));
    EXPECT_TRUE(network.FindChan("#baz"));

    EXPECT_FALSE(network.FindChan("#f"));
    EXPECT_FALSE(network.FindChan("&foo"));
    EXPECT_FALSE(network.FindChan("##foo"));
}

TEST_F(NetworkTest, FindChans) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    EXPECT_TRUE(network.AddChan("#foo", false));
    EXPECT_TRUE(network.AddChan("#Bar", false));
    EXPECT_TRUE(network.AddChan("#BAZ", false));

    EXPECT_EQ(network.FindChans("#f*").size(), 1u);
    EXPECT_EQ(network.FindChans("#b*").size(), 2u);
    EXPECT_EQ(network.FindChans("#?A*").size(), 2u);
    EXPECT_EQ(network.FindChans("*z").size(), 1u);
}

TEST_F(NetworkTest, FindQuery) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    EXPECT_TRUE(network.AddQuery("foo"));
    EXPECT_TRUE(network.AddQuery("Bar"));
    EXPECT_TRUE(network.AddQuery("BAZ"));

    EXPECT_TRUE(network.FindQuery("foo"));
    EXPECT_TRUE(network.FindQuery("Bar"));
    EXPECT_TRUE(network.FindQuery("BAZ"));

    EXPECT_TRUE(network.FindQuery("Foo"));
    EXPECT_TRUE(network.FindQuery("BAR"));
    EXPECT_TRUE(network.FindQuery("baz"));

    EXPECT_FALSE(network.FindQuery("f"));
    EXPECT_FALSE(network.FindQuery("fo"));
    EXPECT_FALSE(network.FindQuery("FF"));
}

TEST_F(NetworkTest, FindQueries) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    EXPECT_TRUE(network.AddQuery("foo"));
    EXPECT_TRUE(network.AddQuery("Bar"));
    EXPECT_TRUE(network.AddQuery("BAZ"));

    EXPECT_EQ(network.FindQueries("f*").size(), 1u);
    EXPECT_EQ(network.FindQueries("b*").size(), 2u);
    EXPECT_EQ(network.FindQueries("?A*").size(), 2u);
    EXPECT_EQ(network.FindQueries("*z").size(), 1u);
}
