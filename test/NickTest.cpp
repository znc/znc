/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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
#include <znc/Nick.h>

TEST(NickTest, Parse) {
    CNick Nick1("nick!~ident@host");
    EXPECT_EQ(Nick1.GetNick(), "nick");
    EXPECT_EQ(Nick1.GetIdent(), "~ident");
    EXPECT_EQ(Nick1.GetHost(), "host");
    EXPECT_EQ(Nick1.GetNickMask(), "nick!~ident@host");
    EXPECT_EQ(Nick1.GetHostMask(), "nick!~ident@host");
    EXPECT_TRUE(Nick1.NickEquals("nick"));

    CNick Nick2(":nick!~ident@host");
    EXPECT_EQ(Nick2.GetNick(), "nick");
    EXPECT_EQ(Nick2.GetIdent(), "~ident");
    EXPECT_EQ(Nick2.GetHost(), "host");
    EXPECT_EQ(Nick2.GetNickMask(), "nick!~ident@host");
    EXPECT_EQ(Nick2.GetHostMask(), "nick!~ident@host");
    EXPECT_TRUE(Nick2.NickEquals("nick"));
}
