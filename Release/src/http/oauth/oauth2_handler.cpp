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
* oauth2_handler.cpp
*
* HTTP Library: Oauth 2.0 handler
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/oauth2_handler.h"
//#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_helpers.h"

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::details;

namespace web { namespace http { namespace client { namespace experimental
{


utility::string_t oauth2_config::build_authorization_uri(utility::string_t state) const
{
    // TODO: Use "token" here for implicit flow. This would get token directly without auth code.
    const string_t response_type(U("code"));
    uri_builder u(m_auth_endpoint);
    u.append_query(U("response_type"), response_type);
    u.append_query(U("client_id"), m_client_key);
    u.append_query(U("redirect_uri"), m_redirect_uri);
    u.append_query(U("state"), state);
    if (!scope().empty())
    {
        u.append_query(U("scope"), scope());
    }
    return u.to_string();
}

pplx::task<void> oauth2_config::fetch_token(utility::string_t authorization_code, bool do_http_basic_auth)
{
    http_client token_client(m_token_endpoint);
    http_request request;
    request.set_method(methods::POST);
    request.set_request_uri(U(""));

    utility::string_t request_body(U("grant_type=authorization_code&code=") + uri::encode_data_string(authorization_code)
            + U("&redirect_uri=") + uri::encode_data_string(m_redirect_uri));
    if (!do_http_basic_auth)
    {
        request_body += U("&client_id=") + uri::encode_data_string(m_client_key)
            + U("&client_secret=") + uri::encode_data_string(m_client_secret);
    }
    else
    {
        std::vector<unsigned char> creds_vec(conversions::to_body_data(
                uri::encode_data_string(m_client_key) + U(":") + uri::encode_data_string(m_client_secret))
        );
        request.headers().add(header_names::authorization, U("Basic ") + conversions::to_base64(std::move(creds_vec)));
    }
    request.set_body(request_body, mime_types::application_x_www_form_urlencoded);

    return token_client.request(request)
    .then([](http_response response)
    {
        return response.extract_json();
    })
    .then([](json::value token_json)
    {
        return token_json[U("access_token")].as_string();
    })
    .then([&](utility::string_t token)
    {
        this->set_token(token);
    });
}

pplx::task<http_response> oauth2_handler::propagate(http_request request)
{
    if (m_config.is_enabled())
    {
        if (m_config.bearer_auth())
        {
            request.headers().add(header_names::authorization, _XPLATSTR("Bearer ") + m_config.token());
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


}}}} // namespace web::http::client::experimental
