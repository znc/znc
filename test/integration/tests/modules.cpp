/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
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

TEST_F(ZNCTest, NotifyConnectModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod notify_connect");
    client.ReadUntil("Loaded module");

    auto client2 = ConnectClient();
    client2.Write("PASS :hunter2");
    client2.Write("NICK nick");
    client2.Write("USER user/test x x :x");
    client.ReadUntil("NOTICE nick :*** user attached from 127.0.0.1");

    auto client3 = ConnectClient();
    client3.Write("PASS :hunter2");
    client3.Write("NICK nick");
    client3.Write("USER user@identifier/test x x :x");
    client.ReadUntil(
        "NOTICE nick :*** user@identifier attached from 127.0.0.1");
    client2.ReadUntil(
        "NOTICE nick :*** user@identifier attached from 127.0.0.1");

    client2.Write("QUIT");
    client.ReadUntil("NOTICE nick :*** user detached from 127.0.0.1");

    client3.Close();
    client.ReadUntil(
        "NOTICE nick :*** user@identifier detached from 127.0.0.1");
}

TEST_F(ZNCTest, ShellModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod shell");
    client.Write("PRIVMSG *shell :echo blahblah");
    client.ReadUntil("PRIVMSG nick :blahblah");
    client.ReadUntil("PRIVMSG nick :znc$");
}

TEST_F(ZNCTest, WatchModule) {
    // TODO test other messages
    // TODO test options
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod watch");
    client.Write("PRIVMSG *watch :add *");
    client.ReadUntil("Adding entry:");
    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN :#znc");
    ircd.Write(":n!i@h PRIVMSG #znc :\001ACTION foo\001");
    client.ReadUntil(
        ":$*!watch@znc.in PRIVMSG nick :* CTCP: n [ACTION foo] to [#znc]");
}

TEST_F(ZNCTest, ModuleCrypt) {
    QFile conf(m_dir.path() + "/configs/znc.conf");
    ASSERT_TRUE(conf.open(QIODevice::Append | QIODevice::Text));
    QTextStream(&conf) << "ServerThrottle = 1\n";
    auto znc = Run();

    auto ircd1 = ConnectIRCd();
    auto client1 = LoginClient();
    client1.Write("znc loadmod controlpanel");
    client1.Write("PRIVMSG *controlpanel :CloneUser user user2");
    client1.ReadUntil("User user2 added!");
    client1.Write("PRIVMSG *controlpanel :Set Nick user2 nick2");
    client1.Write("znc loadmod crypt");
    client1.ReadUntil("Loaded module");

    auto ircd2 = ConnectIRCd();
    auto client2 = ConnectClient();
    client2.Write("PASS user2:hunter2");
    client2.Write("NICK nick2");
    client2.Write("USER user2/test x x :x");
    client2.Write("znc loadmod crypt");
    client2.ReadUntil("Loaded module");

    client1.Write("PRIVMSG *crypt :keyx nick2");
    client1.ReadUntil("Sent my DH1080 public key to nick2");

    QByteArray pub1("");
    ircd1.ReadUntilAndGet("NOTICE nick2 :DH1080_INIT ", pub1);
    ircd2.Write(":user!user@user/test " + pub1);

    client2.ReadUntil("Received DH1080 public key from user");
    client2.ReadUntil("Key for user successfully set.");

    QByteArray pub2("");
    ircd2.ReadUntilAndGet("NOTICE user :DH1080_FINISH ", pub2);
    ircd1.Write(":nick2!user2@user2/test " + pub2);

    client1.ReadUntil("Key for nick2 successfully set.");

    client1.Write("PRIVMSG *crypt :listkeys");
    QByteArray key1("");
    client1.ReadUntilAndGet("| nick2  | ", key1);
    client2.Write("PRIVMSG *crypt :listkeys");
    QByteArray key2("");
    client2.ReadUntilAndGet("| user   | ", key2);
    ASSERT_EQ(key1.mid(11), key2.mid(11));
    client1.Write("CAP REQ :echo-message");
    client1.Write("PRIVMSG .nick2 :Hello");
    QByteArray secretmsg;
    ircd1.ReadUntilAndGet("PRIVMSG nick2 :+OK ", secretmsg);
    ircd2.Write(":user!user@user/test " + secretmsg);
    client2.ReadUntil("Hello");
    client1.ReadUntil(secretmsg);  // by echo-message
}

TEST_F(ZNCTest, AutoAttachModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    InstallModule("testmod.cpp", R"(
        #include <znc/Modules.h>
        #include <znc/Client.h>
        class TestModule : public CModule {
          public:
            MODCONSTRUCTOR(TestModule) {}
            EModRet OnChanBufferPlayMessage(CMessage& Message) override {
                PutIRC("TEST " + Message.GetClient()->GetNickMask());
                return CONTINUE;
            }
        };
        MODULEDEFS(TestModule, "Test")
    )");
    client.Write("znc loadmod testmod");
    client.Write("PRIVMSG *controlpanel :Set AutoClearChanBuffer $me no");
    client.Write("znc loadmod autoattach");
    client.Write("PRIVMSG *autoattach :Add * * *");
    client.ReadUntil("Added to list");
    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN :#znc");
    ircd.Write(":server 353 nick #znc :nick");
    ircd.Write(":server 366 nick #znc :End of /NAMES list");
    ircd.Write(":foo PRIVMSG #znc :hi");
    client.ReadUntil(":foo PRIVMSG");
    client.Write("detach #znc");
    client.ReadUntil("Detached");
    ircd.Write(":foo PRIVMSG #znc :hello");
    ircd.ReadUntil("TEST");
    client.ReadUntil("hello");
    client.Write("PRIVMSG *autoattach :Add * test *");
    client.ReadUntil("Added to list");
    client.Write("PRIVMSG *autoattach :Swap 1 2")
    client.ReadUntil("Rules Swapped");
}

TEST_F(ZNCTest, KeepNickModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod keepnick");
    client.ReadUntil("Loaded module");
    ircd.ReadUntil("NICK user");
    ircd.Write(":server 433 * nick :Nickname is already in use.");
    ircd.ReadUntil("NICK user_");
    ircd.Write(":server 001 user_ :Hello");
    client.ReadUntil("Connected!");
    ircd.ReadUntil("NICK user");
    ircd.Write(":server 435 user_ user #error :Nope :-P");
    client.ReadUntil(
        ":*keepnick!znc@znc.in PRIVMSG user_ "
        ":Unable to obtain nick user: Nope :-P, #error");
}

TEST_F(ZNCTest, ModuleCSRFOverride) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod samplewebapi");
    client.ReadUntil("Loaded module");
    auto request = QNetworkRequest(QUrl("http://127.0.0.1:12345/mods/global/samplewebapi/"));
    auto reply = HttpPost(request, {
        {"text", "ipsum"}
    })->readAll().toStdString();
    EXPECT_THAT(reply, HasSubstr("ipsum"));
}

}  // namespace
}  // namespace znc_inttest
