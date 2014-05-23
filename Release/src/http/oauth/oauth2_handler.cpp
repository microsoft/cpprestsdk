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
* HTTP Library: Oauth 2.0 protocol handler
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/oauth2_handler.h"
#include "cpprest/http_helpers.h"

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::details;

namespace web { namespace http { namespace client { namespace experimental
{


utility::string_t oauth2_config::build_authorization_uri()
{
    const utility::string_t response_type((implicit_grant()) ? U("token") : U("code"));
    uri_builder u(auth_endpoint());
    u.append_query(U("response_type"), response_type);
    u.append_query(U("client_id"), client_key());
    u.append_query(U("redirect_uri"), redirect_uri());

    if (custom_state().empty())
    {
        m_state = m_state_generator.generate();
    }
    else
    {
        m_state = custom_state();
    }
    u.append_query(U("state"), state());

    if (!scope().empty())
    {
        u.append_query(U("scope"), scope());
    }
    return u.to_string();
}

pplx::task<void> oauth2_config::token_from_redirected_uri(uri redirected_uri)
{
    auto query = uri::split_query((implicit_grant()) ? redirected_uri.fragment() : redirected_uri.query());
    
    auto state_param = query.find(_XPLATSTR("state"));
    if (state_param == query.end())
    {
        throw oauth2_exception(_XPLATSTR("parameter 'state' missing from redirected URI."));
    }
    if (state() != state_param->second)
    {
        utility::ostringstream_t err;
        err << _XPLATSTR("redirected URI parameter 'state'='") << state_param->second
            << _XPLATSTR("' does not match state='") << state() << _XPLATSTR("'.");
        throw oauth2_exception(err.str().c_str());
    }

    auto code_param = query.find(_XPLATSTR("code"));
    if (code_param != query.end())
    {
        return token_from_code(code_param->second);
    }

    // NOTE: The redirected URI contains access token only in the implicit grant.
    // The implicit grant never passes a refresh token.
    auto token_param = query.find(_XPLATSTR("access_token"));
    if (token_param == query.end())
    {
        throw oauth2_exception(_XPLATSTR("either 'code' or 'access_token' parameter must be in the redirected URI."));
    }

    set_token(token_param->second);
    return pplx::create_task([](){});
}

pplx::task<void> oauth2_config::token_from_code(utility::string_t authorization_code)
{
    return _request_token(U("grant_type=authorization_code&code=") + uri::encode_data_string(authorization_code)
        + U("&redirect_uri=") + uri::encode_data_string(redirect_uri()));
}

pplx::task<void> oauth2_config::token_from_refresh()
{
    return _request_token(U("grant_type=refresh_token&refresh_token=")
        + uri::encode_data_string(token().refresh_token()));
}

pplx::task<void> oauth2_config::_request_token(utility::string_t request_body)
{
    http_request request;
    request.set_method(methods::POST);
    request.set_request_uri(utility::string_t());

    if (!scope().empty())
    {
        request_body += U("&scope=") + uri::encode_data_string(scope());
    }

    if (http_basic_auth())
    {
        std::vector<unsigned char> creds_vec(conversions::to_body_data(
            uri::encode_data_string(client_key()) + U(":") + uri::encode_data_string(client_secret()))
        );
        request.headers().add(header_names::authorization, U("Basic ") + conversions::to_base64(std::move(creds_vec)));
    }
    else
    {
        request_body += U("&client_id=") + uri::encode_data_string(client_key())
            + U("&client_secret=") + uri::encode_data_string(client_secret());
    }
    request.set_body(request_body, mime_types::application_x_www_form_urlencoded);

    http_client token_client(token_endpoint());

    return token_client.request(request)
    .then([this](pplx::task<http_response> resp_task)
    {
        json::value resp_json;
        try
        {
            resp_json = resp_task.get().extract_json().get();
        }
        catch (http_exception &e)
        {
            throw oauth2_exception(_XPLATSTR("encountered http_exception: ") + conversions::to_string_t(std::string(e.what())));
        }
        catch (json::json_exception &e)
        {
            throw oauth2_exception(_XPLATSTR("encountered json_exception: ") + conversions::to_string_t(std::string(e.what())));
        }
        catch (std::exception &e)
        {
            throw oauth2_exception(_XPLATSTR("encountered exception: ") + conversions::to_string_t(std::string(e.what())));
        }
        catch (...)
        {
            throw oauth2_exception(_XPLATSTR("encountered unknown exception"));
        }

        set_token(_parse_token_from_json(resp_json));
    });
}

oauth2_token oauth2_config::_parse_token_from_json(json::value& token_json)
{
    oauth2_token result;

    try
    {
        result.set_access_token(token_json[_XPLATSTR("access_token")].as_string());
    }
    catch (json::json_exception)
    {
        throw oauth2_exception(_XPLATSTR("response json contains no 'access_token': ") + token_json.serialize());
    }

    try
    {
        result.set_token_type(token_json[_XPLATSTR("token_type")].as_string());
    }
    catch (json::json_exception)
    {
        // Some services don't return 'token_type' while it's required by OAuth 2.0 spec:
        // http://tools.ietf.org/html/rfc6749#section-5.1
        // As workaround we act as if 'token_type=bearer' was received.
        result.set_token_type(_XPLATSTR("bearer"));
    }
    if (!utility::details::str_icmp(result.token_type(), _XPLATSTR("bearer")))
    {
        throw oauth2_exception(_XPLATSTR("only 'token_type=bearer' access tokens are currently supported: ") + token_json.serialize());
    }

    try
    {
        result.set_refresh_token(token_json[_XPLATSTR("refresh_token")].as_string());
    }
    catch (json::json_exception)
    {
        // Do nothing. Preserves the old refresh token.
    }

    try
    {
        result.set_expires_in(token_json[_XPLATSTR("expires_in")].as_integer());
    }
    catch (json::json_exception)
    {
        result.set_expires_in(-1); // Set as unspecified.
    }

    try
    {
        result.set_scope(token_json[_XPLATSTR("scope")].as_string());
    }
    catch (json::json_exception)
    {
        result.set_scope(scope()); // Set to current scope().
    }

    return result;
}

pplx::task<http_response> oauth2_handler::propagate(http_request request)
{
    if (config().is_enabled())
    {
        if (config().bearer_auth())
        {
            request.headers().add(header_names::authorization, _XPLATSTR("Bearer ") + config().token().access_token());
        }
        else
        {
            uri_builder ub(request.request_uri());
            ub.append_query(config().access_token_key(), config().token().access_token());
            request.set_request_uri(ub.to_uri());
        }
    }
    return next_stage()->propagate(request);
}


}}}} // namespace web::http::client::experimental
