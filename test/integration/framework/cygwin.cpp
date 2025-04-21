// This file is LGPL, as it's based on Qt's qlocalsocket_unix.cpp
// The original header follows:

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "cygwin.h"

#include <QFile>
#include <QtNetwork/private/qlocalsocket_p.h>
#include <QtNetwork/private/qnet_unix_p.h>
#include <sys/un.h>

namespace znc_inttest_cygwin {
// https://stackoverflow.com/questions/424104/can-i-access-private-members-from-outside-the-class-without-using-friends
template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
struct A_member {
    using type = QScopedPointer<QObjectData> QObject::*;
    friend type get(A_member);
};
template struct Rob<A_member, &QObject::d_ptr>;

// This function is inspired by QLocalSocket::connectToServer() and QLocalSocketPrivate::_q_connectToSocket()
void CygwinWorkaroundLocalConnect(QLocalSocket& sock) {
    QObjectData* o = (sock.*get(A_member())).get();
    QLocalSocketPrivate* d = reinterpret_cast<QLocalSocketPrivate*>(o);
    d->unixSocket.setSocketState(QAbstractSocket::ConnectingState);
    d->state = QLocalSocket::ConnectingState;
    sock.stateChanged(d->state);

    if ((d->connectingSocket = qt_safe_socket(PF_UNIX, SOCK_STREAM, 0, 0)) ==
        -1) {
        sock.errorOccurred(QLocalSocket::UnsupportedSocketOperationError);
        return;
    }

    d->fullServerName = d->serverName;

    const QByteArray encodedConnectingPathName =
        QFile::encodeName(d->serverName);
    struct sockaddr_un name;
    name.sun_family = PF_UNIX;
    ::memcpy(name.sun_path, encodedConnectingPathName.constData(),
             encodedConnectingPathName.size() + 1);
    if (qt_safe_connect(d->connectingSocket, (struct sockaddr*)&name,
                        sizeof(name)) == -1) {
        sock.errorOccurred(QLocalSocket::UnknownSocketError);
        return;
    }

    ::fcntl(d->connectingSocket, F_SETFL,
            ::fcntl(d->connectingSocket, F_GETFL) | O_NONBLOCK);
    d->unixSocket.setSocketDescriptor(d->connectingSocket,
                                      QAbstractSocket::ConnectedState);
    sock.QIODevice::open(QLocalSocket::ReadWrite | QLocalSocket::Unbuffered);
    sock.connected();
    d->connectingSocket = -1;
    d->connectingName.clear();
}
}  // namespace znc_inttest_cygwin
