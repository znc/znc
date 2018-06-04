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

namespace znc_inttest {
namespace {

TEST_F(ZNCTest, Modperl) {
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
    auto znc = Run();
    znc->CanLeak();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modperl");
    client.Write("znc loadmod perleval");
    client.Write("PRIVMSG *perleval :2+2");
    client.ReadUntil(":*perleval!znc@znc.in PRIVMSG nick :Result: 4");
    client.Write("PRIVMSG *perleval :$self->GetUser->GetUserName");
    client.ReadUntil("Result: user");
}

TEST_F(ZNCTest, Modpython) {
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
    auto znc = Run();
    znc->CanLeak();
    auto ircd = ConnectIRCd();
    auto client = LoginClient();
    client.Write("znc loadmod modpython");
    client.Write("znc loadmod pyeval");
    client.Write("PRIVMSG *pyeval :2+2");
    client.ReadUntil(":*pyeval!znc@znc.in PRIVMSG nick :4");
    client.Write("PRIVMSG *pyeval :module.GetUser().GetUserName()");
    client.ReadUntil("nick :'user'");
    ircd.Write(":server 001 nick :Hello");
    ircd.Write(":n!u@h PRIVMSG nick :Hi\xF0, github issue #1229");
    // "replacement character"
    client.ReadUntil("Hi\xEF\xBF\xBD, github issue");
}

TEST_F(ZNCTest, ModpythonSocket) {
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
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
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
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

TEST_F(ZNCTest, ModpythonVCString) {
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
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
    client.Write("PRIVMSG *test :foo");
    client.ReadUntil("'*test', 'foo'");
}

TEST_F(ZNCTest, ModperlVCString) {
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
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
    if (QProcessEnvironment::systemEnvironment().value(
            "DISABLED_ZNC_PERL_PYTHON_TEST") == "1") {
        return;
    }
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

}  // namespace
}  // namespace znc_inttest
