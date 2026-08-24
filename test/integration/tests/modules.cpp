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

#include "znctest.h"

#include <QSslSocket>
#include <QTcpServer>

using testing::HasSubstr;
using testing::Not;

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
    client.ReadUntil("NOTICE nick :*** user attached from ");

    auto client3 = ConnectClient();
    client3.Write("PASS :hunter2");
    client3.Write("NICK nick");
    client3.Write("USER user@identifier/test x x :x");
    client.ReadUntil(
        "NOTICE nick :*** user@identifier attached from ");
    client2.ReadUntil(
        "NOTICE nick :*** user@identifier attached from ");

    client2.Write("QUIT");
    client.ReadUntil("NOTICE nick :*** user detached from ");

    client3.Close();
    client.ReadUntil(
        "NOTICE nick :*** user@identifier detached from ");
}

TEST_F(ZNCTest, ClientNotifyModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod clientnotify");
    client.ReadUntil("Loaded module");

    auto check_not_sent = [](Socket& client, QString wrongAnswer) {
        QString result = QString::fromUtf8(client.ReadRemainder());
        QRegularExpression expr(wrongAnswer);
        QRegularExpressionMatch match = expr.match(result);
        EXPECT_FALSE(match.hasMatch())
            << "Got an answer from the ClientNotifyModule even though we didnt "
               "want one with the given configuration: "
            << wrongAnswer.toStdString() << result.toStdString();
    };

    auto client2 = LoginClient();
    client.ReadUntilRe(R"(:Another client \((localhost)?\) authenticated as your user. Use the 'ListClients' command to see all 2 clients.)");
    auto client3 = LoginClient();
    client.ReadUntilRe(R"(:Another client \((localhost)?\) authenticated as your user. Use the 'ListClients' command to see all 3 clients.)");

    // disable notifications for every message
    client.Write("PRIVMSG *clientnotify :NewOnly on");

    // check that we do not ge a notification after connecting from a know ip
    auto client4 = LoginClient();
    check_not_sent(client, ":Another client (.*) authenticated as your user. Use the 'ListClients' command to see all 4 clients.");

    // choose to notify only on new client ids
    client.Write("PRIVMSG *clientnotify :NotifyOnNewID on");

    auto client5 = LoginClient("identifier123");
    client.ReadUntilRe(R"(:Another client \((localhost)? / identifier123\) authenticated as your user. Use the 'ListClients' command to see all 5 clients.)");
    auto client6 = LoginClient("identifier123");
    check_not_sent(client, ":Another client (.* / identifier123) authenticated as your user. Use the 'ListClients' command to see all 6 clients.");

    auto client7 = LoginClient("not_identifier123");
    client.ReadUntilRe(R"(:Another client \((localhost)? / not_identifier123\) authenticated as your user. Use the 'ListClients' command to see all 7 clients.)");

    // choose to notify from both clientids and new IPs
    client.Write("PRIVMSG *clientnotify :NotifyOnNewIP on");

    auto client8 = LoginClient();
    check_not_sent(client, ":Another client (.* / identifier123) authenticated as your user. Use the 'ListClients' command to see all 8 clients.");
    auto client9 = LoginClient("definitely_not_identifier123");
    client.ReadUntilRe(R"(:Another client \((localhost)? / definitely_not_identifier123\) authenticated as your user. Use the 'ListClients' command to see all 9 clients.)");
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
    // TODO test options
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod watch");
    client.ReadUntil("Loaded module");

    client.Write("PRIVMSG *watch :add *");
    client.ReadUntil("Adding entry:");

    client.Write("PRIVMSG *watch :add * *spaces *word1 word2*");
    client.ReadUntil("Adding entry:");

    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN :#znc");

    // OnChanCTCPMessage / OnChanActionMessage
    ircd.Write(":n!i@h PRIVMSG #znc :\001ACTION foo\001");
    client.ReadUntil(
        ":$*!watch@znc.in PRIVMSG nick :* CTCP: n [ACTION foo] to [#znc]");

    // OnChanNoticeMessage
    ircd.Write(":n!i@h NOTICE #znc :Channel notice");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :-n:#znc- Channel notice");

    // OnChanTextMessage
    ircd.Write(":n!i@h PRIVMSG #znc :SOMETHING word1 word2 SOMETHING");
    client.ReadUntil(
        ":*spaces!watch@znc.in PRIVMSG nick :<n:#znc> SOMETHING word1 word2 "
        "SOMETHING");

    // OnPrivCTCPMessage / OnPrivActionMessage
    ircd.Write(":n!i@h PRIVMSG nick :\001ACTION foo\001");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* CTCP: n [ACTION foo]");

    // OnPrivNoticeMessage
    ircd.Write(":n!i@h NOTICE nick :Private notice");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :-n- Private notice");

    // OnPrivTextMessage
    ircd.Write(":n!i@h PRIVMSG nick :Hello there");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :<n> Hello there");

    // OnJoinMessage
    ircd.Write(":join!i@h JOIN :#znc");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* join (i@h) joins #znc");

    // OnKickMessage
    ircd.Write(":kick!i@h JOIN :#znc");
    ircd.Write(":n!i@h KICK #znc kick :Test Kick");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* n kicked kick from #znc because "
        "[Test Kick]");

    // OnNickMessage
    ircd.Write(":someone!i@h JOIN :#znc");
    ircd.Write(":someone!i@h NICK :newname");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* someone is now known as newname");

    // OnPartMessage
    ircd.Write(":part!i@h JOIN :#znc");
    ircd.Write(":part!i@h PART #znc :Leaving");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* part (i@h) parts #znc(Leaving)");

    // OnQuitMessage
    ircd.Write(":quit!i@h JOIN :#znc");
    ircd.Write(":quit!i@h QUIT :Goodbye");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* Quits: quit (i@h) (Goodbye)");

    // OnRawMode
    ircd.Write(":n!i@h MODE #znc +o op");
    client.ReadUntil(":$*!watch@znc.in PRIVMSG nick :* n sets mode: +o op on #znc");
}

TEST_F(ZNCTest, CryptModule) {
#ifndef HAVE_LIBSSL
    GTEST_SKIP() << "SSL is disabled";
#endif

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
    client1.ReadUntilAndGet("\002nick2\017: ", key1);
    client2.Write("PRIVMSG *crypt :listkeys");
    QByteArray key2("");
    client2.ReadUntilAndGet("\002user\017: ", key2);
    ASSERT_EQ(key1.mid(9), key2.mid(8));

    // OnPrivTextMessage
    client1.Write("CAP REQ :echo-message");
    client1.Write("PRIVMSG .nick2 :Hello");
    QByteArray secretmsg;
    ircd1.ReadUntilAndGet("PRIVMSG nick2 :+OK ", secretmsg);
    ircd2.Write(":user!user@user/test " + secretmsg);
    client2.ReadUntil("Hello");
    client1.ReadUntil(secretmsg);  // by echo-message

    client1.Write("PRIVMSG *crypt :SetNickPrefix .");
    client1.ReadUntil("Setting Nick Prefix to .");
    client2.Write("PRIVMSG *crypt :SetNickPrefix .");
    client2.ReadUntil("Setting Nick Prefix to .");

    // OnPrivNoticeMessage
    client1.Write("NOTICE .nick2 :secret notice");
    QByteArray noticeMsg;
    ircd1.ReadUntilAndGet("NOTICE nick2 :+OK ", noticeMsg);
    ircd2.Write(":user!user@user/test " + noticeMsg);
    QByteArray noticeLine;
    client2.ReadUntilAndGet("NOTICE nick2 :", noticeLine);
    QByteArray noticemessage = noticeLine.mid(noticeLine.lastIndexOf(':') + 1);
    EXPECT_EQ(noticemessage, "secret notice");

    // OnUserActionMessage
    client1.Write("PRIVMSG .nick2 :\001ACTION waves\001");
    QByteArray actionMsg;
    ircd1.ReadUntilAndGet("PRIVMSG nick2 :\001ACTION +OK ", actionMsg);
    ircd2.Write(":user!user@user/test " + actionMsg);
    QByteArray actionLine;
    client2.ReadUntilAndGet("PRIVMSG nick2 :", actionLine);
    QByteArray actionMessage = actionLine.mid(actionLine.lastIndexOf(':') + 1);
    EXPECT_EQ(actionMessage, "\001ACTION waves\001");

    client1.Write("JOIN #test");
    client2.Write("JOIN #test");

    QByteArray chanKey = "channelKey123";
    client1.Write(QByteArray("PRIVMSG *crypt :SetKey #test ") + chanKey);
    client1.ReadUntil(QByteArray("Set encryption key for [#test] to [") +
                      chanKey + "]");
    client2.Write(QByteArray("PRIVMSG *crypt :SetKey #test ") + chanKey);
    client2.ReadUntil(QByteArray("Set encryption key for [#test] to [") +
                      chanKey + "]");

    // OnChanTextMessage
    client1.Write("PRIVMSG #test :channel secret");
    QByteArray chanMsg;
    ircd1.ReadUntilAndGet("PRIVMSG #test :+OK ", chanMsg);
    ircd2.Write(":user!user@user/test " + chanMsg);
    client2.ReadUntil("channel secret");

    // OnChanNoticeMessage
    client1.Write("NOTICE #test :chan notice");
    QByteArray chanNoticeMsg;
    ircd1.ReadUntilAndGet("NOTICE #test :+OK ", chanNoticeMsg);
    ircd2.Write(":user!user@user/test " + chanNoticeMsg);
    client2.ReadUntil("chan notice");

    // OnChanActionMessage
    client1.Write("PRIVMSG #test :\001ACTION dances\001");
    QByteArray chanActionMsg;
    ircd1.ReadUntilAndGet("PRIVMSG #test :\001ACTION +OK ", chanActionMsg);
    ircd2.Write(":user!user@user/test " + chanActionMsg);
    client2.ReadUntil("dances");

    // OnTopicMessage / OnNumericMessage
    client1.Write("TOPIC #test :new chan topic");
    QByteArray chanTopicEncrypted;
    ircd1.ReadUntilAndGet("TOPIC #test :+OK ", chanTopicEncrypted);
    chanTopicEncrypted = "+OK " + chanTopicEncrypted.mid(
                                      chanTopicEncrypted.lastIndexOf(' ') + 1);
    ircd2.Write(":ircd2 332 nick2 #test :" + chanTopicEncrypted);
    client2.ReadUntil("new chan topic");
}

TEST_F(ZNCTest, AutoAttachModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    InstallModule("testmod.cpp", R"(
        #include <znc/Client.h>
        #include <znc/Modules.h>
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

    // OnChanTextMessage
    ircd.Write(":foo PRIVMSG #znc :hi");
    client.ReadUntil(":foo PRIVMSG");
    client.Write("detach #znc");
    client.ReadUntil("Detached");
    ircd.Write(":foo PRIVMSG #znc :hello");
    ircd.ReadUntil("TEST");
    client.ReadUntil("hello");

    // OnChanActionMessage
    ircd.Write(":foo PRIVMSG #znc :hi");
    client.ReadUntil(":foo PRIVMSG");
    client.Write("detach #znc");
    client.ReadUntil("Detached");
    ircd.Write(":foo PRIVMSG #znc :\001ACTION hello\001");
    ircd.ReadUntil("TEST");
    client.ReadUntil("\001ACTION hello\001");

    // OnChanNoticeMessage
    ircd.Write(":foo NOTICE #znc :hi");
    client.ReadUntil(":foo NOTICE");
    client.Write("detach #znc");
    client.ReadUntil("Detached");
    ircd.Write(":foo NOTICE #znc :hello NOTICE");
    ircd.ReadUntil("TEST");
    client.ReadUntil("hello NOTICE");
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
        ":*keepnick!keepnick@znc.in PRIVMSG user_ "
        ":Unable to obtain nick user: Nope :-P, #error");

    client.Write("PRIVMSG *keepnick :state");
    client.ReadUntil("Currently disabled");

    client.Write("NICK user_");
    ircd.ReadUntil("NICK user_");
    client.Write("JOIN #test");
    ircd.ReadUntil("JOIN #test");
    ircd.Write(":server 353 nick = #test :user_ user");
    ircd.Write(":server 366 nick #test :End of /NAMES list");

    client.Write("PRIVMSG *keepnick :enable");
    client.ReadUntil("Trying to get");

    // OnQuitMessage
    ircd.Write(":user QUIT :Leaving");
    ircd.ReadUntil("NICK user");

    // OnNickMessage
    client.Write("NICK test");
    ircd.ReadUntil("NICK test");
    ircd.Write(":user!ident@host JOIN #test");
    client.ReadUntil("JOIN");
    ircd.Write(":user!ident@host NICK user2");
    ircd.ReadUntil("NICK user");
}

TEST_F(ZNCTest, ModuleCSRFOverride) {
    // TODO: Qt 6.8 introduced QNetworkRequest::FullLocalServerNameAttribute to
    // let it connect to unix socket
    int port = PickPortNumber();
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write(QStringLiteral("znc addport %1 all all").arg(port).toUtf8());
    client.Write("znc loadmod samplewebapi");
    client.ReadUntil("Loaded module");
    auto request = QNetworkRequest(
        QUrl(QStringLiteral("http://127.0.0.1:%1/mods/global/samplewebapi/")
                 .arg(port)));
    auto reply =
        HttpPost(request, {{"text", "ipsum"}})->readAll().toStdString();
    EXPECT_THAT(reply, HasSubstr("ipsum"));
}

class SaslModuleTest : public ZNCTest,
                       public testing::WithParamInterface<
                           std::pair<int, std::vector<std::string>>> {
  public:
    static std::string Prefix() {
        std::string s;
        for (int i = 0; i < 33; ++i) s += "YWFh";
        s += "YQBh";
        for (int i = 0; i < 33; ++i) s += "YWFh";
        s += "AGJi";
        for (int i = 0; i < 31; ++i) s += "YmJi";
        EXPECT_EQ(s.length(), 396);
        return s;
    }

  protected:
    int PassLen() { return std::get<0>(GetParam()); }
    void ExpectPlainAuth(Socket& ircd) {
        for (const auto& str : std::get<1>(GetParam())) {
            QByteArray line;
            ircd.ReadUntilAndGet("AUTHENTICATE ", line);
            ASSERT_EQ(line.toStdString(), "AUTHENTICATE " + str);
        }
        ASSERT_EQ(ircd.ReadRemainder().indexOf("AUTHENTICATE"), -1);
    }
};

TEST_P(SaslModuleTest, Test) {
    QFile conf(m_dir.path() + "/configs/znc.conf");
    ASSERT_TRUE(conf.open(QIODevice::Append | QIODevice::Text));
    QTextStream(&conf) << "ServerThrottle = 1\n";
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod sasl");
    QByteArray sUser(100, 'a');
    QByteArray sPass(PassLen(), 'b');
    client.Write("PRIVMSG *sasl :set " + sUser + " " + sPass);
    client.Write("znc jump");
    ircd = ConnectIRCd();
    ircd.ReadUntil("CAP LS");
    ircd.Write("CAP * LS :away-notify sasl");
    ircd.ReadUntil("CAP REQ :away-notify sasl");
    ircd.Write("CAP * ACK :away-notify sasl");
    ircd.ReadUntil("AUTHENTICATE EXTERNAL");
    ircd.Write(":server 904 *");
    ircd.ReadUntil("AUTHENTICATE PLAIN");
    ircd.Write("AUTHENTICATE +");
    ExpectPlainAuth(ircd);
    ircd.Write(":server 903 user :Logged in");
    ircd.ReadUntil("CAP END");
}

INSTANTIATE_TEST_CASE_P(SaslInst, SaslModuleTest,
                        testing::Values(
                            std::pair<int, std::vector<std::string>>{
                                95, {SaslModuleTest::Prefix()}},
                            std::pair<int, std::vector<std::string>>{
                                96, {SaslModuleTest::Prefix() + "Yg==", "+"}},
                            std::pair<int, std::vector<std::string>>{
                                97, {SaslModuleTest::Prefix() + "YmI=", "+"}},
                            std::pair<int, std::vector<std::string>>{
                                98, {SaslModuleTest::Prefix() + "YmJi", "+"}},
                            std::pair<int, std::vector<std::string>>{
                                99,
                                {SaslModuleTest::Prefix() + "YmJi", "Yg=="}}));

TEST_F(ZNCTest, SaslMechsNotInit) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod sasl");
    client.Write("PRIVMSG *sasl :set * *");
    client.ReadUntil("Password has been set");
    ircd.Write("AUTHENTICATE +");
    ircd.Write("PING foo");
    ircd.ReadUntil("PONG foo");
}

TEST_F(ZNCTest, SaslRequire) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod sasl");
    client.Write("PRIVMSG *sasl :set * *");
    client.Write("PRIVMSG *sasl :requireauth yes");
    client.ReadUntil("Password has been set");
    client.Write("znc jump");
    ircd = ConnectIRCd();
    ircd.ReadUntil("CAP LS");
    ircd.Write(":server 001 nick :Hello");
    ircd.ReadUntil("QUIT :SASL not available");
    auto ircd2 = ConnectIRCd();
}

TEST_F(ZNCTest, SaslAuthPlainImapAuth) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    QTcpServer imap;
    ASSERT_TRUE(imap.listen(QHostAddress::LocalHost)) << imap.errorString().toStdString();
    auto client = LoginClient();
    client.Write(
        QStringLiteral("znc loadmod imapauth 127.0.0.1 %1 %@mail.test.com")
            .arg(imap.serverPort())
            .toUtf8());
    client.ReadUntil("Loaded");

    auto client2 = ConnectClient();
    client2.Write("NICK foo");
    client2.Write("CAP REQ :sasl");
    client2.Write("USER bar");
    client2.Write("AUTHENTICATE PLAIN");
    client2.Write("AUTHENTICATE " + QByteArrayLiteral("\0user@phone/net\0hunter3").toBase64());
    client2.ReadUntil("ACK :sasl");

    ASSERT_TRUE(imap.waitForNewConnection(30000 /* msec */));
    auto imapsock = WrapIO(imap.nextPendingConnection());
    imapsock.Write("* OK IMAP4rev1 Service Ready");
    imapsock.ReadUntil("AUTH LOGIN user@mail.test.com hunter3");
    imapsock.Write("AUTH OK");

    client2.ReadUntil(":irc.znc.in 903 foo :SASL authentication successful");
}

TEST_F(ZNCTest, SaslAuthExternal) {
#ifndef HAVE_LIBSSL
    GTEST_SKIP() << "SSL is disabled";
#endif

    int port = PickPortNumber();

    auto znc = Run();
    auto ircd = ConnectIRCd();
    ircd.Write(":server 001 nick :Hello");
    auto client = LoginClient();
    client.Write(QStringLiteral("znc addport +%1 all all").arg(port).toUtf8());
    client.ReadUntil(":Port added");
    client.Write("znc loadmod certauth");
    client.ReadUntil("Loaded");
    client.Close();

    QSslSocket sock;
    // Could generate a new one for the test, but this one is good enough
    sock.setLocalCertificate(m_dir.path() + "/znc.pem");
    sock.setPrivateKey(m_dir.path() + "/znc.pem");
    sock.setPeerVerifyMode(QSslSocket::VerifyNone);
    sock.connectToHostEncrypted("127.0.0.1", port);
    ASSERT_TRUE(sock.waitForConnected()) << sock.errorString().toStdString();
    ASSERT_TRUE(sock.waitForEncrypted()) << sock.errorString().toStdString();
    auto client2 = WrapIO(&sock);
    client2.Write("PASS :hunter2");
    client2.Write("NICK nick");
    client2.Write("USER user/test x x :x");
    client2.Write("privmsg *certauth add");
    client2.ReadUntil("added");

    auto Reconnect = [&] {
        client2.Close();
        ASSERT_TRUE(sock.state() == QAbstractSocket::UnconnectedState || sock.waitForDisconnected())
            << sock.errorString().toStdString();
        sock.connectToHostEncrypted("127.0.0.1", port);
        ASSERT_TRUE(sock.waitForConnected())
            << sock.errorString().toStdString();
        ASSERT_TRUE(sock.waitForEncrypted())
            << sock.errorString().toStdString();
        client2.Write("CAP REQ sasl");
        client2.Write("NICK nick");
        client2.Write("USER u x x :x");
        client2.ReadUntil("ACK :sasl");
        client2.Write("AUTHENTICATE EXTERNAL");
        client2.ReadUntil("AUTHENTICATE +");
    };

    Reconnect();
    ircd.Write(":friend PRIVMSG nick :hello");
    client2.Write("AUTHENTICATE +");
    client2.ReadUntil(
        ":irc.znc.in 900 nick nick!user@127.0.0.1 user :You are now logged in "
        "as user");
    client2.ReadUntil(":irc.znc.in 903 nick :SASL authentication successful");
    client2.Write("CAP END");
    // '[' comes from lack of server-time
    client2.ReadUntil(":friend PRIVMSG nick :[");

    Reconnect();
    client2.Write("AUTHENTICATE " + QByteArrayLiteral("user/te").toBase64());
    client2.ReadUntil(
        ":irc.znc.in 900 nick nick!user@127.0.0.1 user :You are now logged in "
        "as user");
    client2.ReadUntil(":irc.znc.in 903 nick :SASL authentication successful");
    client2.Write("CAP END");
    client2.ReadUntil(
        ":*status!status@znc.in PRIVMSG nick :Network te doesn't exist.");

    Reconnect();
    client2.Write("AUTHENTICATE " + QByteArrayLiteral("moo").toBase64());
    client2.ReadUntil(
        ":irc.znc.in 904 nick :The specified user doesn't have this key");

    client = LoginClient();
    client.Write("privmsg *certauth :del 1");
    client.ReadUntil("Removed");
    Reconnect();
    client2.Write("AUTHENTICATE +");
    client2.ReadUntil(
        ":irc.znc.in 904 nick :Client cert not recognized");

    // Wrong mechanism
    auto client3 = ConnectClient();
    client3.Write("CAP LS 302");
    client3.Write("NICK nick");
    client3.ReadUntil(" sasl=EXTERNAL,PLAIN ");
    client3.Write("CAP REQ :sasl");
    client3.ReadUntil("ACK :sasl");
    client3.Write("AUTHENTICATE FOO");
    client3.ReadUntil(":irc.znc.in 908 nick EXTERNAL,PLAIN :are available SASL mechanisms");
    client3.ReadUntil(
        ":irc.znc.in 904 nick :SASL authentication failed");
}

TEST_F(ZNCTest, StripControlsModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod stripcontrols");
    client.ReadUntil("Loaded module");

    ircd.Write(":server 001 nick :Hello");
    client.Write(":nick JOIN #test");
    ircd.ReadUntil("JOIN #test");

    // OnChanCTCPMessage
    ircd.Write(":user!id@host PRIVMSG #test :\001\002bold\002 \003\034red\003 test\001");
    client.ReadUntil(":user!id@host PRIVMSG #test :\001bold red test\001");

    // OnChanNoticeMessage
    ircd.Write(":user!id@host NOTICE #test :\002bold\002 \003\034red\003 test");
    client.ReadUntil(":user!id@host NOTICE #test :bold red test");

    // OnChanTextMessage
    ircd.Write(":user!id@host PRIVMSG #test :\002bold\002 \003\034red\003 test");
    client.ReadUntil(":user!id@host PRIVMSG #test :bold red test");

    // OnPrivCTCPMessage
    ircd.Write(":user!id@host PRIVMSG nick :\001\002bold\002 \003\034red\003 test\001");
    client.ReadUntil(":user!id@host PRIVMSG nick :\001bold red test\001");

    // OnPrivNoticeMessage
    ircd.Write(":user!id@host NOTICE nick :\002bold\002 \003\034red\003 test");
    client.ReadUntil(":user!id@host NOTICE nick :bold red test");

    // OnPrivTextMessage
    ircd.Write(":user!id@host PRIVMSG nick :\002bold\002 \003\034red\003 test");
    client.ReadUntil(":user!id@host PRIVMSG nick :bold red test");

    // OnTopicMessage
    ircd.Write(":user!id@host TOPIC #test :\002bold\002 \003\034red\003 test");
    client.ReadUntil(":user!id@host TOPIC #test :bold red test");

    // OnNumericMessage
    // Topic from joining channel.
    ircd.Write("332 nick #test :\002bold\002 \003\034red\003 test");
    client.ReadUntil("332 nick #test :bold red test");

    // Topic from /list
    //ircd.Write("321 nick Channel :Users  Name]");
    ircd.Write("322 nick #test 42 :\002bold\002 \003\034red\003 test");
    client.ReadUntil("322 nick #test 42 :bold red test");

}

TEST_F(ZNCTest, AutoReplyModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod autoreply I'm currently away");
    client.ReadUntil("Loaded module");

    // Set for 1 second instead of waiting 15 seconds.
    client.Write("PRIVMSG *simple_away :SetTimer 1");
    client.ReadUntil("1 second");

    ircd.Write(":server 001 nick :Hello");

    client.Write("QUIT :Going away");
    client.Close();

    ircd.ReadUntil("AWAY :Auto away");
    ircd.Write(":testuser!test@host PRIVMSG nick :Hello there");
    ircd.ReadUntil("NOTICE testuser :I'm currently away");

    ircd.Write(":testuser!test@host PRIVMSG nick :Another message");
    ircd.Write(":otheruser!other@host PRIVMSG nick :Hi nick");
    ircd.ReadUntil("NOTICE otheruser :I'm currently away");

    auto client2 = LoginClient();
    ircd.Write(":thirduser!third@host PRIVMSG nick :Are you there?");
}

TEST_F(ZNCTest, BounceDCCModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod bouncedcc");
    client.ReadUntil("Loaded module");

    ircd.Write(":server 001 nick :Hello");

    // OnUserCTCPMessage
    client.Write("PRIVMSG friend :\001DCC CHAT chat 3232235521 12345\001");
    QByteArray line;
    ircd.ReadUntilAndGet("PRIVMSG friend :\001DCC CHAT chat", line);
    EXPECT_THAT(line.toStdString(), Not(HasSubstr("3232235521 12345")));

    // OnPrivCTCPMessage
    ircd.Write(":friend!user@host PRIVMSG nick :\001DCC CHAT chat 3232235521 54321\001");
    QByteArray line2;
    client.ReadUntilAndGet(":friend!user@host PRIVMSG nick :\001DCC CHAT chat", line2);
    EXPECT_THAT(line2.toStdString(), Not(HasSubstr("3232235521 54321")));
}

TEST_F(ZNCTest, ChanSaverModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("PRIVMSG *controlpanel :GetChan InConfig $me $network #test");
    client.ReadUntil("No channels matching [#test] found");

    client.Write("JOIN #test");
    ircd.Write(":server 001 nick :Hello");
    ircd.ReadUntil("JOIN #test");
    ircd.Write(":nick JOIN :#test");

    client.ReadUntil(":nick JOIN :#test");
    client.Write("PRIVMSG *controlpanel :GetChan InConfig $me $network #test");
    client.ReadUntil("InConfig = true");

    client.Write("PART #test");
    ircd.ReadUntil("PART #test");
    ircd.Write(":nick PART #test");

    client.Write("PRIVMSG *controlpanel :GetChan InConfig $me $network #test");
    client.ReadUntil("No channels matching [#test] found");
}

TEST_F(ZNCTest, ClearBufferOnMsgModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod clearbufferonmsg");
    client.ReadUntil("Loaded module");

    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN :#test");
    ircd.Write(":server 353 nick = #test :nick someone");
    ircd.Write(":server 366 nick #test :End of /NAMES list");
    client.ReadUntil("End of /NAMES");

    // OnUserTextMessage
    ircd.Write(":someone!user@host PRIVMSG #test :testing - OnUserTextMessage");
    client.ReadUntil("testing - OnUserTextMessage");
    client.Write("PRIVMSG #test :test");
    ircd.ReadUntil("PRIVMSG");
    client.Write("DETACH #test");
    client.ReadUntil("Detached");
    client.Write("ATTACH #test");
    client.ReadUntil("End of /NAMES list");
    QByteArray remainder = client.ReadRemainder();
    EXPECT_THAT(remainder, Not(HasSubstr("OnUserTextMessage")))
        << "OnUserTextMessage failed to clear buffer";

    // OnUserActionMessage
    ircd.Write(":someone!user@host PRIVMSG #test :testing - OnUserActionMessage");
    client.ReadUntil("testing - OnUserActionMessage");
    client.Write("PRIVMSG #test :\001ACTION testing\001");
    ircd.ReadUntil("PRIVMSG");
    client.Write("DETACH #test");
    client.ReadUntil("Detached");
    client.Write("ATTACH #test");
    client.ReadUntil("End of /NAMES list");
    remainder = client.ReadRemainder();
    EXPECT_THAT(remainder, Not(HasSubstr("OnUserActionMessage")))
        << "OnUserActionMessage failed to clear buffer";

    // OnUserCTCPMessage
    ircd.Write(":someone!user@host PRIVMSG #test :testing - OnUserCTCPMessage");
    client.ReadUntil("testing - OnUserCTCPMessage");
    client.Write("PRIVMSG #test :\001VERSION\001");
    ircd.ReadUntil("PRIVMSG");
    client.Write("DETACH #test");
    client.ReadUntil("Detached");
    client.Write("ATTACH #test");
    client.ReadUntil("End of /NAMES list");
    remainder = client.ReadRemainder();
    EXPECT_THAT(remainder, Not(HasSubstr("OnUserCTCPMessage")))
        << "OnUserCTCPMessage failed to clear buffer";

    // OnUserNoticeMessage
    ircd.Write(":someone!user@host PRIVMSG #test :testing - OnUserNoticeMessage");
    client.ReadUntil("testing - OnUserNoticeMessage");
    client.Write("NOTICE #test :testing");
    ircd.ReadUntil("NOTICE");
    client.Write("DETACH #test");
    client.ReadUntil("Detached");
    client.Write("ATTACH #test");
    client.ReadUntil("End of /NAMES list");
    remainder = client.ReadRemainder();
    EXPECT_THAT(remainder, Not(HasSubstr("OnUserNoticeMessage")))
        << "OnUserNoticeMessage failed to clear buffer";

    // OnUserPartMessage
    ircd.Write(":someone!user@host PRIVMSG #test :testing - OnUserPartMessage");
    client.ReadUntil("testing - OnUserPartMessage");
    client.Write("PART #test :testing");
    ircd.ReadUntil("PART");
    client.Write("JOIN #test");
    ircd.ReadUntil("JOIN");
    ircd.Write(":server 353 nick = #test :nick someone");
    ircd.Write(":server 366 nick #test :End of /NAMES list");
    client.ReadUntil("End of /NAMES");
    client.Write("DETACH #test");
    client.ReadUntil("Detached");
    client.Write("ATTACH #test");
    client.ReadUntil("End of /NAMES list");
    remainder = client.ReadRemainder();
    EXPECT_THAT(remainder, Not(HasSubstr("OnUserPartMessage")))
        << "OnUserPartMessage failed to clear buffer";

    // OnUserTopicMessage
    ircd.Write(":someone!user@host PRIVMSG #test :testing - OnUserTopicMessage");
    client.ReadUntil("testing - OnUserTopicMessage");
    client.Write("TOPIC #test :testing");
    ircd.ReadUntil("TOPIC");
    client.Write("DETACH #test");
    client.ReadUntil("Detached");
    client.Write("ATTACH #test");
    client.ReadUntil("End of /NAMES list");
    remainder = client.ReadRemainder();
    EXPECT_THAT(remainder, Not(HasSubstr("OnUserTopicMessage")))
        << "OnUserTopicMessage failed to clear buffer";
}

TEST_F(ZNCTest, CtcpFloodModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod ctcpflood");
    client.ReadUntil("Loaded module");

    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN :#test");
    ircd.Write(":server 353 nick = #test :nick someone");
    ircd.Write(":server 366 nick #test :End of /NAMES list");

    ircd.Write(":someone!user@host PRIVMSG #test :\001message1\001");
    ircd.Write(":someone!user@host PRIVMSG #test :\001message2\001");
    ircd.Write(":someone!user@host PRIVMSG #test :\001message3\001");
    ircd.Write(":someone!user@host PRIVMSG #test :\001message4\001");
    ircd.Write(":someone!user@host PRIVMSG #test :\001message5\001");
    ircd.Write(":someone!user@host PRIVMSG #test :\001message6\001");
    ircd.Write(":someone!user@host PRIVMSG #test :\001message7\001");
    client.ReadUntil("Limit reached by");
}

TEST_F(ZNCTest, FloodDetachModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod flooddetach");
    client.ReadUntil("Loaded module");
    ircd.Write(":server 001 nick :Hello");

    auto JoinChan = [&](const QString& sChan) {
        ircd.Write((":nick JOIN :" + sChan).toUtf8());
        ircd.Write((":server 353 nick = " + sChan + " :nick someone").toUtf8());
        ircd.Write(
            (":server 366 nick " + sChan + " :End of /NAMES list").toUtf8());
        client.ReadUntil("End of /NAMES");
    };
    // Just use 10 for everything.

    // OnChanMessage
    JoinChan("#test-privmsg");
    for (int i = 1; i <= 10; i++) {
        ircd.Write((":someone!user@host PRIVMSG #test-privmsg :message" +
                    QString::number(i))
                       .toUtf8());
    }
    client.ReadUntil("Channel #test-privmsg was flooded");

    // OnNickMessage
    JoinChan("#test-nick");
    QString sNick = "someone";
    for (int i = 1; i <= 10; i++) {
        QString sNewNick = "someone" + QString::number(i + 1);
        ircd.Write((":" + sNick + "!user@host NICK " + sNewNick).toUtf8());
        sNick = sNewNick;
    }
    client.ReadUntil("Channel #test-nick was flooded");

    // OnTopicMessage
    JoinChan("#test-topic");
    for (int i = 1; i <= 10; i++) {
        ircd.Write(
            (":someone!user@host TOPIC #test-topic :TOPIC" + QString::number(i))
                .toUtf8());
    }
    client.ReadUntil("Channel #test-topic was flooded");

    // OnNoticeMessage
    JoinChan("#test-notice");
    for (int i = 1; i <= 10; i++) {
        ircd.Write((":someone!user@host NOTICE #test-notice :notice" +
                    QString::number(i))
                       .toUtf8());
    }
    client.ReadUntil("Channel #test-notice was flooded");

    // OnChanActionMessage / OnChanCTCPMessage
    JoinChan("#test-ctcp");
    for (int i = 1; i <= 10; i++) {
        ircd.Write((":someone!user@host PRIVMSG #test-ctcp :\001"
                    "ACTION action" +
                    QString::number(i) + "\001")
                       .toUtf8());
    }
    client.ReadUntil("Channel #test-ctcp was flooded");
}

TEST_F(ZNCTest, KickRejoinModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod kickrejoin");
    client.ReadUntil("Loaded module");

    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":nick JOIN :#test");
    ircd.Write(":server 353 nick = #test :nick @foobar");
    ircd.Write(":server 366 nick #test :End of /NAMES list");

    ircd.Write(":foobar!user@host KICK #test nick :Kicked!");
    ircd.ReadUntil("JOIN #test");
}

TEST_F(ZNCTest, NickServModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod nickserv");
    client.ReadUntil("Loaded module");

    client.Write("PRIVMSG *nickserv :set hunter2");
    client.ReadUntil("Password set");

    ircd.Write(":server 001 nick :Hello");

    // OnPrivNoticeMessage
    ircd.Write(
        ":NickServ!services@network NOTICE nick :This nickname is registered. "
        "Please choose a different nickname, or identify via /msg NickServ "
        "identify <password>.");
    ircd.ReadUntil("NICKSERV IDENTIFY hunter2");

    // OnPrivTextMessage
    ircd.Write(
        ":NickServ!services@network PRIVMSG nick :This nickname is registered. "
        "Please choose a different nickname, or identify via /msg NickServ "
        "identify <password>.");
    ircd.ReadUntil("NICKSERV IDENTIFY hunter2");
}

TEST_F(ZNCTest, StickyChanModule) {
    auto znc = Run();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();

    client.Write("znc loadmod stickychan");
    client.ReadUntil("Loaded module");

    client.Write("PRIVMSG *stickychan :stick #sticky");
    client.ReadUntil("Stuck #sticky");

    ircd.Write("001 nick Welcome");
    ircd.Write(":nick JOIN :#sticky");

    client.ReadUntil("JOIN :#sticky");
    client.Write("PART #sticky :leaving");

    ASSERT_THAT(ircd.ReadRemainder(), Not(HasSubstr("PART")));

    client.Write("PRIVMSG *controlpanel :GetChan InConfig $me $network #sticky");
    client.ReadUntil("InConfig = true");
}


}  // namespace
}  // namespace znc_inttest
