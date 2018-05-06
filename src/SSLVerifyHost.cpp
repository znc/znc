/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#include <znc/SSLVerifyHost.h>
#include <znc/Translation.h>
#include <arpa/inet.h>

#ifdef HAVE_LIBSSL
#if defined(OPENSSL_VERSION_NUMBER) && !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100007
# define CONST_ASN1_STRING_DATA const /* 1.1.0-pre7: openssl/openssl@17ebf85abda18c3875b1ba6670fe7b393bc1f297 */
#else
# define ASN1_STRING_get0_data( x ) ASN1_STRING_data( x )
# define CONST_ASN1_STRING_DATA
#endif

#include <openssl/x509v3.h>

namespace ZNC_Curl {
///////////////////////////////////////////////////////////////////////////
//
// This block is from https://github.com/bagder/curl/blob/master/lib/
// Copyright: Daniel Stenberg, <daniel@haxx.se>, license: MIT
//

/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2014, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

/* Portable, consistent toupper (remember EBCDIC). Do not use toupper() because
   its behavior is altered by the current locale. */
inline char Curl_raw_toupper(char in) {
    switch (in) {
        case 'a':
            return 'A';
        case 'b':
            return 'B';
        case 'c':
            return 'C';
        case 'd':
            return 'D';
        case 'e':
            return 'E';
        case 'f':
            return 'F';
        case 'g':
            return 'G';
        case 'h':
            return 'H';
        case 'i':
            return 'I';
        case 'j':
            return 'J';
        case 'k':
            return 'K';
        case 'l':
            return 'L';
        case 'm':
            return 'M';
        case 'n':
            return 'N';
        case 'o':
            return 'O';
        case 'p':
            return 'P';
        case 'q':
            return 'Q';
        case 'r':
            return 'R';
        case 's':
            return 'S';
        case 't':
            return 'T';
        case 'u':
            return 'U';
        case 'v':
            return 'V';
        case 'w':
            return 'W';
        case 'x':
            return 'X';
        case 'y':
            return 'Y';
        case 'z':
            return 'Z';
    }
    return in;
}

/*
* Curl_raw_equal() is for doing "raw" case insensitive strings. This is meant
* to be locale independent and only compare strings we know are safe for
* this. See http://daniel.haxx.se/blog/2008/10/15/strcasecmp-in-turkish/ for
* some further explanation to why this function is necessary.
*
* The function is capable of comparing a-z case insensitively even for
* non-ascii.
*/
static int Curl_raw_equal(const char* first, const char* second) {
    while (*first && *second) {
        if (Curl_raw_toupper(*first) != Curl_raw_toupper(*second))
            /* get out of the loop as soon as they don't match */
            break;
        first++;
        second++;
    }
    /* we do the comparison here (possibly again), just to make sure that if the
       loop above is skipped because one of the strings reached zero, we must
       not
       return this as a successful match */
    return (Curl_raw_toupper(*first) == Curl_raw_toupper(*second));
}
static int Curl_raw_nequal(const char* first, const char* second, size_t max) {
    while (*first && *second && max) {
        if (Curl_raw_toupper(*first) != Curl_raw_toupper(*second)) {
            break;
        }
        max--;
        first++;
        second++;
    }
    if (0 == max) return 1; /* they are equal this far */
    return Curl_raw_toupper(*first) == Curl_raw_toupper(*second);
}

static const int CURL_HOST_NOMATCH = 0;
static const int CURL_HOST_MATCH = 1;

/*
 * Match a hostname against a wildcard pattern.
 * E.g.
 *  "foo.host.com" matches "*.host.com".
 *
 * We use the matching rule described in RFC6125, section 6.4.3.
 * http://tools.ietf.org/html/rfc6125#section-6.4.3
 *
 * In addition: ignore trailing dots in the host names and wildcards, so that
 * the names are used normalized. This is what the browsers do.
 *
 * Do not allow wildcard matching on IP numbers. There are apparently
 * certificates being used with an IP address in the CN field, thus making no
 * apparent distinction between a name and an IP. We need to detect the use of
 * an IP address and not wildcard match on such names.
 *
 * NOTE: hostmatch() gets called with copied buffers so that it can modify the
 * contents at will.
 */

static int hostmatch(char* hostname, char* pattern) {
    const char* pattern_label_end, *pattern_wildcard, *hostname_label_end;
    int wildcard_enabled;
    size_t prefixlen, suffixlen;
    struct in_addr ignored;
#ifdef ENABLE_IPV6
    struct sockaddr_in6 si6;
#endif

    /* normalize pattern and hostname by stripping off trailing dots */
    size_t len = strlen(hostname);
    if (hostname[len - 1] == '.') hostname[len - 1] = 0;
    len = strlen(pattern);
    if (pattern[len - 1] == '.') pattern[len - 1] = 0;

    pattern_wildcard = strchr(pattern, '*');
    if (pattern_wildcard == nullptr)
        return Curl_raw_equal(pattern, hostname) ? CURL_HOST_MATCH
                                                 : CURL_HOST_NOMATCH;

    /* detect IP address as hostname and fail the match if so */
    if (inet_pton(AF_INET, hostname, &ignored) > 0) return CURL_HOST_NOMATCH;
#ifdef ENABLE_IPV6
    else if (Curl_inet_pton(AF_INET6, hostname, &si6.sin6_addr) > 0)
        return CURL_HOST_NOMATCH;
#endif

    /* We require at least 2 dots in pattern to avoid too wide wildcard
       match. */
    wildcard_enabled = 1;
    pattern_label_end = strchr(pattern, '.');
    if (pattern_label_end == nullptr ||
        strchr(pattern_label_end + 1, '.') == nullptr ||
        pattern_wildcard > pattern_label_end ||
        Curl_raw_nequal(pattern, "xn--", 4)) {
        wildcard_enabled = 0;
    }
    if (!wildcard_enabled)
        return Curl_raw_equal(pattern, hostname) ? CURL_HOST_MATCH
                                                 : CURL_HOST_NOMATCH;

    hostname_label_end = strchr(hostname, '.');
    if (hostname_label_end == nullptr ||
        !Curl_raw_equal(pattern_label_end, hostname_label_end))
        return CURL_HOST_NOMATCH;

    /* The wildcard must match at least one character, so the left-most
       label of the hostname is at least as large as the left-most label
       of the pattern. */
    if (hostname_label_end - hostname < pattern_label_end - pattern)
        return CURL_HOST_NOMATCH;

    prefixlen = pattern_wildcard - pattern;
    suffixlen = pattern_label_end - (pattern_wildcard + 1);
    return Curl_raw_nequal(pattern, hostname, prefixlen) &&
                   Curl_raw_nequal(pattern_wildcard + 1,
                                   hostname_label_end - suffixlen, suffixlen)
               ? CURL_HOST_MATCH
               : CURL_HOST_NOMATCH;
}

static int Curl_cert_hostcheck(const char* match_pattern,
                               const char* hostname) {
    char* matchp;
    char* hostp;
    int res = 0;
    if (!match_pattern || !*match_pattern || !hostname ||
        !*hostname) /* sanity check */
        ;
    else {
        matchp = strdup(match_pattern);
        if (matchp) {
            hostp = strdup(hostname);
            if (hostp) {
                if (hostmatch(hostp, matchp) == CURL_HOST_MATCH) res = 1;
                free(hostp);
            }
            free(matchp);
        }
    }

    return res;
}

//
// End of https://github.com/bagder/curl/blob/master/lib/
//
///////////////////////////////////////////////////////////////////////////
}  // namespace ZNC_Curl

namespace ZNC_iSECPartners {
///////////////////////////////////////////////////////////////////////////
//
// This block is from https://github.com/iSECPartners/ssl-conservatory/
// Copyright: Alban Diquet, license: MIT
//

/*
 * Helper functions to perform basic hostname validation using OpenSSL.
 *
 * Please read "everything-you-wanted-to-know-about-openssl.pdf" before
 * attempting to use this code. This whitepaper describes how the code works,
 * how it should be used, and what its limitations are.
 *
 * Author:  Alban Diquet
 * License: See LICENSE
 *
 */

typedef enum {
    MatchFound,
    MatchNotFound,
    NoSANPresent,
    MalformedCertificate,
    Error
} HostnameValidationResult;

#define HOSTNAME_MAX_SIZE 255

/**
* Tries to find a match for hostname in the certificate's Common Name field.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if the Common Name had a NUL character embedded in it.
* Returns Error if the Common Name could not be extracted.
*/
static HostnameValidationResult matches_common_name(const char* hostname,
                                                    const X509* server_cert) {
    int common_name_loc = -1;
    X509_NAME_ENTRY* common_name_entry = nullptr;
    ASN1_STRING* common_name_asn1 = nullptr;
    CONST_ASN1_STRING_DATA char* common_name_str = nullptr;

    // Find the position of the CN field in the Subject field of the certificate
    common_name_loc = X509_NAME_get_index_by_NID(
        X509_get_subject_name((X509*)server_cert), NID_commonName, -1);
    if (common_name_loc < 0) {
        return Error;
    }

    // Extract the CN field
    common_name_entry = X509_NAME_get_entry(
        X509_get_subject_name((X509*)server_cert), common_name_loc);
    if (common_name_entry == nullptr) {
        return Error;
    }

    // Convert the CN field to a C string
    common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
    if (common_name_asn1 == nullptr) {
        return Error;
    }
    common_name_str =
        (CONST_ASN1_STRING_DATA char*)ASN1_STRING_get0_data(common_name_asn1);

    // Make sure there isn't an embedded NUL character in the CN
    if (ASN1_STRING_length(common_name_asn1) !=
        static_cast<int>(strlen(common_name_str))) {
        return MalformedCertificate;
    }

    DEBUG("SSLVerifyHost: Found CN " << common_name_str);
    // Compare expected hostname with the CN
    if (ZNC_Curl::Curl_cert_hostcheck(common_name_str, hostname)) {
        return MatchFound;
    } else {
        return MatchNotFound;
    }
}

/**
* Tries to find a match for hostname in the certificate's Subject Alternative Name extension.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
* Returns NoSANPresent if the SAN extension was not present in the certificate.
*/
static HostnameValidationResult matches_subject_alternative_name(
    const char* hostname, const X509* server_cert) {
    HostnameValidationResult result = MatchNotFound;
    int i;
    int san_names_nb = -1;
    STACK_OF(GENERAL_NAME)* san_names = nullptr;

    // Try to extract the names within the SAN extension from the certificate
    san_names = reinterpret_cast<STACK_OF(GENERAL_NAME)*>(X509_get_ext_d2i(
        (X509*)server_cert, NID_subject_alt_name, nullptr, nullptr));
    if (san_names == nullptr) {
        return NoSANPresent;
    }
    san_names_nb = sk_GENERAL_NAME_num(san_names);

    // Precompute binary representation of hostname in case if it's IP address.
    // Not the other way around, because there can be multiple text
    // representation of the same IP.
    char ip4[4] = {};
    char ip6[16] = {};
    const int ip4try = inet_pton(AF_INET, hostname, ip4);
    const int ip6try = inet_pton(AF_INET6, hostname, ip6);

    // Check each name within the extension
    for (i = 0; i < san_names_nb; i++) {
        const GENERAL_NAME* current_name = sk_GENERAL_NAME_value(san_names, i);

        if (current_name->type == GEN_DNS) {
            // Current name is a DNS name, let's check it
            CONST_ASN1_STRING_DATA char* dns_name =
                (CONST_ASN1_STRING_DATA char*)ASN1_STRING_get0_data(
                    current_name->d.dNSName);

            // Make sure there isn't an embedded NUL character in the DNS name
            if (ASN1_STRING_length(current_name->d.dNSName) !=
                static_cast<int>(strnlen(
                    dns_name, ASN1_STRING_length(current_name->d.dNSName)))) {
                DEBUG("SSLVerifyHost: embedded null in DNS SAN");
                result = MalformedCertificate;
                break;
            } else {  // Compare expected hostname with the DNS name
                DEBUG("SSLVerifyHost: Found DNS SAN " << dns_name);
                if (ZNC_Curl::Curl_cert_hostcheck(dns_name, hostname)) {
                    result = MatchFound;
                    break;
                }
            }
        } else if (current_name->type == GEN_IPADD) {
            CString ip(reinterpret_cast<const char*>(
                           ASN1_STRING_get0_data(current_name->d.iPAddress)),
                       ASN1_STRING_length(current_name->d.iPAddress));
            DEBUG("SSLVerifyHost: Found IP SAN "
                  << ip.Escape_n(CString::EHEXCOLON));
            if (ip4try && ASN1_STRING_length(current_name->d.iPAddress) == 4) {
                if (memcmp(ip4,
                           ASN1_STRING_get0_data(current_name->d.iPAddress),
                           4) == 0) {
                    result = MatchFound;
                    break;
                }
            } else if (ip6try &&
                       ASN1_STRING_length(current_name->d.iPAddress) == 16) {
                if (memcmp(ip6,
                           ASN1_STRING_get0_data(current_name->d.iPAddress),
                           16) == 0) {
                    result = MatchFound;
                    break;
                }
            }
        }
    }
    sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

    return result;
}

/**
* Validates the server's identity by looking for the expected hostname in the
* server's certificate. As described in RFC 6125, it first tries to find a match
* in the Subject Alternative Name extension. If the extension is not present in
* the certificate, it checks the Common Name instead.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
* Returns Error if there was an error.
*/
static HostnameValidationResult validate_hostname(const char* hostname,
                                                  const X509* server_cert) {
    HostnameValidationResult result;

    if ((hostname == nullptr) || (server_cert == nullptr)) return Error;

    // First try the Subject Alternative Names extension
    result = matches_subject_alternative_name(hostname, server_cert);
    if (result == NoSANPresent) {
        // Extension was not found: try the Common Name
        result = matches_common_name(hostname, server_cert);
    }

    return result;
}

//
// End of https://github.com/iSECPartners/ssl-conservatory/
//
///////////////////////////////////////////////////////////////////////////
}  // namespace ZNC_iSECPartners

bool ZNC_SSLVerifyHost(const CString& sHost, const X509* pCert,
                       CString& sError) {
    struct Tr : CCoreTranslationMixin {
        using CCoreTranslationMixin::t_s;
    };
    DEBUG("SSLVerifyHost: checking " << sHost);
    ZNC_iSECPartners::HostnameValidationResult eResult =
        ZNC_iSECPartners::validate_hostname(sHost.c_str(), pCert);
    switch (eResult) {
        case ZNC_iSECPartners::MatchFound:
            DEBUG("SSLVerifyHost: verified");
            return true;
        case ZNC_iSECPartners::MatchNotFound:
            DEBUG("SSLVerifyHost: host doesn't match");
            sError = Tr::t_s("hostname doesn't match");
            return false;
        case ZNC_iSECPartners::MalformedCertificate:
            DEBUG("SSLVerifyHost: malformed cert");
            sError = Tr::t_s("malformed hostname in certificate");
            return false;
        default:
            DEBUG("SSLVerifyHost: error");
            sError = Tr::t_s("hostname verification error");
            return false;
    }
}

#endif /* HAVE_LIBSSL */
