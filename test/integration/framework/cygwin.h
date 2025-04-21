#pragma once

#include <QLocalSocket>

namespace znc_inttest_cygwin {
// Qt uses non-blocking sockets for unix sockets, but cygwin emulates them via
// AF_INET sockets, so connect() fails. This function connects the socket by
// reaching into private parts of QLocalSocket, and sets it non-blocking only
// after connect().
void CygwinWorkaroundLocalConnect(QLocalSocket& sock);
}
