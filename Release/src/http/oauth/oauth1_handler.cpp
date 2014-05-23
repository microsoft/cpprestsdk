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
#include "cpprest/oauth1_handler.h"
#include "cpprest/asyncrt_utils.h"

using namespace web;
using namespace http;
using namespace client;
using namespace utility;

namespace web { namespace http { namespace client { namespace experimental
{


//
// Start of platform-dependent _hmac_sha1() block...
//
#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt) // Windows desktop


#include <winternl.h>
#include <bcrypt.h>


std::vector<unsigned char> oauth1_handler::_hmac_sha1(const utility::string_t key, const utility::string_t data)
{
    NTSTATUS status;
    BCRYPT_ALG_HANDLE alg_handle = nullptr;
    BCRYPT_HASH_HANDLE hash_handle = nullptr;

    std::vector<unsigned char> hash;
    DWORD hash_len = 0;
    ULONG result_len = 0;

    auto key_c = conversions::utf16_to_utf8(std::move(key));
    auto data_c = conversions::utf16_to_utf8(std::move(data));

    status = BCryptOpenAlgorithmProvider(&alg_handle, BCRYPT_SHA1_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(status))
    {
        goto cleanup;
    }
    status = BCryptGetProperty(alg_handle, BCRYPT_HASH_LENGTH, (PBYTE) &hash_len, sizeof(hash_len), &result_len, 0);
    if (!NT_SUCCESS(status))
    {
        goto cleanup;
    }
    hash.resize(hash_len);

    status = BCryptCreateHash(alg_handle, &hash_handle, nullptr, 0, (PBYTE) key_c.c_str(), (ULONG) key_c.length(), 0);
    if (!NT_SUCCESS(status))
    {
        goto cleanup;
    }
    status = BCryptHashData(hash_handle, (PBYTE) data_c.c_str(), (ULONG) data_c.length(), 0);
    if (!NT_SUCCESS(status))
    {
        goto cleanup;
    }
    status = BCryptFinishHash(hash_handle, hash.data(), hash_len, 0);
    if (!NT_SUCCESS(status))
    {
        goto cleanup;
    }

    return hash;

cleanup:

    // TODO: Throw respective error if crypto cannot be used.
    if (hash_handle)
    {
        BCryptDestroyHash(hash_handle);
    }
    if (alg_handle)
    {
        BCryptCloseAlgorithmProvider(alg_handle, 0);
    }
    return hash;
}


#elif defined(_MS_WINDOWS) && defined(__cplusplus_winrt) // Windows RT


using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::Core;
using namespace Windows::Storage::Streams;
 

std::vector<unsigned char> oauth1_handler::_hmac_sha1(const utility::string_t key, const utility::string_t data)
{
    Platform::String^ data_str = ref new Platform::String(data.c_str());
    Platform::String^ key_str = ref new Platform::String(key.c_str());

    MacAlgorithmProvider^ HMACSha1Provider = MacAlgorithmProvider::OpenAlgorithm(MacAlgorithmNames::HmacSha1);
    IBuffer^ content_buffer = CryptographicBuffer::ConvertStringToBinary(data_str, BinaryStringEncoding::Utf8);
    IBuffer^ key_buffer = CryptographicBuffer::ConvertStringToBinary(key_str, BinaryStringEncoding::Utf8);

    auto signature_key = HMACSha1Provider->CreateKey(key_buffer);
    auto signed_buffer = CryptographicEngine::Sign(signature_key, content_buffer);

#if 1
    Platform::Array<unsigned char, 1>^ arr;
    CryptographicBuffer::CopyToByteArray(signed_buffer, &arr);
    return std::vector<unsigned char>(arr->Data, arr->Data + arr->Length);
#else
    // Alternative.
    auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(signed_buffer);
    std::vector<unsigned char> digest(reader->UnconsumedBufferLength);
    if (!digest.empty())
    {
        reader->ReadBytes(::Platform::ArrayReference<unsigned char>(&digest[0], static_cast<unsigned int>(digest.size())));
    }
    return digest;
#endif
}


#else // Linux, Mac OS X


#include <openssl/hmac.h>


std::vector<unsigned char> oauth1_handler::_hmac_sha1(const utility::string_t key, const utility::string_t data)
{
    unsigned char digest[HMAC_MAX_MD_CBLOCK];
    unsigned int digest_len = 0;

    HMAC(EVP_sha1(), key.c_str(), static_cast<int>(key.length()),
            (const unsigned char*) data.c_str(), data.length(),
            digest, &digest_len);

    return std::vector<unsigned char>(digest, digest + digest_len);
}


#endif
//
// ...End of platform-dependent _hmac_sha1() block.
//


utility::string_t oauth1_handler::_generate_timestamp()
{
// FIXME: this only works on Linux
    utility::ostringstream_t os;
    os << (utility::datetime::utc_timestamp() - 11644473600LL);
    return os.str();
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
        utility::ostringstream_t os;
        os << query.first << "=" << query.second;
        queries.push_back(os.str());
    }

    // Push oauth1 parameters.
    queries.push_back(_XPLATSTR("oauth_version=1.0"));
    queries.push_back(_XPLATSTR("oauth_consumer_key=" + m_config.key()));
    queries.push_back(_XPLATSTR("oauth_token=" + m_config.token()));
    queries.push_back(_XPLATSTR("oauth_signature_method=" + m_config.method()));
    queries.push_back(_XPLATSTR("oauth_timestamp=" + std::move(timestamp)));
    queries.push_back(_XPLATSTR("oauth_nonce=" + std::move(nonce)));

    sort(queries.begin(), queries.end());

    utility::ostringstream_t os;
    for (auto i = queries.begin(); i != queries.end() - 1; ++i)
    {
        os << *i << _XPLATSTR("&");
    }
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
    return uri::encode_data_string(m_config.secret()) + _XPLATSTR("&") + uri::encode_data_string(m_config.token_secret());
}

// Builds HMAC-SHA1 signature according to:
// http://tools.ietf.org/html/rfc5849#section-3.4.2
utility::string_t oauth1_handler::_build_hmac_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const
{
    auto text(_build_signature_base_string(std::move(request), std::move(timestamp), std::move(nonce)));
    auto digest(_hmac_sha1(_build_key(), std::move(text)));
    auto signature(conversions::to_base64(std::move(digest)));
    return signature;
}

// TODO: RSA-SHA1
/*
// Builds RSA-SHA1 signature according to:
// http://tools.ietf.org/html/rfc5849#section-3.4.3
utility::string_t oauth1_handler::_build_rsa_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const
{
    return U("");
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
    if (oauth1_methods::hmac_sha1 == m_config.method())
    {
        return _build_hmac_sha1_signature(std::move(request), std::move(timestamp), std::move(nonce));
    }
// TODO: RSA-SHA1
//    else if (oauth1_methods::rsa_sha1 == m_config.m_method)
//    {
//        return _build_rsa_sha1_signature(std::move(request), std::move(timestamp), std::move(nonce));
//    }
    else if (oauth1_methods::plaintext == m_config.method())
    {
        return _build_plaintext_signature();
    }
// TODO: add assertion?
    return _XPLATSTR("");
}

pplx::task<http_response> oauth1_handler::propagate(http_request request)
{
    if (m_config.is_enabled())
    {
        utility::string_t timestamp(_generate_timestamp());
        utility::string_t nonce(m_nonce_generator.generate());

        utility::ostringstream_t os;
        os << "OAuth ";
        if (!m_config.realm().empty())
        {
            os << "realm=\"" << m_config.realm() << "\", ";
        }
        os << "oauth_version=\"1.0\", oauth_consumer_key=\"" << m_config.key();
        os << "\", oauth_token=\"" << m_config.token();
        os << "\", oauth_signature_method=\"" << m_config.method();
        os << "\", oauth_timestamp=\"" << timestamp;
        os << "\", oauth_nonce=\"" << nonce;
        os << "\", oauth_signature=\"" << uri::encode_data_string(_build_signature(request, std::move(timestamp), std::move(nonce)));
        os << "\"";

        request.headers().add(_XPLATSTR("Authorization"), os.str());
    }
    return next_stage()->propagate(request);
}


#define _OAUTH1_METHODS
#define DAT(a,b) const oauth1_method oauth1_methods::a = b;
#include "cpprest/http_constants.dat"
#undef _OAUTH1_METHODS
#undef DAT


}}}} // namespace web::http::client::experimental
