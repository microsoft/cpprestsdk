/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* oauth1_handler.cpp
*
* HTTP Library: Oauth 1.0 handler
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#include <openssl/hmac.h>
#include "cpprest/oauth1_handler.h"
#include "cpprest/asyncrt_utils.h"

using namespace web;
using namespace http;
using namespace client;
using namespace utility;

namespace web { namespace http { namespace client
{


static const int s_nonce_length(32);
static const utility::string_t s_nonce_chars("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");


std::vector<unsigned char> oauth1_handler::_hmac_sha1(const utility::string_t key, const utility::string_t data)
{
    // Generate digest with using libcrypto required by libssl.
    unsigned char digest[HMAC_MAX_MD_CBLOCK];
    unsigned int digest_len = 0;

    HMAC(EVP_sha1(), key.c_str(), static_cast<int>(key.length()),
            (const unsigned char*) data.c_str(), data.length(),
            digest, &digest_len);

    return std::vector<unsigned char>(digest, digest + digest_len);
}


utility::string_t oauth1_handler::_generate_nonce()
{
    std::uniform_int_distribution<> distr(0, static_cast<int>(s_nonce_chars.length() - 1));
    utility::string_t nonce;
    nonce.reserve(s_nonce_length);
    std::generate_n(std::back_inserter(nonce), s_nonce_length, [&]() { return s_nonce_chars[distr(m_random)]; } );
    return nonce;
}

utility::string_t oauth1_handler::_generate_timestamp()
{
// FIXME: this only works on Linux
    return std::to_string(utility::datetime::utc_timestamp() - 11644473600LL);
}

// Notes:
// - Doesn't support URIs without scheme or host.
// - If URI port is unspecified.
utility::string_t oauth1_handler::_build_base_string_uri(const uri& u)
{
    utility::ostringstream_t os;
    os << u.scheme() << "://" << u.host();
    if (!u.is_port_default() && u.port() != 80 && u.port() != 443)
    {
        os << ":" << u.port();
    }
    os << u.path();
// TODO: remove, trailing '?' is not added to the uri
//    if (!u.query().empty())
//    {
//        os << "?";
//    }
    return uri::encode_data_string(os.str());
}

utility::string_t oauth1_handler::_build_normalized_parameters(uri u,
        utility::string_t timestamp, utility::string_t nonce) const
{
    // While map sorts items by keys it doesn't take value into account.
    // We need to sort the query parameters separately.
    std::map<utility::string_t, utility::string_t> queries_map = http::uri::split_query(std::move(u).query());
    std::vector<utility::string_t> queries;
    for (auto query : queries_map)
    {
        queries.push_back(query.first + "=" + query.second);
    }

    // Push oauth1 parameters.
    queries.push_back("oauth_version=1.0");
    queries.push_back("oauth_consumer_key=" + m_config.m_key);
    queries.push_back("oauth_token=" + m_config.m_token);
    queries.push_back("oauth_signature_method=" + m_config.m_method);
    queries.push_back("oauth_timestamp=" + std::move(timestamp));
    queries.push_back("oauth_nonce=" + std::move(nonce));

    sort(queries.begin(), queries.end());

    utility::ostringstream_t os;
    std::copy(queries.begin(), queries.end() - 1, std::ostream_iterator<utility::string_t>(os, "&"));
    os << queries.back();

    return uri::encode_data_string(os.str());
}

// Builds signature base string according to:
// http://tools.ietf.org/html/rfc5849#section-3.4.1.1
utility::string_t oauth1_handler::_build_signature_base_string(http_request request,
        utility::string_t timestamp, utility::string_t nonce) const
{
    uri u(request.absolute_uri());
    utility::ostringstream_t os;
    os << request.method();
    os << "&" << _build_base_string_uri(u);
    os << "&" << _build_normalized_parameters(std::move(u), std::move(timestamp), std::move(nonce));
    return os.str();
}

utility::string_t oauth1_handler::_build_key() const
{
    return uri::encode_data_string(m_config.m_secret) + "&" + uri::encode_data_string(m_config.m_token_secret);
}

// Builds HMAC-SHA1 signature according to:
// http://tools.ietf.org/html/rfc5849#section-3.4.2
utility::string_t oauth1_handler::_build_hmac_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const
{
    auto text(_build_signature_base_string(std::move(request), std::move(timestamp), std::move(nonce)));
    auto digest(_hmac_sha1(_build_key(), text));
    auto signature(conversions::to_base64(digest));
    return signature;
}

// TODO: RSA-SHA1
/*
// Builds RSA-SHA1 signature according to:
// http://tools.ietf.org/html/rfc5849#section-3.4.3
utility::string_t oauth1_handler::_build_rsa_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const
{
    return "";
}
*/

// Builds PLAINTEXT signature according to:
// http://tools.ietf.org/html/rfc5849#section-3.4.4
utility::string_t oauth1_handler::_build_plaintext_signature() const
{
    return _build_key();
}

utility::string_t oauth1_handler::_build_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const
{
    if (oauth1_methods::hmac_sha1 == m_config.m_method)
    {
        return _build_hmac_sha1_signature(std::move(request), std::move(timestamp), std::move(nonce));
    }
// TODO: RSA-SHA1
//    else if (oauth1_methods::rsa_sha1 == m_config.m_method)
//    {
//        return _build_rsa_sha1_signature(std::move(request), std::move(timestamp), std::move(nonce));
//    }
    else if (oauth1_methods::plaintext == m_config.m_method)
    {
        return _build_plaintext_signature();
    }
// TODO: add assertion?
    return "";
}

pplx::task<http_response> oauth1_handler::propagate(http_request request)
{
    if (m_config.is_enabled())
    {
        utility::string_t timestamp(_generate_timestamp());
        utility::string_t nonce(_generate_nonce());

        utility::ostringstream_t os;
        os << "OAuth ";
        if (!m_config.m_realm.empty())
        {
            os << "realm=\"" << m_config.m_realm << "\", ";
        }
        os << "oauth_version=\"1.0\", oauth_consumer_key=\"" << m_config.m_key;
        os << "\", oauth_token=\"" << m_config.m_token;
        os << "\", oauth_signature_method=\"" << m_config.m_method;
        os << "\", oauth_timestamp=\"" << timestamp;
        os << "\", oauth_nonce=\"" << nonce;
        os << "\", oauth_signature=\"" << uri::encode_data_string(_build_signature(request, std::move(timestamp), std::move(nonce)));
        os << "\"";

        request.headers().add("Authorization", os.str());
    }
    return next_stage()->propagate(request);
}


#define _OAUTH1_METHODS
#define DAT(a,b) const oauth1_method oauth1_methods::a = b;
#include "cpprest/http_constants.dat"
#undef _OAUTH1_METHODS
#undef DAT


}}} // namespace web::http::client
