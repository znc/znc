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

#include <QDateTime>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>

#define Z if (::testing::Test::HasFatalFailure()) return

namespace {

class IO {
public:
	IO(QIODevice* device, bool verbose = false) : m_device(device), m_verbose(verbose) {}
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
			ASSERT_GT(timeout_ms, 0) << pattern.toStdString();
			ASSERT_TRUE(m_device->waitForReadyRead(timeout_ms)) << pattern.toStdString();
			QByteArray chunk = m_device->readAll();
			if (m_verbose) {
				std::cout << chunk.toStdString() << std::flush;
			}
			m_readed += chunk;
		}
	}
	void Write(QString s = "") {
		s += "\n";
		if (m_verbose) {
			std::cout << s.toStdString() << std::flush;
		}
		QTextStream str(m_device);
		str << s;
	}

private:
	QIODevice* m_device;
	bool m_verbose;
	QByteArray m_readed;
};

class Process : public IO {
public:
	Process(QString cmd, QStringList args, bool interactive) : IO(&m_proc, true) {
		if (!interactive) {
			m_proc.setProcessChannelMode(QProcess::ForwardedChannels);
		}
		m_proc.start(cmd, args);
	}
	~Process() {
		if (m_kill) m_proc.terminate();
		EXPECT_TRUE(m_proc.waitForFinished());
		m_proc.kill();  // for case if it didn't finish yet
	}
	void ShouldFinishItself() {
		m_kill = false;
	}

private:
	bool m_kill = true;
	QProcess m_proc;
};

void WriteConfig(QString path) {
	Process p("./znc", QStringList() << "--debug" << "--datadir" << path << "--makeconf", true);
	p.ReadUntil("Listen on port");Z;          p.Write("12345");
	p.ReadUntil("Listen using SSL");Z;        p.Write();
	p.ReadUntil("IPv6");Z;                    p.Write();
	p.ReadUntil("Username");Z;                p.Write("user");
	p.ReadUntil("password");Z;                p.Write("hunter2");
	p.ReadUntil("Confirm");Z;                 p.Write("hunter2");
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

TEST(ZNC, Connect) {
	QTemporaryDir dir;
	WriteConfig(dir.path());Z;
	QTcpServer ser;
	ASSERT_TRUE(ser.listen(QHostAddress::LocalHost, 6667)) << ser.errorString().toStdString();
	Process znc("./znc", QStringList() << "--debug" << "--datadir" << dir.path(), false);
	ASSERT_TRUE(ser.waitForNewConnection(30000 /* msec */));
	IO ircd(ser.nextPendingConnection());
	ircd.ReadUntil("CAP LS");Z;
	QTcpSocket cli;
	cli.connectToHost("127.0.0.1", 12345);
	ASSERT_TRUE(cli.waitForConnected()) << cli.errorString().toStdString();
	IO cl(&cli);
	cl.Write("PASS :hunter2");
	cl.Write("NICK nick");
	cl.Write("USER user/test x x :x");
	cl.ReadUntil("Welcome");Z;
}

}  // namespace
