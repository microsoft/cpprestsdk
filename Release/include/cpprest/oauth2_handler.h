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
* oauth2_handler.h
*
* HTTP Library: Oauth 2.0 protocol handler
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_OAUTH2_HANDLER_H
#define _CASA_OAUTH2_HANDLER_H

#include "cpprest/http_msg.h"

namespace web
{
namespace http
{
namespace client
{


/// <summary>
/// Oauth2 configuration.
/// </summary>
struct oauth2_config
{
    oauth2_config(utility::string_t client_key, utility::string_t client_secret,
            utility::string_t auth_endpoint, utility::string_t token_endpoint,
            utility::string_t redirect_uri) :
                m_client_key(client_key),
                m_client_secret(client_secret),
                m_auth_endpoint(auth_endpoint),
                m_token_endpoint(token_endpoint),
                m_redirect_uri(redirect_uri)
    {
    }

    oauth2_config(utility::string_t token) :
        m_token(std::move(token))
    {
    }

    utility::string_t build_authorization_uri(utility::string_t state) const;

    pplx::task<void> fetch_token(utility::string_t authorization_code, bool do_http_basic_auth=true);

    bool is_enabled() const { return !m_token.empty(); }

    const utility::string_t& client_key() const { return m_client_key; }
    void set_client_key(utility::string_t client_key) { m_client_key = std::move(client_key); }

    const utility::string_t& client_secret() const { return m_client_secret; }
    void set_client_secret(utility::string_t client_secret) { m_client_secret = std::move(client_secret); }

    const utility::string_t& auth_endpoint() const { return m_auth_endpoint; }
    void set_auth_endpoint(utility::string_t auth_endpoint) { m_auth_endpoint = std::move(auth_endpoint); }

    const utility::string_t& token_endpoint() const { return m_token_endpoint; }
    void set_token_endpoint(utility::string_t token_endpoint) { m_token_endpoint = std::move(token_endpoint); }

    const utility::string_t& redirect_uri() const { return m_redirect_uri; }
    void set_redirect_uri(utility::string_t redirect_uri) { m_redirect_uri = std::move(redirect_uri); }

    const utility::string_t& scope() const { return m_scope; }
    void set_scope(utility::string_t scope) { m_scope = std::move(scope); }

    const utility::string_t& token() const { return m_token; }
    void set_token(utility::string_t token) { m_token = std::move(token); }

    bool bearer_auth() const { return m_bearer_auth; }
    void set_bearer_auth(bool enable) { m_bearer_auth = std::move(enable); }

    const utility::string_t&  access_token_key() const { return m_access_token_key; }
    void set_access_token_key(utility::string_t access_token_key) { m_access_token_key = std::move(access_token_key); }

private:
    friend class http_client_config;
    oauth2_config() {}

    utility::string_t m_client_key;
    utility::string_t m_client_secret;
    utility::string_t m_auth_endpoint;
    utility::string_t m_token_endpoint;
    utility::string_t m_redirect_uri;
    utility::string_t m_scope;
    utility::string_t m_token;
    bool m_bearer_auth = true;
    utility::string_t m_access_token_key = _XPLATSTR("access_token");
};


/// <summary>
/// Oauth2 handler. Specialization of http_pipeline_stage.
/// </summary>
class oauth2_handler : public http_pipeline_stage
{
public:
    oauth2_handler(oauth2_config config) : m_config(std::move(config)) {}

    void set_config(oauth2_config config) { m_config = std::move(config); }
    const oauth2_config& get_config() const { return m_config; }

    virtual pplx::task<http_response> propagate(http_request request) override;

private:
    oauth2_config m_config;
};


}}} // namespace web::http::client

#endif  /* _CASA_OAUTH2_HANDLER_H */
