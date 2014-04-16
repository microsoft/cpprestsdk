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
    oauth2_config(utility::string_t token, bool bearer_auth=true,
            utility::string_t access_token_key=_XPLATSTR("access_token")) :
        m_token(std::move(token)),
        m_bearer_auth(bearer_auth),
        m_access_token_key(access_token_key)
    {
    }

    const utility::string_t& token() const { return m_token; }
    void set_token(utility::string_t token) { m_token = std::move(token); }

    bool bearer_auth() const { return m_bearer_auth; }
    void set_bearer_auth(bool enable) { m_bearer_auth = std::move(enable); }

    const utility::string_t&  access_token_key() const { return m_access_token_key; }
    void set_access_token_key(utility::string_t access_token_key) { m_access_token_key = std::move(access_token_key); }

    bool is_enabled() const { return !m_token.empty(); }

private:
    friend class http_client_config;
    oauth2_config() : m_bearer_auth(true) {}

    utility::string_t m_token;
    bool m_bearer_auth;
    utility::string_t m_access_token_key;
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

    virtual pplx::task<http_response> propagate(http_request request) override
    {
        if (m_config.is_enabled())
        {
            if (m_config.bearer_auth())
            {
                request.headers().add(_XPLATSTR("Authorization"), _XPLATSTR("Bearer ") + m_config.token());
            }
            else
            {
                uri_builder ub(request.request_uri());
                ub.append_query(m_config.access_token_key(), m_config.token());
                request.set_request_uri(ub.to_uri());
            }
        }
        return next_stage()->propagate(request);
    }

private:
    oauth2_config m_config;
};


}}} // namespace web::http::client

#endif  /* _CASA_OAUTH2_HANDLER_H */
