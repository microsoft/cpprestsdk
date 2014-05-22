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
class http_client_config;
namespace experimental
{


/// <summary>
/// Exception type for OAuth 2.0 errors.
/// </summary>
class oauth2_exception : public std::exception
{
public:
    oauth2_exception(utility::string_t msg) : m_msg(utility::conversions::to_utf8string(std::move(msg))) {}
    ~oauth2_exception() _noexcept {}
    const char* what() const _noexcept { return m_msg.c_str(); }

private:
    std::string m_msg;
};


/// <summary>
/// OAuth 2.0 configuration.
///
/// Encapsulates functionality for:
/// -  Authenticating requests with an access token.
/// -  Performing the OAuth 2.0 authorization code grant authorization flow.
///    See: http://tools.ietf.org/html/rfc6749#section-4.1
/// -  Performing the OAuth 2.0 implicit grant authorization flow.
///    See: http://tools.ietf.org/html/rfc6749#section-4.2
///
/// Performing OAuth 2.0 authorization:
/// 1. Set service and client/app parameters:
/// -  Client/app key & secret (as provided by the service).
/// -  The service authorization endpoint and token endpoint.
/// -  Your client/app redirect URI.
/// -  Use set_state() to assign a unique state string for the authorization
///    session (default: "").
/// -  If needed, use set_bearer_auth() to control bearer token passing in either
///    query or header (default: header). See: http://tools.ietf.org/html/rfc6750#section-2
/// -  If needed, use set_access_token_key() to set "non-standard" access token
///    key (default: "access_token").
/// -  If needed, use set_implicit_grant() to enable implicit grant flow.
/// 2. Build authorization URI with build_authorization_uri() and open this in web browser/control.
/// 3. The resource owner should then clicks "Yes" to authorize your client/app, and
///    as a result the web browser/control is redirected to redirect_uri().
/// 5. Capture the redirected URI either in web control or by HTTP listener.
/// 6. Pass the redirected URI to token_from_redirected_uri() to obtain access token.
/// -  The method ensures redirected URI contains same state() as set in step 1.
/// -  In implicit_grant() is false, this will create HTTP request to fetch access token
///    from the service. Otherwise access token is already included in the redirected URI.
///
/// Usage for issuing authenticated requests:
/// 1. Perform authorization as above to obtain the access token or use an existing token.
/// -  Some services provice option to generate access tokens for testing purposes.
/// 2. Pass the resulting oauth2_config with the access token to http_client_config::set_oauth2().
/// 3. Construct http_client with this http_client_config. After this all requests
///    by that client will be OAuth 2.0 authenticated.
///
/// </summary>
class oauth2_config
{
public:
    oauth2_config(utility::string_t client_key, utility::string_t client_secret,
            utility::string_t auth_endpoint, utility::string_t token_endpoint,
            utility::string_t redirect_uri) :
                m_client_key(client_key),
                m_client_secret(client_secret),
                m_auth_endpoint(auth_endpoint),
                m_token_endpoint(token_endpoint),
                m_redirect_uri(redirect_uri),
                m_implicit_grant(false),
                m_bearer_auth(true),
                m_http_basic_auth(true),
                m_access_token_key(_XPLATSTR("access_token"))
    {
    }

    oauth2_config(utility::string_t token) :
        m_token(std::move(token)),
        m_implicit_grant(false),
        m_bearer_auth(true),
        m_http_basic_auth(true),
        m_access_token_key(_XPLATSTR("access_token"))
    {
    }

    /// <summary>
    /// Builds an authorization URI to be loaded in the web browser.
    /// The URI is built with auth_endpoint() as basis.
    /// The implicit_grant() affects the built URI by selecting
    /// either authorization code or implicit grant flow.
    /// </summary>
    _ASYNCRTIMP utility::string_t build_authorization_uri() const;

    /// <summary>
    /// Get the access token based on redirected URI.
    /// Behavior depends on the implicit_grant() setting.
    /// If implicit_grant() is false redirect URI is parsed for 'code' query
    /// parameter which is then used to fetch a token
    /// from token_endpoint().
    /// Otherwise, redirect URI fragment part is parsed for 'access_token'
    /// parameter containing the token.
    /// In both cases 'state' parameter is parsed and verified to match state().
    /// When token is successfully obtained, set_token() is called, and config is
    /// ready for use.
    /// An oauth2_exception is thrown if anything fails.
    /// </summary>
    _ASYNCRTIMP pplx::task<void> token_from_redirected_uri(uri redirected_uri);

    /// <summary>
    /// Creates a task to fetch token from the token endpoint.
    /// The task creates a request to the token_endpoint() which is used exchange an authorization code to an access token.
    /// If successful, resulting token is set as active via set_token().
    /// </summary>
    /// <param name="authorization_code">Code received via redirect upon successful authorization.</param>
    _ASYNCRTIMP pplx::task<void> fetch_token(utility::string_t authorization_code);

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

    const utility::string_t& state() const { return m_state; }
    /// <summary>
    /// State string should be unique for each authorization session.
    /// This state string should be returned by the authorization server on redirect.
    /// </summary>
    void set_state(utility::string_t state) { m_state = std::move(state); }

    const utility::string_t& token() const { return m_token; }
    void set_token(utility::string_t token) { m_token = std::move(token); }

    bool implicit_grant() const { return m_implicit_grant; }
    /// <summary>
    /// False means authorization code grant flow is used.
    /// True means implicit grant flow is used.
    /// Default: False.
    /// </summary>
    void set_implicit_grant(bool enable) { m_implicit_grant = std::move(enable); }

    bool bearer_auth() const { return m_bearer_auth; }
    /// <summary>
    /// Bearer token passing method. This must be selected based on what the service accepts.
    /// True means token is passed in the request header. (http://tools.ietf.org/html/rfc6750#section-2.1)
    /// False means token in passed in the query parameters. (http://tools.ietf.org/html/rfc6750#section-2.3)
    /// Default: True.
    /// </summary>
    void set_bearer_auth(bool enable) { m_bearer_auth = std::move(enable); }

    bool http_basic_auth() const { return m_http_basic_auth; }
    /// <summary>
    /// Token endpoint authorization method. This must be selected based on what the service accepts.
    /// True means HTTP Basic is used for authentication.
    /// False means client key & secret are passed in the HTTP request body.
    /// Default: True.
    /// </summary>
    void set_http_basic_auth(bool enable) { m_http_basic_auth = std::move(enable); }

    const utility::string_t&  access_token_key() const { return m_access_token_key; }
    /// <summary>
    /// Set custom access token key field in the case service requires a "non-standard" key.
    /// Default access token key is "access_token".
    /// </summary>
    void set_access_token_key(utility::string_t access_token_key) { m_access_token_key = std::move(access_token_key); }

private:
    friend class web::http::client::http_client_config;
    oauth2_config() {}

    utility::string_t m_client_key;
    utility::string_t m_client_secret;
    utility::string_t m_auth_endpoint;
    utility::string_t m_token_endpoint;
    utility::string_t m_redirect_uri;
    utility::string_t m_scope;
    utility::string_t m_state;

    bool m_implicit_grant;
    bool m_bearer_auth;
    bool m_http_basic_auth;
    utility::string_t m_access_token_key;

    utility::string_t m_token;
};


/// <summary>
/// OAuth 2.0 handler.
/// Specialization of http_pipeline_stage to perform OAuth 2.0 request authentication.
/// </summary>
class oauth2_handler : public http_pipeline_stage
{
public:
    oauth2_handler(oauth2_config cfg) : m_config(std::move(cfg)) {}

    const oauth2_config& config() const { return m_config; }
    void set_config(oauth2_config cfg) { m_config = std::move(cfg); }

    _ASYNCRTIMP virtual pplx::task<http_response> propagate(http_request request) override;

private:
    oauth2_config m_config;
};


}}}} // namespace web::http::client::experimental

#endif  /* _CASA_OAUTH2_HANDLER_H */
