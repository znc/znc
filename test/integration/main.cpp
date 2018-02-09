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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

#ifndef ZNC_BIN_DIR
#define ZNC_BIN_DIR ""
#endif

using testing::AnyOf;
using testing::Eq;
using testing::HasSubstr;

namespace {

template <typename Device>
class IO {
  public:
    IO(Device* device, bool verbose = false)
        : m_device(device), m_verbose(verbose) {}
    virtual ~IO() {}
    void ReadUntil(QByteArray pattern) {
        auto deadline = QDateTime::currentDateTime().addSecs(60);
        while (true) {
            int search = m_readed.indexOf(pattern);
            if (search != -1) {
                m_readed.remove(0, search + pattern.length());
                return;
            }
            if (m_readed.length() > pattern.length()) {
                m_readed = m_readed.right(pattern.length());
            }
            const int timeout_ms =
                QDateTime::currentDateTime().msecsTo(deadline);
            ASSERT_GT(timeout_ms, 0) << "Wanted:" << pattern.toStdString();
            ASSERT_TRUE(m_device->waitForReadyRead(timeout_ms))
                << "Wanted: " << pattern.toStdString();
            QByteArray chunk = m_device->readAll();
            if (m_verbose) {
                std::cout << chunk.toStdString() << std::flush;
            }
            m_readed += chunk;
        }
    }
    /*
     * Reads from Device until pattern is matched and returns this pattern
     * up to and excluding the first newline. Pattern itself can contain a newline.
     * Have to use second param as the ASSERT_*'s return a non-QByteArray.
     */
    void ReadUntilAndGet(QByteArray pattern, QByteArray& match) {
        auto deadline = QDateTime::currentDateTime().addSecs(60);
        while (true) {
            int search = m_readed.indexOf(pattern);
            if (search != -1) {
                int start = 0;
                /* Don't look for what we've already found */
                if (pattern != "\n") {
                  int patlen = pattern.length();
                  start = search;
                  pattern = QByteArray("\n");
                  search = m_readed.indexOf(pattern, start + patlen);
                }
                if (search != -1) {
                  match += m_readed.mid(start, search - start);
                  m_readed.remove(0, search + 1);
                  return;
                }
                /* No newline yet, add to retvalue and trunc output */
                match += m_readed.mid(start);
                m_readed.resize(0);
            }
            if (m_readed.length() > pattern.length()) {
                m_readed = m_readed.right(pattern.length());
            }
            const int timeout_ms =
                QDateTime::currentDateTime().msecsTo(deadline);
            ASSERT_GT(timeout_ms, 0) << "Wanted:" << pattern.toStdString();
            ASSERT_TRUE(m_device->waitForReadyRead(timeout_ms))
                << "Wanted: " << pattern.toStdString();
            QByteArray chunk = m_device->readAll();
            if (m_verbose) {
                std::cout << chunk.toStdString() << std::flush;
            }
            m_readed += chunk;
        }
    }
    void Write(QByteArray s = "", bool new_line = true) {
        if (!m_device) return;
        if (m_verbose) {
            std::cout << s.toStdString() << std::flush;
            if (new_line) {
                std::cout << std::endl;
            }
        }
        s += "\n";
        while (!s.isEmpty()) {
            auto res = m_device->write(s);
            ASSERT_NE(res, -1);
            s.remove(0, res);
        }
        FlushIfCan(m_device);
    }
    void Close() {
#ifdef __CYGWIN__
        // Qt on cygwin silently doesn't send the rest of buffer from socket
        // without this line
        sleep(1);
#endif
        m_device->disconnectFromHost();
    }

  private:
    // Need to flush QTcpSocket, and QIODevice doesn't have flush at all...
    static void FlushIfCan(QIODevice*) {}
    static void FlushIfCan(QTcpSocket* sock) { sock->flush(); }

    Device* m_device;
    bool m_verbose;
    QByteArray m_readed;
};

template <typename Device>
IO<Device> WrapIO(Device* d) {
    return IO<Device>(d);
}

using Socket = IO<QTcpSocket>;

class Process : public IO<QProcess> {
  public:
    Process(QString cmd, QStringList args,
            std::function<void(QProcess*)> setup = [](QProcess*) {})
        : IO(&m_proc, true) {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("ZNC_DEBUG_TIMER", "1");
        // Default exit codes of sanitizers upon error:
        // ASAN - 1
        // LSAN - 23 (part of ASAN, but uses a different value)
        // TSAN - 66
        //
        // ZNC uses 1 too to report startup failure.
        // But we don't want to confuse expected startup failure with ASAN
        // error.
        env.insert("ASAN_OPTIONS", "exitcode=57");
        m_proc.setProcessEnvironment(env);
        setup(&m_proc);
        m_proc.start(cmd, args);
        EXPECT_TRUE(m_proc.waitForStarted())
            << "Failed to start ZNC, did you install it?";
    }
    ~Process() override {
        if (m_kill) m_proc.terminate();
        [this]() {
            ASSERT_TRUE(m_proc.waitForFinished());
            if (!m_allowDie) {
                ASSERT_EQ(QProcess::NormalExit, m_proc.exitStatus());
                if (m_allowLeak) {
                    ASSERT_THAT(m_proc.exitStatus(), AnyOf(Eq(23), Eq(m_exit)));
                } else {
                    ASSERT_EQ(m_exit, m_proc.exitCode());
                }
            }
        }();
    }
    void ShouldFinishItself(int code = 0) {
        m_kill = false;
        m_exit = code;
    }
    void CanDie() { m_allowDie = true; }

    // I can't do much about SWIG...
    void CanLeak() { m_allowLeak = true; }

  private:
    bool m_kill = true;
    int m_exit = 0;
    bool m_allowDie = false;
    bool m_allowLeak = false;
    QProcess m_proc;
};

void WriteConfig(QString path) {
    // clang-format off
    Process p(ZNC_BIN_DIR "/znc", QStringList() << "--debug"
                                                << "--datadir" << path
                                                << "--makeconf");
    p.ReadUntil("Listen on port");          p.Write("12345");
    p.ReadUntil("Listen using SSL");        p.Write();
    p.ReadUntil("IPv6");                    p.Write();
    p.ReadUntil("Username");                p.Write("user");
    p.ReadUntil("password");                p.Write("hunter2", false);
    p.ReadUntil("Confirm");                 p.Write("hunter2", false);
    p.ReadUntil("Nick [user]");             p.Write();
    p.ReadUntil("Alternate nick [user_]");  p.Write();
    p.ReadUntil("Ident [user]");            p.Write();
    p.ReadUntil("Real name");               p.Write();
    p.ReadUntil("Bind host");               p.Write();
    p.ReadUntil("Set up a network?");       p.Write();
    p.ReadUntil("Name [freenode]");         p.Write("test");
    p.ReadUntil("Server host (host only)"); p.Write("127.0.0.1");
    p.ReadUntil("Server uses SSL?");        p.Write();
    p.ReadUntil("6667");                    p.Write();
    p.ReadUntil("password");                p.Write();
    p.ReadUntil("channels");                p.Write();
    p.ReadUntil("Launch ZNC now?");         p.Write("no");
    p.ShouldFinishItself();
    // clang-format on
}

TEST(Config, AlreadyExists) {
    QTemporaryDir dir;
    WriteConfig(dir.path());
    Process p(ZNC_BIN_DIR "/znc", QStringList() << "--debug"
                                                << "--datadir" << dir.path()
                                                << "--makeconf");
    p.ReadUntil("already exists");
    p.CanDie();
}

// Can't use QEventLoop without existing QCoreApplication
class App {
  public:
    App() : m_argv(new char{}), m_app(m_argc, &m_argv) {}
    ~App() { delete m_argv; }

  private:
    int m_argc = 1;
    char* m_argv;
    QCoreApplication m_app;
};

class ZNCTest : public testing::Test {
  protected:
    void SetUp() override {
        WriteConfig(m_dir.path());
        ASSERT_TRUE(m_server.listen(QHostAddress::LocalHost, 6667))
            << m_server.errorString().toStdString();
    }

    Socket ConnectIRCd() {
        [this] {
            ASSERT_TRUE(m_server.waitForNewConnection(30000 /* msec */));
        }();
        return WrapIO(m_server.nextPendingConnection());
    }

    Socket ConnectClient() {
        m_clients.emplace_back();
        QTcpSocket& sock = m_clients.back();
        sock.connectToHost("127.0.0.1", 12345);
        [&] {
            ASSERT_TRUE(sock.waitForConnected())
                << sock.errorString().toStdString();
        }();
        return WrapIO(&sock);
    }

    std::unique_ptr<Process> Run() {
        return std::unique_ptr<Process>(new Process(
            ZNC_BIN_DIR "/znc", QStringList() << "--debug"
                                              << "--datadir" << m_dir.path(),
            [](QProcess* proc) {
                proc->setProcessChannelMode(QProcess::ForwardedChannels);
            }));
    }

    Socket LoginClient() {
        auto client = ConnectClient();
        client.Write("PASS :hunter2");
        client.Write("NICK nick");
        client.Write("USER user/test x x :x");
        return client;
    }

    std::unique_ptr<QNetworkReply> HttpGet(QNetworkRequest request) {
        return HandleHttp(m_network.get(request));
    }
    std::unique_ptr<QNetworkReply> HttpPost(
        QNetworkRequest request, QList<QPair<QString, QString>> data) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/x-www-form-urlencoded");
        QUrlQuery q;
        q.setQueryItems(data);
        return HandleHttp(m_network.post(request, q.toString().toUtf8()));
    }
    std::unique_ptr<QNetworkReply> HandleHttp(QNetworkReply* reply) {
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, [&]() {
            std::cout << "Got HTTP reply" << std::endl;
            loop.quit();
        });
        QObject::connect(
            reply,
            static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(
                &QNetworkReply::error),
            [&](QNetworkReply::NetworkError e) {
                ADD_FAILURE() << reply->errorString().toStdString();
            });
        QTimer::singleShot(30000 /* msec */, &loop, [&]() {
            ADD_FAILURE() << "connection timeout";
            loop.quit();
        });
        std::cout << "Start HTTP loop.exec()" << std::endl;
        loop.exec();
        std::cout << "Finished HTTP loop.exec()" << std::endl;
        return std::unique_ptr<QNetworkReply>(reply);
    }

    void InstallModule(QString name, QString content) {
        QDir dir(m_dir.path());
        ASSERT_TRUE(dir.mkpath("modules"));
        ASSERT_TRUE(dir.cd("modules"));
        if (name.endsWith(".cpp")) {
            // Compile
            QTemporaryDir srcdir;
            QFile file(QDir(srcdir.path()).filePath(name));
            ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << content;
            file.close();
            Process p(
                ZNC_BIN_DIR "/znc-buildmod", QStringList() << file.fileName(),
                [&](QProcess* proc) {
                    proc->setWorkingDirectory(dir.absolutePath());
                    proc->setProcessChannelMode(QProcess::ForwardedChannels);
                });
            p.ShouldFinishItself();
        } else if (name.endsWith(".py")) {
            // Dedent
            QStringList lines = content.split("\n");
            int maxoffset = -1;
            for (const QString& line : lines) {
                int nonspace = line.indexOf(QRegExp("\\S"));
                if (nonspace == -1) continue;
                if (nonspace < maxoffset || maxoffset == -1)
                    maxoffset = nonspace;
            }
            if (maxoffset == -1) maxoffset = 0;
            QFile file(dir.filePath(name));
            ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            for (const QString& line : lines) {
                // QTextStream::operator<<(const QStringRef &string) was
                // introduced in Qt 5.6; let's keep minimum required version
                // less than that for now.
                out << line.mid(maxoffset) << "\n";
            }
        } else {
            // Write as is
            QFile file(dir.filePath(name));
            ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << content;
        }
    }

    App m_app;
    QNetworkAccessManager m_network;
    QTemporaryDir m_dir;
    QTcpServer m_server;
    std::list<QTcpSocket> m_clients;
};

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
    }
    {
        Process p(ZNC_BIN_DIR "/znc-buildmod",
                  QStringList() << srcdir.filePath("testmod.cpp"),
                  [&](QProcess* proc) {
                      proc->setWorkingDirectory(dir.absolutePath());
                      proc->setProcessChannelMode(QProcess::ForwardedChannels);
                  });
        p.ShouldFinishItself();
    }
    client.Write("znc loadmod testmod");
    client.Write("PRIVMSG *testmod :hi");
    client.ReadUntil("Lorem ipsum");
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
    client1.Write("PRIVMSG .nick2 :Hello");
    QByteArray secretmsg;
    ircd1.ReadUntilAndGet("PRIVMSG nick2 :+OK ", secretmsg);
    ircd2.Write(":user!user@user/test " + secretmsg);
    client2.ReadUntil("Hello");
}

}  // namespace

class ThrowListener : public testing::EmptyTestEventListener {
    void OnTestPartResult(const testing::TestPartResult& result) override {
        if (result.type() == testing::TestPartResult::kFatalFailure &&
                !std::uncaught_exception()) {
            throw testing::AssertionException(result);
        }
    }
};

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    testing::UnitTest::GetInstance()->listeners().Append(new ThrowListener);
    return RUN_ALL_TESTS();
}
