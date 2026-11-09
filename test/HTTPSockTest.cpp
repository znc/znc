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

#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
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
// - emit a no-store Cache-Control for dynamic responses, but skip it for
//   304 and for static asset MIME types whose freshness is handled by
//   ETag/Last-Modified;
// - never duplicate a header the caller already added via AddHeader;
// - skip a header entirely when the caller calls OmitHardeningHeader.
TEST_F(HTTPSockHeadersTest, HardeningHeadersDefaultDynamicResponse) {
    CCapturingHTTPSock sock;
    sock.PrintHeader(0, "text/html");
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Frame-Options: SAMEORIGIN")));
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Content-Type-Options: nosniff")));
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Referrer-Policy: no-referrer\r\n")));
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Cache-Control: no-store\r\n")));
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersSkipCacheControlOn304) {
    CCapturingHTTPSock sock;
    sock.PrintHeader(0, "text/html", 304, "Not Modified");
    EXPECT_THAT(sock.m_vsLines, Contains(StartsWith("X-Frame-Options:")));
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Cache-Control:"))));
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
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Referrer-Policy: no-referrer\r\n")));
}

TEST_F(HTTPSockHeadersTest, HardeningHeadersDeferToCallerCacheControl) {
    CCapturingHTTPSock sock;
    sock.AddHeader("Cache-Control", "max-age=300");
    sock.PrintHeader(0, "text/html");
    // No no-store default; only the caller's value should be emitted.
    EXPECT_THAT(sock.m_vsLines, Not(Contains(StartsWith("Cache-Control: no-store"))));
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Cache-Control: max-age=300\r\n")));
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
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Referrer-Policy: no-referrer\r\n")));
}

// Returns the server side of a loopback TCP connection, so that a socket
// handed this fd reports 127.0.0.1 as its peer.
static int LoopbackConnection() {
    int iListen = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    EXPECT_EQ(bind(iListen, (sockaddr*)&addr, len), 0);
    EXPECT_EQ(listen(iListen, 1), 0);
    EXPECT_EQ(getsockname(iListen, (sockaddr*)&addr, &len), 0);
    int iClient = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_EQ(connect(iClient, (sockaddr*)&addr, len), 0);
    int iServer = accept(iListen, nullptr, nullptr);
    close(iClient);
    close(iListen);
    return iServer;
}

// Repeated X-Forwarded-For lines form one list (RFC 7230 3.2.2), and some
// proxies (HAProxy's "option forwardfor") append their own line after
// whatever the client sent instead of extending it. The client's line must
// not be taken as the forwarded address.
TEST_F(HTTPSockHeadersTest, ForwardedForSpansHeaderLines) {
    CZNC::Get().AddTrustedProxy("127.0.0.1");
    CCapturingHTTPSock sock;
    sock.SetSock(LoopbackConnection());
    sock.ReadLine("GET / HTTP/1.1\r\n");
    sock.ReadLine("X-Forwarded-For: 203.0.113.5\r\n");   // sent by the client
    sock.ReadLine("X-Forwarded-For: 198.51.100.7\r\n");  // added by the proxy
    sock.ReadLine("\r\n");
    EXPECT_EQ(sock.GetRemoteIP(), "198.51.100.7");
}

TEST_F(HTTPSockHeadersTest, ForwardedForSingleLine) {
    CZNC::Get().AddTrustedProxy("127.0.0.1");
    CCapturingHTTPSock sock;
    sock.SetSock(LoopbackConnection());
    sock.ReadLine("GET / HTTP/1.1\r\n");
    sock.ReadLine("X-Forwarded-For: 203.0.113.5, 198.51.100.7\r\n");
    sock.ReadLine("\r\n");
    EXPECT_EQ(sock.GetRemoteIP(), "198.51.100.7");
}

// The collected X-Forwarded-For entries are bounded, so a client can't fill
// memory by repeating the header.
TEST_F(HTTPSockHeadersTest, ForwardedForIsBounded) {
    CCapturingHTTPSock sock;
    sock.ReadLine("GET / HTTP/1.1\r\n");
    for (int i = 0; i < 10000 && sock.m_vsLines.empty(); ++i) {
        sock.ReadLine("X-Forwarded-For: 203.0.113.5\r\n");
    }
    ASSERT_FALSE(sock.m_vsLines.empty());
    EXPECT_EQ(sock.m_vsLines.front(),
              "HTTP/1.1 431 Request Header Fields Too Large\r\n");
    EXPECT_EQ(sock.GetCloseType(), Csock::CLT_AFTERWRITE);
}

TEST_F(HTTPSockHeadersTest, ForwardedForEntryIsBounded) {
    CCapturingHTTPSock sock;
    sock.ReadLine("GET / HTTP/1.1\r\n");
    sock.ReadLine("X-Forwarded-For: " + CString(100, 'a') + "\r\n");
    ASSERT_FALSE(sock.m_vsLines.empty());
    EXPECT_EQ(sock.m_vsLines.front(),
              "HTTP/1.1 431 Request Header Fields Too Large\r\n");
    EXPECT_EQ(sock.GetCloseType(), Csock::CLT_AFTERWRITE);
}

namespace {

// Answers every request with a fixed page, so tests can tell whether the
// body was written.
class CPageHTTPSock : public CCapturingHTTPSock {
  public:
    void OnPageRequest(const CString& sURI) override {
        PrintPage("<p>hello</p>");
    }
};

}  // namespace

// HEAD is routed like GET, but the response must not include a body
// (RFC 9110 9.3.2). Monitoring tools rely on this to check the web UI.
TEST_F(HTTPSockHeadersTest, GetSendsBody) {
    CPageHTTPSock sock;
    sock.ReadLine("GET / HTTP/1.1\r\n");
    sock.ReadLine("\r\n");
    ASSERT_FALSE(sock.m_vsLines.empty());
    EXPECT_EQ(sock.m_vsLines.front(), "HTTP/1.1 200 OK\r\n");
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Content-Length: 12\r\n")));
    EXPECT_THAT(sock.m_vsLines, Contains(CString("<p>hello</p>")));
}

TEST_F(HTTPSockHeadersTest, HeadSendsHeadersWithoutBody) {
    CPageHTTPSock sock;
    sock.ReadLine("HEAD / HTTP/1.1\r\n");
    sock.ReadLine("\r\n");
    ASSERT_FALSE(sock.m_vsLines.empty());
    EXPECT_EQ(sock.m_vsLines.front(), "HTTP/1.1 200 OK\r\n");
    // Same headers as GET, including the length of the body that GET sends.
    EXPECT_THAT(sock.m_vsLines, Contains(CString("Content-Length: 12\r\n")));
    EXPECT_THAT(sock.m_vsLines, Not(Contains(CString("<p>hello</p>"))));
    EXPECT_EQ(sock.GetCloseType(), Csock::CLT_AFTERWRITE);
}

TEST_F(HTTPSockHeadersTest, HeadErrorPageHasNoBody) {
    CCapturingHTTPSock sock;
    sock.ReadLine("HEAD / HTTP/1.1\r\n");
    sock.ReadLine("\r\n");
    sock.m_vsLines.clear();
    sock.PrintNotFound();
    ASSERT_FALSE(sock.m_vsLines.empty());
    EXPECT_EQ(sock.m_vsLines.front(), "HTTP/1.1 404 Not Found\r\n");
    EXPECT_EQ(sock.m_vsLines.back(), "\r\n");
}
