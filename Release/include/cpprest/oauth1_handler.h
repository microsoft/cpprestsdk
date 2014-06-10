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
* oauth1_handler.h
*
* HTTP Library: Oauth 1.0 protocol handler
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_OAUTH1_HANDLER_H
#define _CASA_OAUTH1_HANDLER_H

#include "cpprest/http_msg.h"

using namespace utility;

namespace web
{
namespace http
{
namespace client
{
class http_client_config;
namespace experimental
{


/// <summary>
/// Constant strings for OAuth 1.0 signature methods.
/// </summary>
typedef utility::string_t oauth1_method;
class oauth1_methods
{
public:
#define _OAUTH1_METHODS
#define DAT(a,b) _ASYNCRTIMP static const oauth1_method a;
#include "cpprest/http_constants.dat"
#undef _OAUTH1_METHODS
#undef DAT
};


/// <summary>
/// Constant strings for OAuth 1.0.
/// </summary>
typedef utility::string_t oauth1_string;
class oauth1_strings
{
public:
#define _OAUTH1_STRINGS
#define DAT(a_, b_) _ASYNCRTIMP static const oauth1_string a_;
#include "cpprest/http_constants.dat"
#undef _OAUTH1_STRINGS
#undef DAT
};


/// <summary>
/// Exception type for OAuth 1.0 errors.
/// </summary>
class oauth1_exception : public std::exception
{
public:
    oauth1_exception(utility::string_t msg) : m_msg(utility::conversions::to_utf8string(std::move(msg))) {}
    ~oauth1_exception() _noexcept {}
    const char* what() const _noexcept { return m_msg.c_str(); }

private:
    std::string m_msg;
};


/// <summary>
/// OAuth 1.0 token and associated information.
/// </summary>
class oauth1_token
{
public:
    oauth1_token(utility::string_t token=utility::string_t(),
            utility::string_t secret=utility::string_t()) :
        m_token(token),
        m_secret(secret)
    {}

    bool is_valid() const { return !(token().empty() || secret().empty()); }

    const utility::string_t& token() const { return m_token; }
    void set_token(utility::string_t token) { m_token = std::move(token); }

    const utility::string_t& secret() const { return m_secret; }
    void set_secret(utility::string_t secret) { m_secret = std::move(secret); }

private:
    utility::string_t m_token;
    utility::string_t m_secret;
};


/// <summary>
/// State currently used by oauth1_config to authenticate request.
/// The state varies for every request (due to timestamp and nonce).
/// The state also contains extra transmitted protocol parameters during
/// authorization flow (i.e. 'oauth_callback' or 'oauth_verifier').
/// </summary>
class oauth1_auth_state
{
public:

    oauth1_auth_state(utility::string_t timestamp, utility::string_t nonce) :
        m_timestamp(timestamp),
        m_nonce(nonce)
    {}

    oauth1_auth_state(utility::string_t timestamp, utility::string_t nonce,
            utility::string_t extra_key, utility::string_t extra_value) :
        m_timestamp(timestamp),
        m_nonce(nonce),
        m_extra_key(extra_key),
        m_extra_value(extra_value)
    {}

    const utility::string_t& timestamp() const { return m_timestamp; }
    void set_timestamp(utility::string_t timestamp) { m_timestamp = std::move(timestamp); }
    
    const utility::string_t& nonce() const { return m_nonce; }
    void set_nonce(utility::string_t nonce) { m_nonce = std::move(nonce); }

    const utility::string_t& extra_key() const { return m_extra_key; }
    void set_extra_key(utility::string_t key) { m_extra_key = std::move(key); }

    const utility::string_t& extra_value() const { return m_extra_value; }
    void set_extra_value(utility::string_t value) { m_extra_value = std::move(value); }

private:
    utility::string_t m_timestamp;
    utility::string_t m_nonce;
    utility::string_t m_extra_key;
    utility::string_t m_extra_value;
};



/// <summary>
/// Oauth1 configuration.
/// </summary>
class oauth1_config
{
public:
    oauth1_config(utility::string_t consumer_key, utility::string_t consumer_secret,
            utility::string_t temp_endpoint, utility::string_t auth_endpoint,
            utility::string_t token_endpoint, utility::string_t callback_uri,
            oauth1_method method) :
        m_consumer_key(consumer_key),
        m_consumer_secret(consumer_secret),
        m_temp_endpoint(temp_endpoint),
        m_auth_endpoint(auth_endpoint),
        m_token_endpoint(token_endpoint),
        m_callback_uri(callback_uri),
        m_method(std::move(method)),
        m_use_core10(false)
    {}

    oauth1_config(utility::string_t consumer_key, utility::string_t consumer_secret,
            oauth1_token token, oauth1_method method) :
        m_consumer_key(consumer_key),
        m_consumer_secret(consumer_secret),
        m_token(std::move(token)),
        m_method(std::move(method)),
        m_use_core10(false)
    {}

    /// <summary>
    /// Builds an authorization URI to be loaded in the web browser.
    /// The URI is built with auth_endpoint() as basis.
    /// The method creates a task for HTTP request to first obtain a
    /// temporary token. The authorization URI build based on this token.
    /// The use_core10() option affects the process by passing 'oauth_callback'
    /// parameter either in the temporary token request or in the query part
    /// of the built authorization uri.
    /// </summary>
    _ASYNCRTIMP pplx::task<utility::string_t> build_authorization_uri();

    /// <summary>
    /// Get the access token based on redirected URI.
    /// Behavior depends on the use_core10() setting.
    /// If use_core10() is false, the URI is expected to contain 'oauth_verifier'
    /// parameter, which is then used to fetch a token using the
    /// token_from_verifier() method.
    /// See: http://tools.ietf.org/html/rfc5849#section-2.2
    /// Otherwise (for obsolete OAuth Core 1.0), the redirect URI parameter
    /// 'oauth_token' is used instead of 'oauth_verifier' to get the token.
    /// See: http://oauth.net/core/1.0/#auth_step2
    /// In both cases, the 'oauth_token' is parsed and verified to match
    /// the current token().
    /// When token is successfully obtained, set_token() is called, and config is
    /// ready for use.
    /// </summary>
    _ASYNCRTIMP pplx::task<void> token_from_redirected_uri(web::http::uri redirected_uri);

    /// <summary>
    /// Creates a task to fetch token from the token endpoint.
    /// The task creates a HTTP request to the token_endpoint() which is
    /// used exchange a verifier to an access token.
    /// If successful, resulting token is set as active via set_token().
    /// See: http://tools.ietf.org/html/rfc5849#section-2.3
    /// </summary>
    /// <param name="verifier">Verifier received via redirect upon successful authorization.</param>
    pplx::task<void> token_from_verifier(utility::string_t verifier)
    {
        return _request_token(_generate_auth_state(oauth1_strings::verifier, uri::encode_data_string(verifier)), false);
    }

    const utility::string_t& consumer_key() const { return m_consumer_key; }
    void set_consumer_key(utility::string_t key) { m_consumer_key = std::move(key); }

    const utility::string_t& consumer_secret() const { return m_consumer_secret; }
    void set_consumer_secret(utility::string_t secret) { m_consumer_secret = std::move(secret); }

    const utility::string_t& temp_endpoint() const { return m_temp_endpoint; }
    void set_temp_endpoint(utility::string_t temp_endpoint) { m_temp_endpoint = std::move(temp_endpoint); }

    const utility::string_t& auth_endpoint() const { return m_auth_endpoint; }
    void set_auth_endpoint(utility::string_t auth_endpoint) { m_auth_endpoint = std::move(auth_endpoint); }

    const utility::string_t& token_endpoint() const { return m_token_endpoint; }
    void set_token_endpoint(utility::string_t token_endpoint) { m_token_endpoint = std::move(token_endpoint); }

    const utility::string_t& callback_uri() const { return m_callback_uri; }
    void set_callback_uri(utility::string_t callback_uri) { m_callback_uri = std::move(callback_uri); }

    const oauth1_token& token() const { return m_token; }
    void set_token(oauth1_token token) { m_token = std::move(token); }

    const oauth1_method& method() const { return m_method; }
    void set_method(oauth1_method method) { m_method = std::move(method); }

    const utility::string_t& realm() const { return m_realm; }
    void set_realm(utility::string_t realm) { m_realm = std::move(realm); }

    bool use_core10() const { return m_use_core10; }
    /// <summary>
    /// If false, OAuth 1.0 protocol (RFC 5849) will be used.
    /// Otherwise the obsolete OAuth Core 1.0 version will be used.
    /// Default: False.
    /// OAuth 1.0 specification: http://tools.ietf.org/html/rfc5849
    /// OAuth Core 1.0 document: http://oauth.net/core/1.0/
    /// </summary>
    void set_use_core10(bool use_obsolete) { m_use_core10 = std::move(use_obsolete); }

    bool is_enabled() const { return token().is_valid() && !(consumer_key().empty() || consumer_secret().empty()); }

    /// <summary>
    /// Builds signature base string according to:
    /// http://tools.ietf.org/html/rfc5849#section-3.4.1.1
    /// </summary>
    // NOTE: Public for unit tests.
    _ASYNCRTIMP utility::string_t _build_signature_base_string(http_request request, oauth1_auth_state state) const;

    /// <summary>
    /// Builds HMAC-SHA1 signature according to:
    /// http://tools.ietf.org/html/rfc5849#section-3.4.2
    /// </summary>
    utility::string_t _build_hmac_sha1_signature(http_request request, oauth1_auth_state state) const
    {
        auto text(_build_signature_base_string(std::move(request), std::move(state)));
        auto digest(_hmac_sha1(_build_key(), std::move(text)));
        auto signature(conversions::to_base64(std::move(digest)));
        return signature;
    }

    /// <summary>
    /// Builds RSA-SHA1 signature according to:
    /// http://tools.ietf.org/html/rfc5849#section-3.4.3
    /// NOTE: This feature is not implemented.
    /// </summary>
    utility::string_t _build_rsa_sha1_signature(http_request, oauth1_auth_state) const
    {
        throw oauth1_exception(_XPLATSTR("RSA-SHA1 signature method is not implemented."));
    }

    /// <summary>
    /// Builds PLAINTEXT signature according to:
    /// http://tools.ietf.org/html/rfc5849#section-3.4.4
    /// </summary>
    utility::string_t _build_plaintext_signature() const
    {
        return _build_key();
    }

    oauth1_auth_state _generate_auth_state(utility::string_t extra_key, utility::string_t extra_value)
    {
        return oauth1_auth_state(_generate_timestamp(), _generate_nonce(), std::move(extra_key), std::move(extra_value));
    }

    oauth1_auth_state _generate_auth_state()
    {
        return oauth1_auth_state(_generate_timestamp(), _generate_nonce());
    }

private:
    friend class web::http::client::http_client_config;
    friend class oauth1_handler;
    oauth1_config() : m_use_core10(false) {}

    utility::string_t _generate_nonce()
    {
        return m_nonce_generator.generate();
    }

    static utility::string_t _generate_timestamp()
    {
        utility::ostringstream_t os;
        os << utility::datetime::utc_timestamp();
        return os.str();
    }

    _ASYNCRTIMP static std::vector<unsigned char> _hmac_sha1(const utility::string_t key, const utility::string_t data);

    static utility::string_t _build_base_string_uri(const uri& u);

    utility::string_t _build_normalized_parameters(web::http::uri u, const oauth1_auth_state& state) const;

    utility::string_t _build_signature(http_request request, oauth1_auth_state state) const;

    utility::string_t _build_key() const
    {
        return uri::encode_data_string(consumer_secret()) + _XPLATSTR("&") + uri::encode_data_string(token().secret());
    }

    void _authenticate_request(http_request &req)
    {
        _authenticate_request(req, _generate_auth_state());
    }
    
    _ASYNCRTIMP void _authenticate_request(http_request &req, oauth1_auth_state state);

    _ASYNCRTIMP pplx::task<void> _request_token(oauth1_auth_state state, bool is_temp_token_request);

    // Required.
    utility::string_t m_consumer_key;
    utility::string_t m_consumer_secret;
    oauth1_token m_token;
    oauth1_method m_method;
    
    // Optional.
    utility::string_t m_realm;

    // For authorization.
    utility::string_t m_temp_endpoint;
    utility::string_t m_auth_endpoint;
    utility::string_t m_token_endpoint;
    utility::string_t m_callback_uri;
    bool m_use_core10;

    utility::nonce_generator m_nonce_generator;
};


/// <summary>
/// Oauth1 handler. Specialization of http_pipeline_stage.
/// </summary>
class oauth1_handler : public http_pipeline_stage
{
public:
    oauth1_handler(oauth1_config cfg) :
        m_config(std::move(cfg))
    {}

    const oauth1_config& config() const { return m_config; }
    void set_config(oauth1_config cfg) { m_config = std::move(cfg); }

    virtual pplx::task<http_response> propagate(http_request request) override
    {
        if (m_config.is_enabled())
        {
            m_config._authenticate_request(request);
        }
        return next_stage()->propagate(request);
    }

private:
    oauth1_config m_config;
};


}}}} // namespace web::http::client::experimental

#endif  /* _CASA_OAUTH1_HANDLER_H */
