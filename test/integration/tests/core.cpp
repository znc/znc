/*
 * Copyright (C) 2004-2020 ZNC, see the NOTICE file for details.
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

#include "znctest.h"
#include <gmock/gmock.h>

using testing::HasSubstr;

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
    ASSERT_THAT(cap_ls.toStdString(), AllOf(HasSubstr("cap-notify"), Not(HasSubstr("away-notify"))));
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

}  // namespace
}  // namespace znc_inttest
