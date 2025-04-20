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

#pragma once

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QProcess>
#include <QLocalSocket>
#include <QTcpSocket>

#include <memory>

namespace znc_inttest {

template <typename Device>
class IO {
  public:
    IO(Device* device, bool verbose = false)
        : m_device(device), m_verbose(verbose) {}
    virtual ~IO() {}
    void ReadUntil(QByteArray pattern);
    /*
     * Reads from Device until pattern is matched and returns this pattern
     * up to and excluding the first newline. Pattern itself can contain a newline.
     * Have to use second param as the ASSERT_*'s return a non-QByteArray.
     */
    void ReadUntilAndGet(QByteArray pattern, QByteArray& match);
    // Can be used to check that something was not sent. Slow.
    QByteArray ReadRemainder();
    void Write(QByteArray s = "", bool new_line = true);
    void Close();

  private:
    // Need to flush QTcpSocket, and QIODevice doesn't have flush at all...
    static void FlushIfCan(QIODevice*) {}
    static void FlushIfCan(QTcpSocket* sock) { sock->flush(); }
    static void FlushIfCan(QLocalSocket* sock) { sock->flush(); }

    Device* m_device;
    bool m_verbose;
    QByteArray m_readed;
};

template <typename Device>
IO<Device> WrapIO(Device* d) {
    return IO<Device>(d);
}

using Socket = IO<QLocalSocket>;

class Process : public IO<QProcess> {
  public:
    Process(QString cmd, QStringList args,
            std::function<void(QProcess*)> setup = [](QProcess*) {});
    ~Process() override;
    void ShouldFinishItself(int code = 0) {
        m_kill = false;
        m_exit = code;
    }
    void CanDie() { m_allowDie = true; }
    void ShouldFinishInSec(int sec) { m_finishTimeoutSec = sec; }

    // I can't do much about SWIG...
    void CanLeak() { m_allowLeak = true; }

  private:
    bool m_kill = true;
    int m_exit = 0;
    bool m_allowDie = false;
    bool m_allowLeak = false;
    QProcess m_proc;
    int m_finishTimeoutSec = 30;
};

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

// Implementation

template <typename Device>
void IO<Device>::ReadUntil(QByteArray pattern) {
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

template <typename Device>
void IO<Device>::ReadUntilAndGet(QByteArray pattern, QByteArray& match) {
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
              if (match.endsWith('\r')) {
                  match.chop(1);
              }
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

template <typename Device>
QByteArray IO<Device>::ReadRemainder() {
    auto deadline = QDateTime::currentDateTime().addSecs(2);
    while (QDateTime::currentDateTime() < deadline) {
        const int timeout_ms =QDateTime::currentDateTime().msecsTo(deadline);
        m_device->waitForReadyRead(std::max(1, timeout_ms));
        QByteArray chunk = m_device->readAll();
        if (m_verbose) {
            std::cout << chunk.toStdString() << std::flush;
        }
        m_readed += chunk;
    }
    QByteArray result = std::move(m_readed);
    m_readed.clear();
    return result;
}

template <typename Device>
void IO<Device>::Write(QByteArray s, bool new_line) {
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

inline void DisconnectFromServer(QTcpSocket* s) { s->disconnectFromHost(); }
inline void DisconnectFromServer(QLocalSocket* s) { s->disconnectFromServer(); }

template <typename Device>
void IO<Device>::Close() {
#ifdef __CYGWIN__
    // Qt on cygwin silently doesn't send the rest of buffer from socket
    // without this line
    sleep(1);
#endif
    DisconnectFromServer(m_device);
}

int PickPortNumber();

}  // namespace znc_inttest

