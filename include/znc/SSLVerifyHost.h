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

#ifndef ZNC_SSLVERIFYHOST_H
#define ZNC_SSLVERIFYHOST_H

#ifdef HAVE_LIBSSL

#include <znc/Csocket.h>
#include <znc/ZNCString.h>

bool ZNC_SSLVerifyHost(const CString& sHost, const X509* pCert, CString& sError);

#endif /* HAVE_LIBSSL */

#endif /* ZNC_SSLVERIFYHOST_H */
