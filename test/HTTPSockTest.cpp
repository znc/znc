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
#include <znc/HTTPSock.h>

// Validation contract used by AddHeader to keep CR/LF (and therefore
// response-splitting bytes) out of the response stream (#2010).
TEST(HTTPSockTest, IsValidHeaderField) {
    // Plain field names and values are accepted.
    EXPECT_TRUE(CHTTPSock::IsValidHeaderField(""));
    EXPECT_TRUE(CHTTPSock::IsValidHeaderField("X-Custom"));
    EXPECT_TRUE(CHTTPSock::IsValidHeaderField("text/html; charset=utf-8"));
    EXPECT_TRUE(CHTTPSock::IsValidHeaderField("a value with spaces and tabs\t"));

    // CR or LF anywhere is rejected; both halves of a CRLF pair are
    // rejected even individually.
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("\r"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("\n"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("\r\n"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("X\rFoo"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("X\nFoo"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("safe\r\nInjected: yes"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("trailing\n"));
    EXPECT_FALSE(CHTTPSock::IsValidHeaderField("\rleading"));
}
