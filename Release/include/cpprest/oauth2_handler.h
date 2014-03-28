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
    oauth2_config(utility::string_t token) : m_token(std::move(token)) {}

    bool is_enabled() const { return !m_token.empty(); }

    utility::string_t m_token;

private:
    friend class http_client_config;
    oauth2_config() {}
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
            request.headers().add(_XPLATSTR("Authorization"), _XPLATSTR("Bearer ") + m_config.m_token);
        }
        return next_stage()->propagate(request);
    }

private:
    oauth2_config m_config;
};


}}} // namespace web::http::client

#endif  /* _CASA_OAUTH2_HANDLER_H */
