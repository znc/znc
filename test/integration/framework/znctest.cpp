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

#include <QRegularExpression>
#include <QProcess>
#include "znctest.h"
#include "cygwin.h"

#ifndef ZNC_BIN_DIR
#define ZNC_BIN_DIR ""
#endif

namespace znc_inttest {

void WriteConfig(QString path) {
    Process p(ZNC_BIN_DIR "/znc",
              QStringList() << "--debug" << "--datadir" << path << "--makeconf",
              [=](QProcess* p) {
                  auto env = p->processEnvironment();
                  env.insert("ZNC_LISTEN_UNIX_SOCKET",
                             path + "/inttest.znc");
                  p->setProcessEnvironment(env);
              });
    // clang-format off
    p.ReadUntil("Username");                p.Write("user");
    p.ReadUntil("password");                p.Write("hunter2", false);
    p.ReadUntil("Confirm");                 p.Write("hunter2", false);
    p.ReadUntil("Nick [user]");             p.Write();
    p.ReadUntil("Alternate nick [user_]");  p.Write();
    p.ReadUntil("Ident [user]");            p.Write();
    p.ReadUntil("Real name");               p.Write();
    p.ReadUntil("Bind host");               p.Write();
    p.ReadUntil("Set up a network?");       p.Write();
    p.ReadUntil("Name [libera]");           p.Write("test");
    p.ReadUntil("Server host (host only)"); p.Write("unix:" + path.toUtf8() + "/inttest.ircd");
    p.ReadUntil("Server uses SSL?");        p.Write();
    p.ReadUntil("6667");                    p.Write();
    p.ReadUntil("password");                p.Write();
    p.ReadUntil("channels");                p.Write();
    p.ReadUntil("Launch ZNC now?");         p.Write("no");
    p.ShouldFinishItself();
    // clang-format on

    // Default 30s is too slow for the test
    QFile conf(path + "/configs/znc.conf");
    ASSERT_TRUE(conf.open(QIODevice::Append | QIODevice::Text));
    QTextStream out(&conf);
    out << R"(
        ServerThrottle = 5
    )";
}

void ZNCTest::SetUp() {
    WriteConfig(m_dir.path());
    ASSERT_TRUE(m_server.listen(m_dir.path() + "/inttest.ircd"))
        << m_server.errorString().toStdString();
}

Socket ZNCTest::ConnectIRCd() {
    [this] { ASSERT_TRUE(m_server.waitForNewConnection(30000 /* msec */)); }();
    return WrapIO(m_server.nextPendingConnection());
}

Socket ZNCTest::ConnectClient() {
    m_clients.emplace_back();
    QLocalSocket& sock = m_clients.back();
    sock.setServerName(m_dir.path() + "/inttest.znc");
#ifdef __CYGWIN__
    znc_inttest_cygwin::CygwinWorkaroundLocalConnect(sock);
#else
    sock.connectToServer();
#endif
    [&] {
        ASSERT_TRUE(sock.waitForConnected())
            << sock.errorString().toStdString();
    }();
    return WrapIO(&sock);
}

Socket ZNCTest::LoginClient(QString identifier) {
    auto client = ConnectClient();
    client.Write("PASS :hunter2");
    client.Write("NICK nick");
    if ( identifier.length() == 0 ) {
        client.Write("USER user/test x x :x");
    } else {
        client.Write("USER user@" + identifier.toUtf8() + "/test x x :x");
    }
    return client;
}

std::unique_ptr<Process> ZNCTest::Run() {
    return std::unique_ptr<Process>(new Process(
        ZNC_BIN_DIR "/znc",
        QStringList() << "--debug"
                      << "--datadir" << m_dir.path(),
        [](QProcess* proc) {
            proc->setProcessChannelMode(QProcess::ForwardedChannels);
        }));
}

std::unique_ptr<QNetworkReply> ZNCTest::HttpGet(QNetworkRequest request) {
    return HandleHttp(m_network.get(request));
}
std::unique_ptr<QNetworkReply> ZNCTest::HttpPost(
    QNetworkRequest request, QList<QPair<QString, QString>> data) {
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");
    QUrlQuery q;
    q.setQueryItems(data);
    return HandleHttp(m_network.post(request, q.toString().toUtf8()));
}
std::unique_ptr<QNetworkReply> ZNCTest::HandleHttp(QNetworkReply* reply) {
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        std::cout << "Got HTTP reply" << std::endl;
        loop.quit();
    });
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    QObject::connect(reply, &QNetworkReply::errorOccurred,
                     [&](QNetworkReply::NetworkError e) {
                         ADD_FAILURE() << reply->errorString().toStdString();
                     });
#else
    QObject::connect(
        reply,
        static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(
            &QNetworkReply::error),
        [&](QNetworkReply::NetworkError e) {
            ADD_FAILURE() << reply->errorString().toStdString();
        });
#endif
    QTimer::singleShot(30000 /* msec */, &loop, [&]() {
        ADD_FAILURE() << "connection timeout";
        loop.quit();
    });
    std::cout << "Start HTTP loop.exec()" << std::endl;
    loop.exec();
    std::cout << "Finished HTTP loop.exec()" << std::endl;
    return std::unique_ptr<QNetworkReply>(reply);
}

void ZNCTest::InstallModule(QString name, QString content) {
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
        Process p(ZNC_BIN_DIR "/znc-buildmod", QStringList() << file.fileName(),
                  [&](QProcess* proc) {
                      proc->setWorkingDirectory(dir.absolutePath());
                      proc->setProcessChannelMode(QProcess::ForwardedChannels);
                  });
        p.ShouldFinishItself();
        p.ShouldFinishInSec(300);
    } else if (name.endsWith(".py")) {
        // Dedent
        QStringList lines = content.split("\n");
        int maxoffset = -1;
        for (const QString& line : lines) {
            int nonspace = line.indexOf(QRegularExpression("\\S"));
            if (nonspace == -1) continue;
            if (nonspace < maxoffset || maxoffset == -1) maxoffset = nonspace;
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

void ZNCTest::InstallTranslation(QString module, QString language, QString content) {
    QDir dir(m_dir.path());
    for (QString d : std::vector<QString>{"modules", "locale", language, "LC_MESSAGES"}) {
        ASSERT_TRUE(dir.mkpath(d)) << d.toStdString();
        ASSERT_TRUE(dir.cd(d)) << d.toStdString();
    }
    QTemporaryDir srcdir;
    QFile file(QDir(srcdir.path()).filePath("foo.po"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << content;
    file.close();
    {
        Process p("msgfmt", QStringList() << "-D" << "." << "-o" << "foo.mo" << "foo.po",
            [&](QProcess* proc) {
                proc->setWorkingDirectory(srcdir.path());
                proc->setProcessChannelMode(QProcess::ForwardedChannels);
            });
        p.ShouldFinishItself();
        p.ShouldFinishInSec(300);
    }
    QFile result(QDir(srcdir.path()).filePath("foo.mo"));
    result.rename(dir.filePath("znc-" + module + ".mo"));
}


}  // namespace znc_inttest
