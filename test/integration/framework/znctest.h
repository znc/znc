/*
 * Copyright (C) 2004-2022 ZNC, see the NOTICE file for details.
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

#include "base.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace znc_inttest {

void WriteConfig(QString path);

class ZNCTest : public testing::Test {
  protected:
    void SetUp() override;

    Socket ConnectIRCd();
    Socket ConnectClient();
    Socket LoginClient();

    std::unique_ptr<Process> Run();

    std::unique_ptr<QNetworkReply> HttpGet(QNetworkRequest request);
    std::unique_ptr<QNetworkReply> HttpPost(
        QNetworkRequest request, QList<QPair<QString, QString>> data);
    std::unique_ptr<QNetworkReply> HandleHttp(QNetworkReply* reply);

    void InstallModule(QString name, QString content);

    App m_app;
    QNetworkAccessManager m_network;
    QTemporaryDir m_dir;
    QTcpServer m_server;
    std::list<QTcpSocket> m_clients;
};

}  // namespace znc_inttest
