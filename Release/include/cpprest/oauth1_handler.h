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
/// Constant strings for oauth1 signature methods.
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
/// Oauth1 configuration.
/// </summary>
struct oauth1_config
{
    oauth1_config(utility::string_t key, utility::string_t secret,
            utility::string_t token, utility::string_t token_secret,
            oauth1_method method) :
        m_key(std::move(key)),
        m_secret(std::move(secret)),
        m_token(std::move(token)),
        m_token_secret(std::move(token_secret)),
        m_method(std::move(method))
    {
    }

    const utility::string_t& key() const { return m_key; }
    void set_key(utility::string_t key) { m_key = std::move(key); }

    const utility::string_t& secret() const { return m_secret; }
    void set_secret(utility::string_t secret) { m_secret = std::move(secret); }

    const utility::string_t& token() const { return m_token; }
    void set_token(utility::string_t token) { m_token = std::move(token); }

    const utility::string_t& token_secret() const { return m_token_secret; }
    void set_token_secret(utility::string_t token_secret) { m_token_secret = std::move(token_secret); }

    const oauth1_method& method() const { return m_method; }
    void set_method(oauth1_method method) { m_method = std::move(method); }

    const utility::string_t& realm() const { return m_realm; }
    void set_realm(utility::string_t realm) { m_realm = std::move(realm); }

    bool is_enabled() const { return !m_key.empty() && !m_secret.empty() && !m_token.empty() && !m_token_secret.empty(); }

private:
    friend class web::http::client::http_client_config;
    oauth1_config() {}

    // Required.
    utility::string_t m_key;
    utility::string_t m_secret;
    utility::string_t m_token;
    utility::string_t m_token_secret;
    oauth1_method m_method;

    // Optional.
    utility::string_t m_realm;
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

    _ASYNCRTIMP virtual pplx::task<http_response> propagate(http_request request) override;

    _ASYNCRTIMP utility::string_t _build_signature_base_string(http_request request, utility::string_t timestamp, utility::string_t nonce) const;

    _ASYNCRTIMP utility::string_t _build_hmac_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const;
// TODO: RSA-SHA1
//    utility::string_t _build_rsa_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const;
    _ASYNCRTIMP utility::string_t _build_plaintext_signature() const;

private:
    static utility::string_t _generate_timestamp();
    static std::vector<unsigned char> _hmac_sha1(const utility::string_t key, const utility::string_t data);
    static utility::string_t _build_base_string_uri(const uri& u);

    utility::string_t _build_normalized_parameters(uri u, utility::string_t timestamp, utility::string_t nonce) const;

    utility::string_t _build_signature(http_request request, utility::string_t timestamp, utility::string_t nonce) const;
    utility::string_t _build_key() const;

    oauth1_config m_config;
    utility::nonce_generator m_nonce_generator;
};


}}}} // namespace web::http::client::experimental

#endif  /* _CASA_OAUTH1_HANDLER_H */
