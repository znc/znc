/*
 * Copyright (C) 2004-2020 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_LISTENER_H
#define ZNC_LISTENER_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>

// Forward Declarations
class CRealListener;
class CConfig;
// !Forward Declarations

class CListener {
  public:
    typedef enum { ACCEPT_IRC, ACCEPT_HTTP, ACCEPT_ALL } EAcceptType;

    CListener(const CString& sURIPrefix, bool bSSL, EAcceptType eAccept)
        : m_bSSL(bSSL),
          m_sURIPrefix(sURIPrefix),
          m_pListener(nullptr),
          m_eAcceptType(eAccept) {}

    virtual ~CListener();

    CListener(const CListener&) = delete;
    CListener& operator=(const CListener&) = delete;

    // Getters
    bool IsSSL() const { return m_bSSL; }
    CRealListener* GetRealListener() const { return m_pListener; }
    const CString& GetURIPrefix() const { return m_sURIPrefix; }
    EAcceptType GetAcceptType() const { return m_eAcceptType; }
    // !Getters

    // It doesn't make sense to change any of the settings after Listen()
    // except this one, so don't add other setters!
    void SetAcceptType(EAcceptType eType) { m_eAcceptType = eType; }

    virtual bool Listen() = 0;
    void ResetRealListener();
    virtual CConfig ToConfig() const;

  private:
  protected:
    void setupSSL(CRealListener* listener) const;

    bool m_bSSL;
    CString m_sURIPrefix;
    CRealListener* m_pListener;
    EAcceptType m_eAcceptType;
};

class CTCPListener : public CListener {
  public:
    CTCPListener(unsigned short uPort, const CString& sBindHost,
                 const CString& sURIPrefix, bool bSSL, EAddrType eAddr,
                 EAcceptType eAccept)
        : CListener(sURIPrefix, bSSL, eAccept),
          m_eAddr(eAddr),
          m_uPort(uPort),
          m_sBindHost(sBindHost) {}
    ~CTCPListener();

    CTCPListener(const CTCPListener&) = delete;
    CTCPListener& operator=(const CTCPListener&) = delete;

    // Getters
    EAddrType GetAddrType() const { return m_eAddr; }
    unsigned short GetPort() const { return m_uPort; }
    const CString& GetBindHost() const { return m_sBindHost; }
    // !Getters

    bool Listen() override;
    CConfig ToConfig() const override;

  protected:
    EAddrType m_eAddr;
    unsigned short m_uPort;
    CString m_sBindHost;
};

class CUnixListener : public CListener {
  public:
    CUnixListener(const CString& sPath, const CString& sURIPrefix, bool bSSL,
                 EAcceptType eAccept)
        : CListener(sURIPrefix, bSSL, eAccept),
          m_sPath(sPath) {}
    ~CUnixListener();

    CUnixListener(const CUnixListener&) = delete;
    CUnixListener& operator=(const CUnixListener&) = delete;

    // Getters
    const CString& GetPath() const { return m_sPath; }
    // !Getters

    bool Listen() override;
    CConfig ToConfig() const override;

  protected:
    CString m_sPath;
};

class CRealListener : public CZNCSock {
  public:
    CRealListener(CListener& listener) : CZNCSock(), m_Listener(listener) {}
    virtual ~CRealListener();

    bool ConnectionFrom(const CString& sHost, unsigned short uPort) override;
    Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
    void SockError(int iErrno, const CString& sDescription) override;

  private:
    CListener& m_Listener;
};

class CIncomingConnection : public CZNCSock {
  public:
    CIncomingConnection(const CString& sHostname, unsigned short uPort,
                        CListener::EAcceptType eAcceptType,
                        const CString& sURIPrefix);
    virtual ~CIncomingConnection() {}
    void ReadLine(const CString& sData) override;
    void ReachedMaxBuffer() override;

  private:
    CListener::EAcceptType m_eAcceptType;
    const CString m_sURIPrefix;
};

#endif  // !ZNC_LISTENER_H
