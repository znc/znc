/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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
#include "znctestconfig.h"

namespace znc_inttest {
namespace {

TEST_F(ZNCTest, Modperl) {
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod perleval");
    client.Write("PRIVMSG *perleval :2+2");
    client.ReadUntil(":*perleval!perleval@znc.in PRIVMSG nick :Result: 4");
    client.Write("PRIVMSG *perleval :$self->GetUser->GetUsername");
    client.ReadUntil("Result: user");
}

TEST_F(ZNCTest, Modpython) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod pyeval");
    client.Write("PRIVMSG *pyeval :2+2");
    client.ReadUntil(":*pyeval!pyeval@znc.in PRIVMSG nick :4");
    client.Write("PRIVMSG *pyeval :module.GetUser().GetUsername()");
    client.ReadUntil("nick :'user'");
    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":n!u@h PRIVMSG nick :Hi\xF0, github issue #1229");
    // "replacement character"
    client.ReadUntil("Hi\xEF\xBF\xBD, github issue");

    // Non-existing encoding
    client.Write("PRIVMSG *controlpanel :Set ClientEncoding $me Western");
    client.Write("JOIN #a\342");
    client.ReadUntil(
        ":*controlpanel!controlpanel@znc.in PRIVMSG nick :ClientEncoding = UTF-8");
    ircd.ReadUntil("JOIN #a\xEF\xBF\xBD");
}

TEST_F(ZNCTest, ModpythonSocket) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("socktest.py", R"(
        import znc

        class acc(znc.Socket):
            def OnReadData(self, data):
                self.GetModule().PutModule('received {} bytes'.format(len(data)))
                self.Close()

        class lis(znc.Socket):
            def OnAccepted(self, host, port):
                sock = self.GetModule().CreateSocket(acc)
                sock.DisableReadLine()
                return sock

        class socktest(znc.Module):
            def OnLoad(self, args, ret):
                listen = self.CreateSocket(lis)
                self.port = listen.Listen()
                return True

            def OnModCommand(self, cmd):
                sock = self.CreateSocket()
                sock.Connect('127.0.0.1', self.port)
                sock.WriteBytes(b'blah')
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod socktest");
    client.Write("PRIVMSG *socktest :foo");
    client.ReadUntil("received 4 bytes");
}

TEST_F(ZNCTest, ModperlSocket) {
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("socktest.pm", R"(
        package socktest::acc;
        use base 'ZNC::Socket';
        sub OnReadData {
            my ($self, $data, $len) = @_;
            $self->GetModule->PutModule("received $len bytes");
            $self->Close;
        }

        package socktest::lis;
        use base 'ZNC::Socket';
        sub OnAccepted {
            my $self = shift;
            return $self->GetModule->CreateSocket('socktest::acc');
        }

        package socktest::conn;
        use base 'ZNC::Socket';

        package socktest;
        use base 'ZNC::Module';
        sub OnLoad {
            my $self = shift;
            my $listen = $self->CreateSocket('socktest::lis');
            $self->{port} = $listen->Listen;
            return 1;
        }
        sub OnModCommand {
            my ($self, $cmd) = @_;
            my $sock = $self->CreateSocket('socktest::conn');
            $sock->Connect('127.0.0.1', $self->{port});
            $sock->Write('blah');
        }

        1;
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod socktest");
    client.Write("PRIVMSG *socktest :foo");
    client.ReadUntil("received 4 bytes");
}

TEST_F(ZNCTest, ModpythonUnixSocket) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
#ifdef __CYGWIN__
    GTEST_SKIP() << "Bug to fix: https://github.com/znc/znc/issues/1947";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("socktest.py", R"(
        import znc

        class acc(znc.Socket):
            def OnReadData(self, data):
                self.GetModule().PutModule('received {} bytes'.format(len(data)))
                self.Close()

        class lis(znc.Socket):
            def OnAccepted(self, host, port):
                sock = self.GetModule().CreateSocket(acc)
                sock.DisableReadLine()
                return sock

        class socktest(znc.Module):
            def OnLoad(self, args, ret):
                listen = self.CreateSocket(lis)
                return listen.Listen(addrtype='unix', path=self.TestSockPath())

            def OnModCommand(self, cmd):
                sock = self.CreateSocket()
                sock.ConnectUnix(self.TestSockPath())
                sock.WriteBytes(b'blah')

            def TestSockPath(self):
                path = self.GetSavePath() + "/sock"
                # https://unix.stackexchange.com/questions/367008/why-is-socket-path-length-limited-to-a-hundred-chars
                if len(path) < 100:
                    return path
                return "./testsock.modpython"
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod socktest");
    client.ReadUntil("Loaded module socktest");
    client.Write("PRIVMSG *socktest :foo");
    client.ReadUntil("received 4 bytes");
}

TEST_F(ZNCTest, ModperlUnixSocket) {
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
#ifdef __CYGWIN__
    GTEST_SKIP() << "Bug to fix: https://github.com/znc/znc/issues/1947";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("socktest.pm", R"(
        package socktest::acc;
        use base 'ZNC::Socket';
        sub OnReadData {
            my ($self, $data, $len) = @_;
            $self->GetModule->PutModule("received $len bytes");
            $self->Close;
        }

        package socktest::lis;
        use base 'ZNC::Socket';
        sub OnAccepted {
            my $self = shift;
            return $self->GetModule->CreateSocket('socktest::acc');
        }

        package socktest::conn;
        use base 'ZNC::Socket';

        package socktest;
        use base 'ZNC::Module';
        sub OnLoad {
            my $self = shift;
            my $listen = $self->CreateSocket('socktest::lis');
            $listen->Listen(addrtype=>'unix', path=>$self->TestSockPath);
        }
        sub OnModCommand {
            my ($self, $cmd) = @_;
            my $sock = $self->CreateSocket('socktest::conn');
            $sock->ConnectUnix($self->TestSockPath);
            $sock->Write('blah');
        }
        sub TestSockPath {
            my $self = shift;
            my $path = $self->GetSavePath . "/sock";
            # https://unix.stackexchange.com/questions/367008/why-is-socket-path-length-limited-to-a-hundred-chars
            if (length($path) < 100) {
                return $path;
            }
            return "./testsock.modperl";
        }

        1;
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod socktest");
    client.ReadUntil("Loaded module socktest");
    client.Write("PRIVMSG *socktest :foo");
    client.ReadUntil("received 4 bytes");
}

TEST_F(ZNCTest, ModpythonVCString) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("test.py", R"(
        import znc

        class test(znc.Module):
            def OnUserRawMessage(self, msg):
                self.PutModule(str(msg.GetParams()))
                return znc.CONTINUE
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod test");
    sleep(1);
    client.Write("PRIVMSG *test :foo");
    client.ReadUntil("'*test', 'foo'");
}

TEST_F(ZNCTest, ModperlVCString) {
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("test.pm", R"(
        package test;
        use base 'ZNC::Module';
        sub OnUserRawMessage {
            my ($self, $msg) = @_;
            my @params = $msg->GetParams;
            $self->PutModule("@params");
            return $ZNC::CONTINUE;
        }

        1;
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod test");
    client.Write("PRIVMSG *test :foo");
    client.ReadUntil(":*test foo");
}

TEST_F(ZNCTest, ModperlNV) {
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("test.pm", R"(
        package test;
        use base 'ZNC::Module';
        sub OnLoad {
            my $self = shift;
            $self->SetNV('a', 'X');
            $self->NV->{b} = 'Y';
            my @k = keys %{$self->NV};
            $self->PutModule("@k");
            return $ZNC::CONTINUE;
        }

        1;
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod test");
    client.ReadUntil(":a b");
}

TEST_F(ZNCTest, ModpythonPackage) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    QDir dir(m_dir.path());
    ASSERT_TRUE(dir.mkpath("modules"));
    ASSERT_TRUE(dir.cd("modules"));
    ASSERT_TRUE(dir.mkpath("packagetest"));
    InstallModule("packagetest/__init__.py", R"(
        import znc
        from .data import value

        class packagetest(znc.Module):
            def OnModCommand(self, cmd):
                self.PutModule('value = ' + value)
    )");
    InstallModule("packagetest/data.py", "value = 'a'");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod packagetest");
    client.Write("PRIVMSG *packagetest :foo");
    client.ReadUntil("value = a");
    InstallModule("packagetest/data.py", "value = 'b'");
    client.Write("PRIVMSG *packagetest :foo");
    client.ReadUntil("value = a");
    client.Write("znc updatemod packagetest");
    client.Write("PRIVMSG *packagetest :foo");
    client.ReadUntil("value = b");
    // Test if python modules are viewable via *status.
    // https://github.com/znc/znc/issues/1884
    client.Write("znc listavailmods");
    client.ReadUntil(":*status!status@znc.in PRIVMSG nick :\x02 packagetest");
    client.ReadUntil(":*status!status@znc.in PRIVMSG nick :\x02 pyeval\x0F: Evaluates python code");
}

TEST_F(ZNCTest, ModpythonModperl) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    // https://github.com/znc/znc/issues/1757
    client.Write("znc loadmod modpython");
    client.ReadUntil("Loaded module modpython");
    client.Write("znc loadmod modperl");
    client.ReadUntil("Loaded module modperl");
}

TEST_F(ZNCTest, ModpythonCommand) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("cmdtest.py", R"(
        import znc

        class cmdtest(znc.Module):
            def OnLoad(self, args, message):
                self.AddHelpCommand()
                self.AddCommand(testcmd)
                return True

        class testcmd(znc.Command):
            command = 'ping'
            args = cmdtest.t_d('ar')
            description = cmdtest.t_d('blah')

            def __call__(self, line):
                self.GetModule().PutModule(line + cmdtest.t_s(' pong'))
    )");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod cmdtest");
    client.Write("PRIVMSG *cmdtest :ping or");
    client.ReadUntil(":*cmdtest!cmdtest@znc.in PRIVMSG nick :ping or pong");

    InstallTranslation("cmdtest", "ru_RU", R"(
        msgid ""
        msgstr ""
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "Plural-Forms: nplurals=4; plural=((n%10==1 && n%100!=11) ? 0 : ((n%10 >= 2 "
        "&& n%10 <=4 && (n%100 < 12 || n%100 > 14)) ? 1 : ((n%10 == 0 || (n%10 >= 5 "
        "&& n%10 <=9)) || (n%100 >= 11 && n%100 <= 14)) ? 2 : 3));\n"
        "Language: ru_RU\n"

        msgid "ar"
        msgstr "аргумент"

        msgid "blah"
        msgstr "бла"

        msgid " pong"
        msgstr " понг"
    )");

    client.Write("PRIVMSG *controlpanel :set language $me ru-RU");
    client.Write("PRIVMSG *cmdtest :help");
    client.ReadUntil(":*cmdtest!cmdtest@znc.in PRIVMSG nick :\x02ping аргумент\x0F: бла");
    client.Write("PRIVMSG *cmdtest :ping");
    client.ReadUntil(":*cmdtest!cmdtest@znc.in PRIVMSG nick :ping понг");
}

TEST_F(ZNCTest, ModpythonSaslAuth) {
#ifndef WANT_PYTHON
    GTEST_SKIP() << "Modpython is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("sasltest.py", R"(
        import znc

        class sasltest(znc.Module):

            module_types = [znc.CModInfo.GlobalModule]

            def OnClientGetSASLMechanisms(self, ssMechanisms):
                ssMechanisms.insert("FOO")

            def OnClientSASLServerInitialChallenge(self, sMechanism, sResponse):
                if sMechanism == "FOO":
                    sResponse.s = "Welcome"
                return znc.CONTINUE

            def OnClientSASLAuthenticate(self, sMechanism, sMessage):
                if sMechanism == "FOO":
                    user = znc.CZNC.Get().FindUser("user")
                    self.GetClient().AcceptSASLLogin(user)
                    return znc.HALT
                return znc.CONTINUE

    )");
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod sasltest");
    client.ReadUntil("Loaded");

    auto client2 = ConnectClient();
    client2.Write("CAP LS 302");
    client2.Write("NICK nick");
    client2.ReadUntil(" sasl=FOO,PLAIN ");
    client2.Write("CAP REQ :sasl");
    client2.Write("AUTHENTICATE FOO");
    client2.ReadUntil("AUTHENTICATE " + QByteArrayLiteral("Welcome").toBase64());
    client2.Write("AUTHENTICATE +");
    client2.ReadUntilRe(
        ":irc.znc.in 900 nick nick!user@(localhost)? user :You are now logged "
        "in as user");
}

TEST_F(ZNCTest, ModperlSaslAuth) {
#ifndef WANT_PERL
    GTEST_SKIP() << "Modperl is disabled";
#endif
    auto znc = Run();
    znc->CanLeak();

    InstallModule("sasltest.pm", R"(
		package sasltest;
		use base 'ZNC::Module';
		sub module_types { $ZNC::CModInfo::GlobalModule }

		sub OnClientGetSASLMechanisms {
			my $self = shift;
			my $mechs = shift;
			$mechs->insert('FOO');
		}

		sub OnClientSASLServerInitialChallenge {
			if ($_[1] eq "FOO") {
				$_[2] = "Welcome";
			}
			return $ZNC::CONTINUE;
		}

		sub OnClientSASLAuthenticate {
			my $self = $_[0];
			if ($_[1] eq "FOO") {
				my $user = ZNC::CZNC::Get()->FindUser("user");
				$self->GetClient->AcceptSASLLogin($user);
				return $ZNC::HALT;
			}
			return $ZNC::CONTINUE;
		}

		1;
)");

    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod sasltest");
    client.ReadUntil("Loaded");

    auto client2 = ConnectClient();
    client2.Write("CAP LS 302");
    client2.Write("NICK nick");
    client2.ReadUntil(" sasl=FOO,PLAIN ");
    client2.Write("CAP REQ :sasl");
    client2.Write("AUTHENTICATE FOO");
    client2.ReadUntil("AUTHENTICATE " + QByteArrayLiteral("Welcome").toBase64());
    client2.Write("AUTHENTICATE +");
    client2.ReadUntilRe(
        ":irc.znc.in 900 nick nick!user@(localhost)? user :You are now logged "
        "in as user");
}

}  // namespace
}  // namespace znc_inttest
