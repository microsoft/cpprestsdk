/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: Oauth 1.0
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if !defined(CPPREST_TARGET_XP)

using namespace utility;
using web::http::client::http_client;
using web::http::client::http_client_config;
using web::http::oauth1::details::oauth1_state;
using web::http::oauth1::details::oauth1_strings;

namespace web { namespace http { namespace oauth1
{

namespace details
{

#define _OAUTH1_STRINGS
#define DAT(a_, b_) const oauth1_string oauth1_strings::a_(_XPLATSTR(b_));
#include "cpprest/details/http_constants.dat"
#undef _OAUTH1_STRINGS
#undef DAT

} // namespace web::http::oauth1::details

namespace experimental
{

//
// Start of platform-dependent _hmac_sha1() block...
//
#if defined(_WIN32) && !defined(__cplusplus_winrt) // Windows desktop

#include <winternl.h>
#include <bcrypt.h>

// Code analysis complains even though there is no bug.
#pragma warning(push)
#pragma warning(disable : 6102)
std::vector<unsigned char> oauth1_config::_hmac_sha1(const utility::string_t& key, const utility::string_t& data)
{
    NTSTATUS status;
    BCRYPT_ALG_HANDLE alg_handle = nullptr;
    BCRYPT_HASH_HANDLE hash_handle = nullptr;

    std::vector<unsigned char> hash;
    DWORD hash_len = 0;
    ULONG result_len = 0;

    const auto &key_c = conversions::utf16_to_utf8(key);
    const auto &data_c = conversions::utf16_to_utf8(data);

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

cleanup:
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
#pragma warning(pop)

#elif defined(_WIN32) && defined(__cplusplus_winrt) // Windows RT

using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::Core;
using namespace Windows::Storage::Streams;

std::vector<unsigned char> oauth1_config::_hmac_sha1(const utility::string_t& key, const utility::string_t& data)
{
    Platform::String^ data_str = ref new Platform::String(data.c_str());
    Platform::String^ key_str = ref new Platform::String(key.c_str());

    MacAlgorithmProvider^ HMACSha1Provider = MacAlgorithmProvider::OpenAlgorithm(MacAlgorithmNames::HmacSha1);
    IBuffer^ content_buffer = CryptographicBuffer::ConvertStringToBinary(data_str, BinaryStringEncoding::Utf8);
    IBuffer^ key_buffer = CryptographicBuffer::ConvertStringToBinary(key_str, BinaryStringEncoding::Utf8);

    auto signature_key = HMACSha1Provider->CreateKey(key_buffer);
    auto signed_buffer = CryptographicEngine::Sign(signature_key, content_buffer);

    Platform::Array<unsigned char, 1>^ arr;
    CryptographicBuffer::CopyToByteArray(signed_buffer, &arr);
    return std::vector<unsigned char>(arr->Data, arr->Data + arr->Length);
}

#else // Linux, Mac OS X

#include <openssl/hmac.h>

std::vector<unsigned char> oauth1_config::_hmac_sha1(const utility::string_t& key, const utility::string_t& data)
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

// Notes:
// - Doesn't support URIs without scheme or host.
// - If URI port is unspecified.
utility::string_t oauth1_config::_build_base_string_uri(const uri& u)
{
    utility::ostringstream_t os;
    os.imbue(std::locale::classic());
    os << u.scheme() << "://" << u.host();
    if (!u.is_port_default() && u.port() != 80 && u.port() != 443)
    {
        os << ":" << u.port();
    }
    os << u.path();
    return uri::encode_data_string(os.str());
}

utility::string_t oauth1_config::_build_normalized_parameters(web::http::uri u, const oauth1_state& state) const
{
    // While map sorts items by keys it doesn't take value into account.
    // We need to sort the query parameters separately.
    std::map<utility::string_t, utility::string_t> queries_map = http::uri::split_query(std::move(u).query());
    std::vector<utility::string_t> queries;
    for (const auto& query : queries_map)
    {
        utility::ostringstream_t os;
        os.imbue(std::locale::classic());
        os << query.first << "=" << query.second;
        queries.push_back(os.str());
    }

    for (const auto& query : parameters())
    {
        utility::ostringstream_t os;
        os.imbue(std::locale::classic());
        os << query.first << "=" << query.second;
        queries.push_back(os.str());
    }

    // Push oauth1 parameters.
    queries.push_back(oauth1_strings::version + U("=1.0"));
    queries.push_back(oauth1_strings::consumer_key + U("=") + web::uri::encode_data_string(consumer_key()));
    if (!m_token.access_token().empty())
    {
        queries.push_back(oauth1_strings::token + U("=") + web::uri::encode_data_string(m_token.access_token()));
    }
    queries.push_back(oauth1_strings::signature_method + U("=") + method());
    queries.push_back(oauth1_strings::timestamp + U("=") + state.timestamp());
    queries.push_back(oauth1_strings::nonce + U("=") + state.nonce());
    if (!state.extra_key().empty())
    {
        queries.push_back(state.extra_key() + U("=") + web::uri::encode_data_string(state.extra_value()));
    }

    // Sort parameters and build the string.
    sort(queries.begin(), queries.end());
    utility::ostringstream_t os;
    os.imbue(std::locale::classic());
    for (auto i = queries.begin(); i != queries.end() - 1; ++i)
    {
        os << *i << U("&");
    }
    os << queries.back();
    return uri::encode_data_string(os.str());
}

static bool is_application_x_www_form_urlencoded (http_request &request)
{
    const auto content_type(request.headers()[header_names::content_type]);
    return 0 == content_type.find(web::http::details::mime_types::application_x_www_form_urlencoded);
}

utility::string_t oauth1_config::_build_signature_base_string(http_request request, oauth1_state state) const
{
    uri u(request.absolute_uri());
    utility::ostringstream_t os;
    os.imbue(std::locale::classic());
    os << request.method();
    os << "&" << _build_base_string_uri(u);

	// http://oauth.net/core/1.0a/#signing_process
	// 9.1.1.  Normalize Request Parameters
	// The request parameters are collected, sorted and concatenated into a normalized string:
	//	- Parameters in the OAuth HTTP Authorization header excluding the realm parameter.
	//	- Parameters in the HTTP POST request body (with a content-type of application/x-www-form-urlencoded).
    //	- HTTP GET parameters added to the URLs in the query part (as defined by [RFC3986] section 3).
    if (is_application_x_www_form_urlencoded(request))
    {
        // Note: this should be improved to not block and handle any potential exceptions.
        utility::string_t str = request.extract_string(true).get();
        request.set_body(str, web::http::details::mime_types::application_x_www_form_urlencoded);
        uri v = http::uri_builder(request.absolute_uri()).append_query(std::move(str), false).to_uri();
        os << "&" << _build_normalized_parameters(std::move(v), std::move(state));
    }
    else
    {
        os << "&" << _build_normalized_parameters(std::move(u), std::move(state));
    }
    return os.str();
}

utility::string_t oauth1_config::_build_signature(http_request request, oauth1_state state) const
{
    if (oauth1_methods::hmac_sha1 == method())
    {
        return _build_hmac_sha1_signature(std::move(request), std::move(state));
    }
    else if (oauth1_methods::plaintext == method())
    {
        return _build_plaintext_signature();
    }
    throw oauth1_exception(U("invalid signature method.")); // Should never happen.
}

pplx::task<void> oauth1_config::_request_token(oauth1_state state, bool is_temp_token_request)
{
    utility::string_t endpoint = is_temp_token_request ? temp_endpoint() : token_endpoint();
    http_request req;
    req.set_method(methods::POST);
    req.set_request_uri(utility::string_t());
    req._set_base_uri(endpoint);

    _authenticate_request(req, std::move(state));

    // configure proxy
    http_client_config config;
    config.set_proxy(m_proxy);

    http_client client(endpoint, config);

    return client.request(req)
    .then([](http_response resp)
    {
        return resp.extract_string();
    })
    .then([this, is_temp_token_request](utility::string_t body) -> void
    {
        auto query(uri::split_query(body));

        if (is_temp_token_request)
        {
            auto callback_confirmed_param = query.find(oauth1_strings::callback_confirmed);
            if (callback_confirmed_param == query.end())
            {
                throw oauth1_exception(U("parameter 'oauth_callback_confirmed' is missing from response: ") + body
                    + U(". the service may be using obsoleted and insecure OAuth Core 1.0 protocol."));
            }
        }

        auto token_param = query.find(oauth1_strings::token);
        if (token_param == query.end())
        {
            throw oauth1_exception(U("parameter 'oauth_token' missing from response: ") + body);
        }

        auto token_secret_param = query.find(oauth1_strings::token_secret);
        if (token_secret_param == query.end())
        {
            throw oauth1_exception(U("parameter 'oauth_token_secret' missing from response: ") + body);
        }

        // Here the token can be either temporary or access token.
        // The authorization is complete if it is access token.
        m_is_authorization_completed = !is_temp_token_request;
        m_token = oauth1_token(web::uri::decode(token_param->second), web::uri::decode(token_secret_param->second));

		for (const auto& qa : query)
        {
            if (qa.first == oauth1_strings::token || qa.first == oauth1_strings::token_secret) continue ;
            m_token.set_additional_parameter(web::uri::decode(qa.first), web::uri::decode(qa.second));
        }
    });
}

void oauth1_config::_authenticate_request(http_request &request, oauth1_state state)
{
    utility::ostringstream_t os;
    os.imbue(std::locale::classic());
    os << "OAuth ";
    if (!realm().empty())
    {
        os << oauth1_strings::realm << "=\"" << web::uri::encode_data_string (realm()) << "\", ";
    }
    os << oauth1_strings::version << "=\"1.0";
    os << "\", " << oauth1_strings::consumer_key << "=\"" << web::uri::encode_data_string (consumer_key());
    if (!m_token.access_token().empty())
    {
        os << "\", " << oauth1_strings::token << "=\"" << web::uri::encode_data_string(m_token.access_token());
    }
    os << "\", " << oauth1_strings::signature_method << "=\"" << method();
    os << "\", " << oauth1_strings::timestamp << "=\"" << state.timestamp();
    os << "\", " << oauth1_strings::nonce << "=\"" << state.nonce();
    os << "\", " << oauth1_strings::signature << "=\"" << uri::encode_data_string(_build_signature(request, state));
    os << "\"";

    if (!state.extra_key().empty())
    {
        os << ", " << state.extra_key() << "=\"" << web::uri::encode_data_string(state.extra_value()) << "\"";
    }

    request.headers().add(header_names::authorization, os.str());
}

pplx::task<utility::string_t> oauth1_config::build_authorization_uri()
{
    pplx::task<void> temp_token_req = _request_token(_generate_auth_state(oauth1_strings::callback, callback_uri()), true);

    return temp_token_req.then([this]
    {
        uri_builder ub(auth_endpoint());
        ub.append_query(oauth1_strings::token, m_token.access_token());
        return ub.to_string();
    });
}

pplx::task<void> oauth1_config::token_from_redirected_uri(const web::http::uri& redirected_uri)
{
    auto query = uri::split_query(redirected_uri.query());

    auto token_param = query.find(oauth1_strings::token);
    if (token_param == query.end())
    {
        return pplx::task_from_exception<void>(oauth1_exception(U("parameter 'oauth_token' missing from redirected URI.")));
    }
    if (m_token.access_token() != token_param->second)
    {
        utility::ostringstream_t err;
        err.imbue(std::locale::classic());
        err << U("redirected URI parameter 'oauth_token'='") << token_param->second
            << U("' does not match temporary token='") << m_token.access_token() << U("'.");
        return pplx::task_from_exception<void>(oauth1_exception(err.str().c_str()));
    }

    auto verifier_param = query.find(oauth1_strings::verifier);
    if (verifier_param == query.end())
    {
        return pplx::task_from_exception<void>(oauth1_exception(U("parameter 'oauth_verifier' missing from redirected URI.")));
    }

    return token_from_verifier(verifier_param->second);
}

// Remove once VS 2013 is no longer supported.
#if defined(_WIN32) && _MSC_VER < 1900
static const oauth1_token empty_token;
#endif
const oauth1_token& oauth1_config::token() const
{
    if (m_is_authorization_completed)
    {
        // Return the token object only if authorization has been completed.
        // Otherwise the token object holds a temporary token which should not be
        // returned to the user.
        return m_token;
    }
    else
    {
#if !defined(_WIN32) || _MSC_VER >= 1900
        static const oauth1_token empty_token;
#endif
        return empty_token;
    }
}

#define _OAUTH1_METHODS
#define DAT(a,b) const oauth1_method oauth1_methods::a = b;
#include "cpprest/details/http_constants.dat"
#undef _OAUTH1_METHODS
#undef DAT

}}}}

#endif
