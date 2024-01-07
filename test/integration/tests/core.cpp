/*
 * Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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

#include "znctest.h"

using testing::HasSubstr;
using testing::ContainsRegex;

namespace znc_inttest {
namespace {

TEST(Config, AlreadyExists) {
    QTemporaryDir dir;
    WriteConfig(dir.path());
    Process p(ZNC_BIN_DIR "/znc", QStringList() << "--debug"
                                                << "--datadir" << dir.path()
                                                << "--makeconf");
    p.ReadUntil("already exists");
    p.CanDie();
}

TEST_F(ZNCTest, Connect) {
    auto znc = Run();

    auto ircd = ConnectIRCd();
    ircd.ReadUntil("CAP LS");

    auto client = ConnectClient();
    client.Write("PASS :hunter2");
    client.Write("NICK nick");
    client.Write("USER user/test x x :x");
    client.ReadUntil("Welcome");
    client.Close();

    client = ConnectClient();
    client.Write("PASS :user:hunter2");
    client.Write("NICK nick");
    client.Write("USER u x x x");
    client.ReadUntil("Welcome");
    client.Close();

    client = ConnectClient();
    client.Write("NICK nick");
    client.Write("USER user x x x");
    client.ReadUntil("Configure your client to send a server password");
    client.Close();

    ircd.Write(":server 001 nick :Hello");
    ircd.ReadUntil("WHO");
}

TEST_F(ZNCTest, Channel) {
    auto znc = Run();
    auto ircd = ConnectIRCd();

    auto client = LoginClient();
    client.ReadUntil("Welcome");
    client.Write("JOIN #znc");
    client.Close();

    ircd.Write(":server 001 nick :Hello");
    ircd.ReadUntil("JOIN #znc");
    ircd.Write(":nick JOIN :#znc");
    ircd.Write(":server 353 nick #znc :nick");
    ircd.Write(":server 366 nick #znc :End of /NAMES list");
    ircd.Write(":server PING :1");
    ircd.ReadUntil("PONG 1");

    client = LoginClient();
    client.ReadUntil(":nick JOIN :#znc");
}

TEST_F(ZNCTest, HTTP) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto reply = HttpGet(QNetworkRequest(QUrl("http://127.0.0.1:12345/")));
    EXPECT_THAT(reply->rawHeader("Server").toStdString(), HasSubstr("ZNC"));
}

TEST_F(ZNCTest, FixCVE20149403) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":server 005 nick CHANTYPES=# :supports");
    ircd.Write(":server PING :1");
    ircd.ReadUntil("PONG 1");

    QNetworkRequest request;
    request.setRawHeader("Authorization",
                         "Basic " + QByteArray("user:hunter2").toBase64());
    request.setUrl(QUrl("http://127.0.0.1:12345/mods/global/webadmin/addchan"));
    HttpPost(request, {
                          {"user", "user"},
                          {"network", "test"},
                          {"submitted", "1"},
                          {"name", "znc"},
                          {"enabled", "1"},
                      });
    EXPECT_THAT(HttpPost(request,
                         {
                             {"user", "user"},
                             {"network", "test"},
                             {"submitted", "1"},
                             {"name", "znc"},
                             {"enabled", "1"},
                         })
                    ->readAll()
                    .toStdString(),
                HasSubstr("Channel [#znc] already exists"));
}

TEST_F(ZNCTest, FixFixOfCVE20149403) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN @#znc");
    ircd.ReadUntil("MODE @#znc");
    ircd.Write(":server 005 nick STATUSMSG=@ :supports");
    ircd.Write(":server PING :12345");
    ircd.ReadUntil("PONG 12345");

    QNetworkRequest request;
    request.setRawHeader("Authorization",
                         "Basic " + QByteArray("user:hunter2").toBase64());
    request.setUrl(QUrl("http://127.0.0.1:12345/mods/global/webadmin/addchan"));
    auto reply = HttpPost(request, {
                                       {"user", "user"},
                                       {"network", "test"},
                                       {"submitted", "1"},
                                       {"name", "@#znc"},
                                       {"enabled", "1"},
                                   });
    EXPECT_THAT(reply->readAll().toStdString(),
                HasSubstr("Could not add channel [@#znc]"));
}

TEST_F(ZNCTest, InvalidConfigInChan) {
    QFile conf(m_dir.path() + "/configs/znc.conf");
    ASSERT_TRUE(conf.open(QIODevice::Append | QIODevice::Text));
    QTextStream out(&conf);
    out << R"(
        <User foo>
            <Network bar>
                <Chan #baz>
                    Invalid = Line
                </Chan>
            </Network>
        </User>
    )";
    out.flush();
    auto znc = Run();
    znc->ShouldFinishItself(1);
}

TEST_F(ZNCTest, Encoding) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    ircd.Write(":server 001 nick :hello");
    // legacy
    ircd.Write(":n!u@h PRIVMSG nick :Hello\xE6world");
    client.ReadUntil("Hello\xE6world");
    client.Write("PRIVMSG *controlpanel :SetNetwork Encoding $me $net UTF-8");
    client.ReadUntil("Encoding = UTF-8");
    ircd.Write(":n!u@h PRIVMSG nick :Hello\xE6world");
    client.ReadUntil("Hello\xEF\xBF\xBDworld");
    client.Write(
        "PRIVMSG *controlpanel :SetNetwork Encoding $me $net ^CP-1251");
    client.ReadUntil("Encoding = ^CP-1251");
    ircd.Write(":n!u@h PRIVMSG nick :Hello\xE6world");
    client.ReadUntil("Hello\xD0\xB6world");
    ircd.Write(":n!u@h PRIVMSG nick :Hello\xD0\xB6world");
    client.ReadUntil("Hello\xD0\xB6world");
}

TEST_F(ZNCTest, BuildMod) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    QTemporaryDir srcd;
    QDir srcdir(srcd.path());
    QFile file(srcdir.filePath("testmod.cpp"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << R"(
        #include <znc/Modules.h>
        class TestModule : public CModule {
          public:
            MODCONSTRUCTOR(TestModule) {}
            void OnModCommand(const CString& sLine) override {
                PutModule("Lorem ipsum");
            }
        };
        MODULEDEFS(TestModule, "Test")
    )";
    file.close();
    QDir dir(m_dir.path());
    EXPECT_TRUE(dir.mkdir("modules"));
    EXPECT_TRUE(dir.cd("modules"));
    {
        Process p(ZNC_BIN_DIR "/znc-buildmod",
                  QStringList() << srcdir.filePath("file-not-found.cpp"),
                  [&](QProcess* proc) {
                      proc->setWorkingDirectory(dir.absolutePath());
                      proc->setProcessChannelMode(QProcess::ForwardedChannels);
                  });
        p.ShouldFinishItself(1);
        p.ShouldFinishInSec(300);
    }
    {
        Process p(ZNC_BIN_DIR "/znc-buildmod",
                  QStringList() << srcdir.filePath("testmod.cpp"),
                  [&](QProcess* proc) {
                      proc->setWorkingDirectory(dir.absolutePath());
                      proc->setProcessChannelMode(QProcess::ForwardedChannels);
                  });
        p.ShouldFinishItself();
        p.ShouldFinishInSec(300);
    }
    client.Write("znc loadmod testmod");
    client.Write("PRIVMSG *testmod :hi");
    client.ReadUntil("Lorem ipsum");
}

TEST_F(ZNCTest, AwayNotify) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = ConnectClient();
    client.Write("CAP LS");
    client.Write("PASS :hunter2");
    client.Write("NICK nick");
    client.Write("USER user/test x x :x");
    QByteArray cap_ls;
    client.ReadUntilAndGet(" LS :", cap_ls);
    ASSERT_THAT(cap_ls.toStdString(),
                AllOf(HasSubstr("cap-notify"), Not(HasSubstr("away-notify"))));
    client.Write("CAP REQ :cap-notify");
    client.ReadUntil("ACK :cap-notify");
    client.Write("CAP END");
    client.ReadUntil(" 001 ");
    ircd.ReadUntil("USER");
    ircd.Write("CAP user LS :away-notify");
    ircd.ReadUntil("CAP REQ :away-notify");
    ircd.Write("CAP user ACK :away-notify");
    ircd.ReadUntil("CAP END");
    ircd.Write(":server 001 user :welcome");
    client.ReadUntil("CAP user NEW :away-notify");
    client.Write("CAP REQ :away-notify");
    client.ReadUntil("ACK :away-notify");
    ircd.Write(":x!y@z AWAY :reason");
    client.ReadUntil(":x!y@z AWAY :reason");
    ircd.Close();
    client.ReadUntil("DEL :away-notify");
    // This test often fails on macos due to ZNC process not finishing.
    // No idea why. Let's try to shutdown it more explicitly...
    client.Write("znc shutdown");
}

TEST_F(ZNCTest, CAP302LSWaitFull) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    ircd.ReadUntil("CAP LS 302");
    ircd.Write("CAP user LS * :away-notify");
    ASSERT_THAT(ircd.ReadRemainder().toStdString(), Not(HasSubstr("away-notify")));
    ircd.Write("CAP user LS :blahblah");
    ircd.ReadUntil("CAP REQ :away-notify");
}

TEST_F(ZNCTest, CAP302NewDel) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    ircd.Write("CAP nick LS :blahblah");
    ircd.ReadUntil("CAP END");
    ircd.Write(":server 001 nick :Hello");
    client.Write("CAP REQ :away-notify");
    client.ReadUntil("NAK :away-notify");
    client.Write("CAP REQ :cap-notify");
    client.ReadUntil("ACK :cap-notify");
    ircd.Write("CAP nick NEW :away-notify");
    ircd.ReadUntil("CAP REQ :away-notify");
    ircd.Write("CAP nick ACK :away-notify");
    client.ReadUntil("CAP nick NEW :away-notify");
    ircd.Write("CAP nick DEL :away-notify");
    client.ReadUntil("CAP nick DEL :away-notify");
}

TEST_F(ZNCTest, JoinKey) {
    QFile conf(m_dir.path() + "/configs/znc.conf");
    ASSERT_TRUE(conf.open(QIODevice::Append | QIODevice::Text));
    QTextStream(&conf) << "ServerThrottle = 1\n";
    auto znc = Run();

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    ircd.Write(":server 001 nick :Hello");
    client.Write("JOIN #znc secret");
    ircd.ReadUntil("JOIN #znc secret");
    ircd.Write(":nick JOIN :#znc");
    client.ReadUntil("JOIN :#znc");
    ircd.Close();

    ircd = ConnectIRCd();
    ircd.Write(":server 001 nick :Hello");
    ircd.ReadUntil("JOIN #znc secret");
}

TEST_F(ZNCTest, StatusEchoMessage) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("CAP REQ :echo-message");
    client.Write("PRIVMSG *status :blah");
    client.ReadUntil(":nick!user@irc.znc.in PRIVMSG *status :blah");
    client.ReadUntil(":*status!status@znc.in PRIVMSG nick :Unknown command");
    client.Write("znc delnetwork test");
    client.ReadUntil("Network deleted");
    auto client2 = LoginClient();
    client2.Write("PRIVMSG *status :blah2");
    client2.ReadUntil(":*status!status@znc.in PRIVMSG nick :Unknown command");
    auto client3 = LoginClient();
    client3.Write("PRIVMSG *status :blah3");
    client3.ReadUntil(":*status!status@znc.in PRIVMSG nick :Unknown command");
}

TEST_F(ZNCTest, MoveChannels) {
    auto znc = Run();
    auto ircd = ConnectIRCd();

    auto client = LoginClient();
    client.Write("JOIN #foo,#bar");
    client.Close();

    ircd.Write(":server 001 nick :Hello");
    ircd.ReadUntil("JOIN #foo,#bar");
    ircd.Write(":nick JOIN :#foo");
    ircd.Write(":server 353 nick #foo :nick");
    ircd.Write(":server 366 nick #foo :End of /NAMES list");
    ircd.Write(":nick JOIN :#bar");
    ircd.Write(":server 353 nick #bar :nick");
    ircd.Write(":server 366 nick #bar :End of /NAMES list");

    client = LoginClient();
    client.ReadUntil(":nick JOIN :#foo");
    client.ReadUntil(":nick JOIN :#bar");
    client.Write("znc movechan #foo 2");
    client.ReadUntil("Moved channel #foo to index 2");
    client.Close();

    client = LoginClient();
    client.ReadUntil(":nick JOIN :#bar");
    client.ReadUntil(":nick JOIN :#foo");
    client.Write("znc swapchans #foo #bar");
    client.ReadUntil("Swapped channels #foo and #bar");
    client.Close();

    client = LoginClient();
    client.ReadUntil(":nick JOIN :#foo");
    client.ReadUntil(":nick JOIN :#bar");
}

TEST_F(ZNCTest, DenyOptions) {
    auto znc = Run();
    auto ircd = ConnectIRCd();

    auto client1 = LoginClient();
    client1.Write("PRIVMSG *controlpanel :CloneUser user user2");
    client1.ReadUntil("User user2 added!");
    client1.Write("PRIVMSG *controlpanel :Set Admin user2 false");
    client1.ReadUntil("Admin = false");
    client1.Write("PRIVMSG *controlpanel :Set MaxNetworks user2 5");
    client1.ReadUntil("MaxNetworks = 5");

    auto client2 = ConnectClient();
    client2.Write("PASS :hunter2");
    client2.Write("NICK nick2");
    client2.Write("USER user2/test x x :x");

    // DenySetNetwork
    // This is false by default so we should be able to add/delete networks/servers
    client2.Write("PRIVMSG *controlpanel :AddNetwork user2 test2");
    client2.ReadUntil("Network test2 added to user user2.");
    client2.Write("PRIVMSG *controlpanel :AddServer user2 test2 127.0.0.1");
    client2.ReadUntil("Added IRC Server 127.0.0.1 to network test2 for user user2.");
    client2.Write("PRIVMSG *controlpanel :DelServer user2 test2 127.0.0.1");
    client2.ReadUntil("Deleted IRC Server 127.0.0.1 from network test2 for user user2.");
    client2.Write("PRIVMSG *controlpanel :DelNetwork user2 test2");
    client2.ReadUntil("Network test2 deleted for user user2.");

    // Set it to true
    client1.Write("PRIVMSG *controlpanel :Set DenySetNetwork user2 true");
    client1.ReadUntil("DenySetNetwork = true");

    // Now we should be denied
    client2.Write("PRIVMSG *controlpanel :AddNetwork user2 test2");
    client2.ReadUntil("Access denied!");
    client2.Write("PRIVMSG *controlpanel :AddServer user2 test 127.0.0.2");
    client2.ReadUntil("Access denied!");
    client2.Write("PRIVMSG *controlpanel :DelServer user2 test 127.0.0.1");
    client2.ReadUntil("Access denied!");
    client2.Write("PRIVMSG *controlpanel :DelNetwork user2 test");
    client2.ReadUntil("Access denied!");

    // DenySetBindHost
    client2.Write("PRIVMSG *controlpanel :Set BindHost user2 127.0.0.1");
    client2.ReadUntil("BindHost = 127.0.0.1");
    client1.Write("PRIVMSG *controlpanel :Set DenySetBindHost user2 true");
    client1.ReadUntil("DenySetBindHost = true");
    client2.Write("PRIVMSG *controlpanel :Set BindHost user2 127.0.0.2");
    client2.ReadUntil("Access denied!");

    // DenySetIdent
    client2.Write("PRIVMSG *controlpanel :Set Ident user2 test");
    client2.ReadUntil("Ident = test");
    client1.Write("PRIVMSG *controlpanel :Set DenySetIdent user2 true");
    client1.ReadUntil("DenySetIdent = true");
    client2.Write("PRIVMSG *controlpanel :Set Ident user2 test2");
    client2.ReadUntil("Access denied!");

    // DenySetRealName
    client2.Write("PRIVMSG *controlpanel :Set RealName user2 test");
    client2.ReadUntil("RealName = test");
    client1.Write("PRIVMSG *controlpanel :Set DenySetRealName user2 true");
    client1.ReadUntil("DenySetRealName = true");
    client2.Write("PRIVMSG *controlpanel :Set RealName user2 test2");
    client2.ReadUntil("Access denied!");

    // DenySetQuitMsg
    client2.Write("PRIVMSG *controlpanel :Set QuitMsg user2 test");
    client2.ReadUntil("QuitMsg = test");
    client1.Write("PRIVMSG *controlpanel :Set DenySetQuitMsg user2 true");
    client1.ReadUntil("DenySetQuitMsg = true");
    client2.Write("PRIVMSG *controlpanel :Set QuitMsg user2 test2");
    client2.ReadUntil("Access denied!");

    // DenySetCTCPReplies
    client2.Write("PRIVMSG *controlpanel :AddCTCP user2 FOO BAR");
    client2.ReadUntil("CTCP requests FOO to user user2 will now get reply: BAR");
    client2.Write("PRIVMSG *controlpanel :DelCTCP user2 FOO");
    client2.ReadUntil("CTCP requests FOO to user user2 will now be sent to IRC clients");
    client1.Write("PRIVMSG *controlpanel :Set DenySetCTCPReplies user2 true");
    client1.ReadUntil("DenySetCTCPReplies = true");
    client2.Write("PRIVMSG *controlpanel :AddCTCP user2 FOO BAR");
    client2.ReadUntil("Access denied!");
    client2.Write("PRIVMSG *controlpanel :DelCTCP user2 FOO");
    client2.ReadUntil("Access denied!");
}

TEST_F(ZNCTest, CAP302MultiLS) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    InstallModule("testmod.cpp", R"(
        #include <znc/Client.h>
        #include <znc/Modules.h>
        class TestModule : public CModule {
          public:
            MODCONSTRUCTOR(TestModule) {}
            void OnClientCapLs(CClient* pClient, SCString& ssCaps) override {
                for (int i = 0; i < 100; ++i) {
                    ssCaps.insert("testcap-" + CString(i));
                }
            }
        };
        GLOBALMODULEDEFS(TestModule, "Test")
    )");
    client.Write("znc loadmod testmod");
    client.ReadUntil("Loaded module testmod");

    auto client2 = ConnectClient();
    client2.Write("CAP LS");
    client2.ReadUntil("LS :");
    auto rem = client2.ReadRemainder();
    ASSERT_GT(rem.indexOf("testcap-10"), 10);
    ASSERT_EQ(rem.indexOf("testcap-80"), -1);
    ASSERT_EQ(rem.indexOf("LS"), -1);

    client2 = ConnectClient();
    client2.Write("CAP LS 302");
    client2.ReadUntil("LS * :");
    rem = client2.ReadRemainder();
    int w = 0;
    ASSERT_GT(w = rem.indexOf("testcap-10"), 1);
    ASSERT_GT(w = rem.indexOf("testcap-22", w), 1);
    ASSERT_GT(w = rem.indexOf("LS * :", w), 1);
    ASSERT_GT(rem.indexOf("testcap-80", w), 1);
    ASSERT_GT(rem.indexOf("LS :", w), 1);
}

TEST_F(ZNCTest, CAP302LSValue) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    InstallModule("testmod.cpp", R"(
        #include <znc/Client.h>
        #include <znc/Modules.h>
        class TestModule : public CModule {
          public:
            MODCONSTRUCTOR(TestModule) {}
            void OnClientCapLs(CClient* pClient, SCString& ssCaps) override {
                if (pClient->HasCap302()) {
                    ssCaps.insert("testcap=blah");
                } else {
                    ssCaps.insert("testcap");
                }
            }
        };
        GLOBALMODULEDEFS(TestModule, "Test")
    )");
    client.Write("znc loadmod testmod");
    client.ReadUntil("Loaded module testmod");

    auto client2 = ConnectClient();
    client2.Write("CAP LS");
    client2.ReadUntil("testcap ");

    client2 = ConnectClient();
    client2.Write("CAP LS 302");
    client2.ReadUntil("testcap=");
}

TEST_F(ZNCTest, ServerDependentCapInModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    InstallModule("testmod.cpp", R"(
        #include <znc/Modules.h>
        #include <znc/Client.h>
        #include <znc/IRCNetwork.h>
        #include <znc/IRCSock.h>
        class TestModule : public CModule {
          public:
            MODCONSTRUCTOR(TestModule) {}
            void OnClientCapLs(CClient* pClient, SCString& ssCaps) override {
                if (GetNetwork() && GetNetwork()->IsServerCapAccepted("testcap")) {
                    if (pClient->HasCap302()) {
                        CString sValue = GetNetwork()->GetIRCSock()->GetCapLsValue("testcap");
                        if (!sValue.empty()) {
                            ssCaps.insert("testcap=" + sValue);
                        } else {
                            ssCaps.insert("testcap");
                        }
                    } else {
                        ssCaps.insert("testcap");
                    }
                }
            }
            bool IsClientCapSupported(CClient* pClient, const CString& sCap,
                                      bool bState) override {
                if (!bState) return false;
                if (sCap != "testcap") return false;
                return GetNetwork() && GetNetwork()->IsServerCapAccepted("testcap");
            }
            void OnClientCapRequest(CClient* pClient, const CString& sCap,
                                    bool bState) override {
                PutModule("OnClientCapRequest " + sCap + " " + CString(bState));
            }
            bool OnServerCap302Available(const CString& sCap, const CString& sValue) override {
                PutModule("OnServerCapAvailable " + sCap + " " + sValue);
                if (sCap == "testcap") {
                    if (GetNetwork()->IsServerCapAccepted("testcap")) {
                        // This can happen when server sent CAP NEW with another value.
                        for (CClient* pClient : GetNetwork()->GetClients()) {
                            pClient->NotifyServerDependentCap("testcap", true, sValue, nullptr);
                        }
                    }
                    return true;
                }
                return false;
            }
            void OnServerCapResult(const CString& sCap, bool bSuccess) override {
                if (sCap == "testcap") {
                    GetNetwork()->NotifyClientsAboutServerDependentCap("testcap", bSuccess, [=](CClient* pClient, bool bState) {
                        PutModule("OnServerCapResult " + sCap + " " + CString(bSuccess) + " " + CString(bState));
                    });
                }
            }
            void OnIRCConnected() override {
                if (GetNetwork()->IsServerCapAccepted("testcap")) {
                    GetNetwork()->NotifyClientsAboutServerDependentCap("testcap", true, [=](CClient* pClient, bool bState) {
                        PutModule("OnIRCConnected " + CString(bState));
                    });
                }
            }
            void OnIRCDisconnected() override {
                GetNetwork()->NotifyClientsAboutServerDependentCap("testcap", false, [=](CClient* pClient, bool bState) {
                    PutModule("OnIRCDisconnected " + CString(bState));
                });
            }
            ~TestModule() override {
                // TODO user module
                GetNetwork()->NotifyClientsAboutServerDependentCap("testcap", false, [=](CClient* pClient, bool bState) {
                    PutModule("~ " + CString(bState));
                });
            }
        };
        MODULEDEFS(TestModule, "Test")
    )");
    client.Write("znc loadmod testmod");
    client.ReadUntil("Loaded module testmod");
    client.Write("znc addnetwork net2");
    client.Close();

    client = ConnectClient();
    client.Write("CAP LS 302");
    client.Write("PASS :hunter2");
    client.Write("NICK nick");
    client.Write("USER user x x :x");
    client.Write("CAP END");
    client.ReadUntil("Welcome");

    ircd.Write("001 nick Welcome");
    ircd.Write("CAP nick NEW testcap=value");
    ircd.ReadUntil("CAP REQ :testcap");
    ircd.Write("CAP nick ACK :testcap");
    client.ReadUntil("CAP nick NEW :testcap=value");
    client.Write("CAP REQ testcap");
    client.ReadUntil("CAP nick ACK :testcap");

    client.Write("CAP LS");
    client.ReadUntil(" testcap=value ");

    ircd.Write("CAP nick DEL testcap");
    client.ReadUntil("CAP nick DEL :testcap");
    client.ReadUntil(":OnServerCapResult testcap false false");

    ircd.Close();
    ircd = ConnectIRCd();
    ircd.ReadUntil("CAP LS 302");
    ircd.Write("CAP nick LS :testcap=new");
    ircd.ReadUntil("CAP REQ :testcap");
    ircd.Write("CAP nick ACK :testcap");
    ircd.ReadUntil("CAP END");
    // TODO should NEW wait until 001?
    // TODO combine multiple NEWs to single line
    client.ReadUntil("CAP nick NEW :testcap=new");
    ircd.Write("001 nick Welcome");
    client.ReadUntil("Welcome");

    // NEW with new value without DEL
    ircd.Write("CAP nick NEW testcap=another");
    client.ReadUntil("CAP nick NEW :testcap=another");

    client.Write("znc jumpnetwork net2");
    client.ReadUntil("AAAAA");
}

TEST_F(ZNCTest, HashUpgrade) {
    QFile conf(m_dir.path() + "/configs/znc.conf");
    ASSERT_TRUE(conf.open(QIODevice::Append | QIODevice::Text));
    QTextStream out(&conf);
    out << R"(
        <User foo>
            <Pass pass>
                Method = MD5
                Salt = abc
                Hash = defdf93cef7fa7a8ee88e65d0e277b99
            </Pass>
        </User>
    )";
    out.flush();
    conf.close();
    auto znc = Run();
    auto ircd = ConnectIRCd();

    auto client = ConnectClient();
    client.Write("PASS :hunter2");
    client.Write("NICK nick");
    client.Write("USER foo x x :x");
    client.ReadUntil("Welcome");
    client.Close();

    client = LoginClient();
    client.Write("znc saveconfig");
    client.ReadUntil("Wrote config");

    ASSERT_TRUE(conf.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&conf);
    QString config = in.readAll();
    // It was upgraded to either Argon2 or SHA256
    EXPECT_THAT(config.toStdString(), Not(ContainsRegex("Method.*MD5")));

    // Check that still can login after the upgrade
    client = ConnectClient();
    client.Write("PASS :hunter2");
    client.Write("NICK nick");
    client.Write("USER foo x x :x");
    client.ReadUntil("Welcome");
    client.Close();
}

}  // namespace
}  // namespace znc_inttest
