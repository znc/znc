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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <znc/HTTPSock.h>
#include <znc/znc.h>

using ::testing::Contains;
using ::testing::Not;
using ::testing::StartsWith;

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

namespace {

// Minimal CHTTPSock subclass for tests: captures every Write() call as a
// separate vector entry, and stubs out the pure-virtual hooks. Each Write
// call from PrintHeader corresponds to one header line, so matchers like
// Contains(StartsWith("X-Frame-Options:")) work directly on m_vsLines.
class CCapturingHTTPSock : public CHTTPSock {
  public:
    CCapturingHTTPSock() : CHTTPSock(nullptr, "") {}

    void OnPageRequest(const CString& sURI) override {}
    Csock* GetSockObj(const CString& sHost, unsigned short uPort) override {
        return nullptr;
    }
    // PrintHeader writes one CString per header line; capture each call.
    using CHTTPSock::Write;
    bool Write(const CString& sData) override {
        m_vsLines.push_back(sData);
        return true;
    }

    VCString m_vsLines;
};

class HTTPSockHeadersTest : public ::testing::Test {
  protected:
    HTTPSockHeadersTest() { CZNC::CreateInstance(); }
    ~HTTPSockHeadersTest() override { CZNC::DestroyInstance(); }
};

}  // namespace

// Hardening response headers introduced for #2012. The fix's contract:
// - emit X-Frame-Options, X-Content-Type-Options, Referrer-Policy on every
//   response (unless the caller already set them or asked to omit them);
// - emit no-store Cache-Control + Pragma for dynamic responses, but skip
//   for 304 and for static asset MIME types whose freshness is handled by
//   ETag/Last-Modified;
// - never duplicate a header the caller already added via AddHeader;
// - skip a header entirely when the caller calls OmitHardeningHeader.
TEST_F(HTTPSockHeadersTest, HardeningHeadersDefaultDynamicResponse) {
    CCapturingHTTPSock sock;
    sock.PrintHeader(0, "text/html");
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Frame-Options: SAMEORIGIN")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Content-Type-Options: nosniff")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("Referrer-Policy: same-origin")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("Cache-Control:")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("Pragma: no-cache")));
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersSkipCacheControlOn304) {
    CCapturingHTTPSock sock;
    sock.PrintHeader(0, "text/html", 304, "Not Modified");
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Frame-Options:")));
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Cache-Control:"))));
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Pragma:"))));
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersSkipCacheControlForStaticAssets) {
    for (const CString& sCT : {CString("image/png"), CString("image/svg+xml"),
                                CString("font/woff2"), CString("text/css"),
                                CString("application/javascript")}) {
        CCapturingHTTPSock sock;
        sock.PrintHeader(0, sCT);
        EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Cache-Control:"))))
            << "for content type " << sCT;
        // Security headers still emitted even for static-like responses.
        EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Content-Type-Options: nosniff")))
            << "for content type " << sCT;
    }
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersDeferToCallerXFrameOptions) {
    // Caller-supplied X-Frame-Options should suppress the default so we
    // don't emit a duplicate (or worse, a conflicting) header.
    CCapturingHTTPSock sock;
    sock.AddHeader("X-Frame-Options", "DENY");
    sock.PrintHeader(0, "text/html");
    // The caller's value goes out via the m_msHeaders loop, not via the
    // hardening emitter, so the SAMEORIGIN default must not appear.
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("X-Frame-Options: SAMEORIGIN"))));
    EXPECT_THAT(sock.m_vsLines, Contains(CString("X-Frame-Options: DENY\r\n")));
    // Other defaults still emitted.
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Content-Type-Options: nosniff")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("Referrer-Policy: same-origin")));
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersDeferToCallerCacheControl) {
    CCapturingHTTPSock sock;
    sock.AddHeader("Cache-Control", "max-age=300");
    sock.PrintHeader(0, "text/html");
    // No no-store default; only the caller's value should be emitted.
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Cache-Control: no-store"))));
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Cache-Control: max-age=300\r\n")));
    // Pragma is independently controlled and still emitted as a default.
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("Pragma: no-cache")));
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersOmittedByCaller) {
    // OmitHardeningHeader skips the default outright (no value emitted).
    CCapturingHTTPSock sock;
    sock.OmitHardeningHeader("X-Frame-Options");
    sock.OmitHardeningHeader("Cache-Control");
    sock.PrintHeader(0, "text/html");
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("X-Frame-Options:"))));
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Cache-Control:"))));
    // Other defaults unaffected.
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Content-Type-Options: nosniff")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("Pragma: no-cache")));
}
