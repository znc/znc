/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#define Z do { if (::testing::Test::HasFatalFailure()) { std::cerr << "At: " << __FILE__ << ":" << __LINE__ << std::endl; return; } } while (0)

using testing::HasSubstr;

namespace {

template <typename Device>
class IO {
public:
	IO(Device* device, bool verbose = false) : m_device(device), m_verbose(verbose) {}
	virtual ~IO() {}
	void ReadUntil(QByteArray pattern) {
		auto deadline = QDateTime::currentDateTime().addSecs(30);
		while (true) {
			int search = m_readed.indexOf(pattern);
			if (search != -1) {
				m_readed.remove(0, search + pattern.length());
				return;
			}
			if (m_readed.length() > pattern.length()) {
				m_readed = m_readed.right(pattern.length());
			}
			const int timeout_ms = QDateTime::currentDateTime().msecsTo(deadline);
			ASSERT_GT(timeout_ms, 0) << "Wanted:" << pattern.toStdString();
			ASSERT_TRUE(m_device->waitForReadyRead(timeout_ms)) << "Wanted: " << pattern.toStdString();
			QByteArray chunk = m_device->readAll();
			if (m_verbose) {
				std::cout << chunk.toStdString() << std::flush;
			}
			m_readed += chunk;
		}
	}
	void Write(QString s = "", bool new_line = true) {
		if (!m_device) return;
		if (m_verbose) {
			std::cout << s.toStdString() << std::flush;
			if (new_line) {
				std::cout << std::endl;
			}
		}
		s += "\n";
		{
			QTextStream str(m_device);
			str << s;
		}
		FlushIfCan(m_device);
	}
	void Close() {
#ifdef __CYGWIN__
#ifdef __x86_64__
		// Qt on cygwin64 silently doesn't send the rest of buffer from socket without this line
		sleep(1);
#endif
#endif
		m_device->disconnectFromHost();
	}

private:
	// QTextStream doesn't flush QTcpSocket, and QIODevice doesn't have flush() at all...
	static void FlushIfCan(QIODevice*) {}
	static void FlushIfCan(QTcpSocket* sock) {
		sock->flush();
	}

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
	Process(QString cmd, QStringList args, bool interactive) : IO(&m_proc, true) {
		if (!interactive) {
			m_proc.setProcessChannelMode(QProcess::ForwardedChannels);
		}
		auto env = QProcessEnvironment::systemEnvironment();
		// Default exit codes of sanitizers upon error:
		// ASAN - 1
		// LSAN - 23 (part of ASAN, but uses a different value)
		// TSAN - 66
		//
		// ZNC uses 1 too to report startup failure.
		// But we don't want to confuse expected starup failure with ASAN error.
		env.insert("ASAN_OPTIONS", "exitcode=57");
		m_proc.setProcessEnvironment(env);
		m_proc.start(cmd, args);
	}
	~Process() {
		if (m_kill) m_proc.terminate();
		[this]() {
			ASSERT_TRUE(m_proc.waitForFinished());
			if (!m_allowDie) {
				ASSERT_EQ(QProcess::NormalExit, m_proc.exitStatus());
				ASSERT_EQ(m_exit, m_proc.exitCode());
			}
		}();
	}
	void ShouldFinishItself(int code = 0) {
		m_kill = false;
		m_exit = code;
	}
	void CanDie() {
		m_allowDie = true;
	}

private:
	bool m_kill = true;
	int m_exit = 0;
	bool m_allowDie = false;
	QProcess m_proc;
};

void WriteConfig(QString path) {
	Process p("./znc", QStringList() << "--debug" << "--datadir" << path << "--makeconf", true);
	p.ReadUntil("Listen on port");Z;          p.Write("12345");
	p.ReadUntil("Listen using SSL");Z;        p.Write();
	p.ReadUntil("IPv6");Z;                    p.Write();
	p.ReadUntil("Username");Z;                p.Write("user");
	p.ReadUntil("password");Z;                p.Write("hunter2", false);
	p.ReadUntil("Confirm");Z;                 p.Write("hunter2", false);
	p.ReadUntil("Nick [user]");Z;             p.Write();
	p.ReadUntil("Alternate nick [user_]");Z;  p.Write();
	p.ReadUntil("Ident [user]");Z;            p.Write();
	p.ReadUntil("Real name");Z;               p.Write();
	p.ReadUntil("Bind host");Z;               p.Write();
	p.ReadUntil("Set up a network?");Z;       p.Write();
	p.ReadUntil("Name [freenode]");Z;         p.Write("test");
	p.ReadUntil("Server host (host only)");Z; p.Write("127.0.0.1");
	p.ReadUntil("Server uses SSL?");Z;        p.Write();
	p.ReadUntil("6667");Z;                    p.Write();
	p.ReadUntil("password");Z;                p.Write();
	p.ReadUntil("channels");Z;                p.Write();
	p.ReadUntil("Launch ZNC now?");Z;         p.Write("no");
	p.ShouldFinishItself();
}

TEST(Config, AlreadyExists) {
	QTemporaryDir dir;
	WriteConfig(dir.path());Z;
	Process p("./znc", QStringList() << "--debug" << "--datadir" << dir.path() << "--makeconf", true);
	p.ReadUntil("already exists");Z;
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
		WriteConfig(m_dir.path());Z;
		ASSERT_TRUE(m_server.listen(QHostAddress::LocalHost, 6667)) << m_server.errorString().toStdString();Z;
	}

	Socket ConnectIRCd() {
		[this]{ ASSERT_TRUE(m_server.waitForNewConnection(30000 /* msec */)); }();
		return WrapIO(m_server.nextPendingConnection());
	}

	Socket ConnectClient() {
		m_clients.emplace_back();
		QTcpSocket& sock = m_clients.back();
		sock.connectToHost("127.0.0.1", 12345);
		[&]{ ASSERT_TRUE(sock.waitForConnected()) << sock.errorString().toStdString(); }();
		return WrapIO(&sock);
	}

	std::unique_ptr<Process> Run() {
		return std::unique_ptr<Process>(new Process("./znc", QStringList() << "--debug" << "--datadir" << m_dir.path(), false));
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
	std::unique_ptr<QNetworkReply> HttpPost(QNetworkRequest request, QList<QPair<QString, QString>> data) {
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
		QUrlQuery q;
		q.setQueryItems(data);
		return HandleHttp(m_network.post(request, q.toString().toUtf8()));
	}
	std::unique_ptr<QNetworkReply> HandleHttp(QNetworkReply* reply) {
		QEventLoop loop;
		QObject::connect(reply, &QNetworkReply::finished, [&]() {
			loop.quit();
		});
		QObject::connect(reply, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), [&](QNetworkReply::NetworkError e) {
			ADD_FAILURE() << reply->errorString().toStdString();
		});
		QTimer::singleShot(30000 /* msec */, &loop, [&]() {
			ADD_FAILURE() << "connection timeout";
			loop.quit();
		});
		loop.exec();
		return std::unique_ptr<QNetworkReply>(reply);
	}

	App m_app;
	QNetworkAccessManager m_network;
	QTemporaryDir m_dir;
	QTcpServer m_server;
	std::list<QTcpSocket> m_clients;
};

TEST_F(ZNCTest, Connect) {
	auto znc = Run();Z;

	auto ircd = ConnectIRCd();Z;
	ircd.ReadUntil("CAP LS");Z;

	auto client = ConnectClient();Z;
	client.Write("PASS :hunter2");
	client.Write("NICK nick");
	client.Write("USER user/test x x :x");
	client.ReadUntil("Welcome");Z;
	client.Close();

	client = ConnectClient();Z;
	client.Write("PASS :user:hunter2");
	client.Write("NICK nick");
	client.Write("USER u x x x");
	client.ReadUntil("Welcome");Z;
	client.Close();

	client = ConnectClient();Z;
	client.Write("NICK nick");
	client.Write("USER user x x x");
	client.ReadUntil("Configure your client to send a server password");
	client.Close();

	ircd.Write(":server 001 nick :Hello");
	ircd.ReadUntil("WHO");Z;
}

TEST_F(ZNCTest, Channel) {
	auto znc = Run();Z;
	auto ircd = ConnectIRCd();Z;

	auto client = LoginClient();Z;
	client.ReadUntil("Welcome");Z;
	client.Write("JOIN #znc");
	client.Close();

	ircd.Write(":server 001 nick :Hello");
	ircd.ReadUntil("JOIN #znc");Z;
	ircd.Write(":nick JOIN :#znc");
	ircd.Write(":server 353 nick #znc :nick");
	ircd.Write(":server 366 nick #znc :End of /NAMES list");

	client = LoginClient();Z;
	client.ReadUntil(":nick JOIN :#znc");Z;
}

TEST_F(ZNCTest, HTTP) {
	auto znc = Run();Z;
	auto ircd = ConnectIRCd();Z;
	auto reply = HttpGet(QNetworkRequest(QUrl("http://127.0.0.1:12345/")));Z;
	EXPECT_THAT(reply->rawHeader("Server").toStdString(), HasSubstr("ZNC"));
}

TEST_F(ZNCTest, FixCVE20149403) {
	auto znc = Run();Z;
	auto ircd = ConnectIRCd();Z;
	ircd.Write(":server 001 nick :Hello");
	ircd.Write(":server 005 nick CHANTYPES=# :supports");
	ircd.Write(":server PING :1");
	ircd.ReadUntil("PONG 1");Z;

	QNetworkRequest request;
	request.setRawHeader("Authorization", "Basic " + QByteArray("user:hunter2").toBase64());
	request.setUrl(QUrl("http://127.0.0.1:12345/mods/global/webadmin/addchan"));
	HttpPost(request, {
		{"user", "user"},
		{"network", "test"},
		{"submitted", "1"},
		{"name", "znc"},
	});
	ircd.ReadUntil("JOIN #znc");Z;
	EXPECT_THAT(HttpPost(request, {
		{"user", "user"},
		{"network", "test"},
		{"submitted", "1"},
		{"name", "znc"},
	})->readAll().toStdString(), HasSubstr("Channel [#znc] already exists"));
}

TEST_F(ZNCTest, FixFixOfCVE20149403) {
	auto znc = Run();Z;
	auto ircd = ConnectIRCd();Z;
	ircd.Write(":server 001 nick :Hello");
	ircd.Write(":nick JOIN @#znc");
	ircd.ReadUntil("MODE @#znc");Z;
	ircd.Write(":server 005 nick STATUSMSG=@ :supports");
	ircd.Write(":server PING :12345");
	ircd.ReadUntil("PONG 12345");Z;

	QNetworkRequest request;
	request.setRawHeader("Authorization", "Basic " + QByteArray("user:hunter2").toBase64());
	request.setUrl(QUrl("http://127.0.0.1:12345/mods/global/webadmin/addchan"));
	auto reply = HttpPost(request, {
		{"user", "user"},
		{"network", "test"},
		{"submitted", "1"},
		{"name", "@#znc"},
	});
	EXPECT_THAT(reply->readAll().toStdString(), HasSubstr("Could not add channel [@#znc]"));
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
	auto znc = Run();Z;
	znc->ShouldFinishItself(1);
}

TEST_F(ZNCTest, ShellModule) {
	auto znc = Run();Z;
	auto ircd = ConnectIRCd();Z;
	auto client = LoginClient();Z;
	client.Write("znc loadmod shell");
	client.Write("PRIVMSG *shell :echo blahblah");
	client.ReadUntil("PRIVMSG nick :blahblah");
	client.ReadUntil("PRIVMSG nick :znc$");
}

}  // namespace
