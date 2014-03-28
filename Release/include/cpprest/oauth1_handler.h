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

#include <random>
#include "cpprest/http_msg.h"

namespace web
{
namespace http
{
namespace client
{


/// <summary>
/// Constant strings for oauth1 signature methods.
/// </summary>
typedef utility::string_t oauth1_method;
class oauth1_methods
{
public:
#define _OAUTH1_METHODS
#define DAT(a,b) const static oauth1_method a;
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

    bool is_enabled() const { return !m_key.empty() && !m_secret.empty() && !m_token.empty() && !m_token_secret.empty(); }

    // Required.
    utility::string_t m_key;
    utility::string_t m_secret;
    utility::string_t m_token;
    utility::string_t m_token_secret;
    oauth1_method m_method;

    // Optional.
    utility::string_t m_realm;

private:
    friend class http_client_config;
    oauth1_config() {}
};


/// <summary>
/// Oauth1 handler. Specialization of http_pipeline_stage.
/// </summary>
class oauth1_handler : public http_pipeline_stage
{
public:
    oauth1_handler(oauth1_config config) :
        m_random(utility::datetime::utc_timestamp()),
        m_config(std::move(config))
    {}

    void set_config(oauth1_config config) { m_config = std::move(config); }
    const oauth1_config& get_config() const { return m_config; }

    virtual pplx::task<http_response> propagate(http_request request) override;

    utility::string_t _generate_nonce();

    utility::string_t _build_base_string_uri(const uri& u);
    utility::string_t _build_query_string(uri u, utility::string_t timestamp, utility::string_t nonce);
    utility::string_t _build_signature_base_string(http_request request, utility::string_t timestamp, utility::string_t nonce);

    utility::string_t _build_hmac_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce);
// TODO: RSA-SHA1
//    utility::string_t _build_rsa_sha1_signature(http_request request, utility::string_t timestamp, utility::string_t nonce);
    utility::string_t _build_plaintext_signature();
    utility::string_t _build_signature(http_request request, utility::string_t timestamp, utility::string_t nonce);

private:
    oauth1_config m_config;
    std::mt19937 m_random;
};


}}} // namespace web::http::client

#endif  /* _CASA_OAUTH1_HANDLER_H */
