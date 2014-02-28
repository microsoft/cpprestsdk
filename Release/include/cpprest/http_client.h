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
* http_client.h
*
* HTTP Library: Client-side APIs.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_HTTP_CLIENT_H
#define _CASA_HTTP_CLIENT_H

#if defined (__cplusplus_winrt)
#define __WRL_NO_DEFAULT_LIB__
#include <wrl.h>
#include <msxml6.h>
namespace web { namespace http{namespace client{
typedef IXMLHTTPRequest2* native_handle;}}}
#else
namespace web { namespace http{namespace client{
typedef void* native_handle;}}}
#endif // __cplusplus_winrt

#include <memory>
#include <limits>

#include "cpprest/xxpublic.h"
#include "cpprest/http_msg.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else 
#include "pplx/pplxtasks.h"
#endif

#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/basic_types.h"
#include "cpprest/asyncrt_utils.h"

namespace web 
{ 
namespace http
{
namespace client
{

#ifdef _MS_WINDOWS
namespace details {
#ifdef __cplusplus_winrt
        class winrt_client ;
#else
        class winhttp_client;
#endif // __cplusplus_winrt
}  
#endif // _MS_WINDOWS

/// <summary>
/// credentials represents a set of user credentials (username and password) to be used
/// for the client and proxy authentication
/// </summary>
class credentials
{
public:
    credentials(utility::string_t username, utility::string_t password) :
        m_is_set(true),
        m_username(std::move(username)),
        m_password(std::move(password))
    {}

    const utility::string_t& username() const { return m_username; }
    const utility::string_t& password() const { return m_password; }
    bool is_set() const { return m_is_set; }

private:
    friend class web_proxy;
    friend class http_client_config;
    credentials() : m_is_set(false) {}

    utility::string_t m_username;
    utility::string_t m_password;
    bool m_is_set;
};

/// <summary>
/// web_proxy represents the concept of the web proxy, which can be auto-discovered,
/// disabled, or specified explicitly by the user
/// </summary>
class web_proxy
{
    enum web_proxy_mode_internal{ use_default_, use_auto_discovery_, disabled_, user_provided_ };
public:
    enum web_proxy_mode{ use_default = use_default_, use_auto_discovery = use_auto_discovery_, disabled  = disabled_};

    /// <summary>
    /// Constructs a proxy with the default settings.
    /// </summary>
    web_proxy() : m_address(_XPLATSTR("")), m_mode(use_default_) {}
    
    /// <summary>
    /// Creates a proxy with specified mode.
    /// </summary>
    /// <param name="mode">Mode to use.</param>
    web_proxy( web_proxy_mode mode ) : m_address(_XPLATSTR("")), m_mode(static_cast<web_proxy_mode_internal>(mode)) {}
    
    /// <summary>
    /// Creates a proxy explicitly with provided address.
    /// </summary>
    /// <param name="address">Proxy URI to use.</param>
    web_proxy( http::uri address ) : m_address(address), m_mode(user_provided_) {}

    /// <summary>
    /// Gets this proxy's URI address. Returns an empty URI if not explicitly set by user.
    /// </summary>
    /// <returns>A reference to this proxy's URI.</returns>
    const http::uri& address() const { return m_address; }

    /// <summary>
    /// Gets the credentials used for authentication with this proxy.
    /// </summary>
    /// <returns>Credentials to for this proxy.</returns>
    const http::client::credentials& credentials() const { return m_credentials; }
    
    /// <summary>
    /// Sets the credentials to use for authentication with this proxy.
    /// </summary>
    /// <param name="cred">Credentials to use for this proxy.</param>
    void set_credentials(http::client::credentials cred) {
        if( m_mode == disabled_ )
        {
            throw std::invalid_argument("Cannot attach credentials to a disabled proxy");
        }
        m_credentials = std::move(cred);
    }

    /// <summary>
    /// Checks if this proxy was constructed with default settings.
    /// </summary>
    /// <returns>True if default, false otherwise.</param>
    bool is_default() const { return m_mode == use_default_; }

    /// <summary>
    /// Checks if using a proxy is disabled.
    /// </summary>
    /// <returns>True if disabled, false otherwise.</returns>
    bool is_disabled() const { return m_mode == disabled_; }

    /// <summary>
    /// Checks if the auto discovery protocol, WPAD, is to be used.
    /// </summary>
    /// <returns>True if auto discovery enabled, false otherwise.</returns>
    bool is_auto_discovery() const { return m_mode == use_auto_discovery_; }
    
    /// <summary>
    /// Checks if a proxy address is explicitly specified by the user.
    /// </summary>
    /// <returns>True if a proxy address was explicitly specified, false otherwise.</returns> 
    bool is_specified() const { return m_mode == user_provided_; }

private:
    http::uri m_address;
    web_proxy_mode_internal m_mode;
    http::client::credentials m_credentials;
};

/// <summary>
/// HTTP client configuration class, used to set the possible configuration options
/// used to create an http_client instance.
/// </summary>
class http_client_config
{
public:
    http_client_config() : 
        m_guarantee_order(false),
        m_timeout(utility::seconds(30)),
        m_chunksize(64 * 1024)
#if !defined(__cplusplus_winrt)
        , m_validate_certificates(true)
#endif
        ,m_set_user_nativehandle_options([](native_handle)->void{})
    {
    }

    /// <summary>
    /// Get the web proxy object
    /// </summary>
    /// <returns>A reference to the web proxy object.</returns>
    const web_proxy& proxy() const
    {
        return m_proxy;
    }

    /// <summary>
    /// Set the web proxy object
    /// </summary>
    /// <param name="proxy">A reference to the web proxy object.</param>
    void set_proxy(const web_proxy& proxy)
    {
        m_proxy = proxy;
    }

    /// <summary>
    /// Get the client credentials
    /// </summary>
    /// <returns>A reference to the client credentials.</returns>
    const http::client::credentials& credentials() const
    {
        return m_credentials;
    }

    /// <summary>
    /// Set the client credentials
    /// </summary>
    /// <param name="cred">A reference to the client credentials.</param>
    void set_credentials(const http::client::credentials& cred)
    {
        m_credentials = cred;
    }

    /// <summary>
    /// Get the 'guarantee order' property
    /// </summary>
    /// <returns>The value of the property.</returns>
    bool guarantee_order() const
    {
        return m_guarantee_order;
    }

    /// <summary>
    /// Set the 'guarantee order' property
    /// </summary>
    /// <param name="guarantee_order">The value of the property.</param>
    void set_guarantee_order(bool guarantee_order)
    {
        m_guarantee_order = guarantee_order;
    }

    /// <summary>
    /// Get the timeout
    /// </summary>
    /// <returns>The timeout (in seconds) used for each send and receive operation on the client.</returns>
    utility::seconds timeout() const
    {
        return m_timeout;
    }

    /// <summary>
    /// Set the timeout
    /// </summary>
    /// <param name="timeout">The timeout (in seconds) used for each send and receive operation on the client.</param>
    void set_timeout(utility::seconds timeout)
    {
        m_timeout = timeout;
    }

    /// <summary>
    /// Get the client chunk size.
    /// </summary>
    /// <returns>The internal buffer size used by the http client when sending and receiving data from the network.</returns>
    size_t chunksize() const
    {
        return m_chunksize;
    }

    /// <summary>
    /// Sets the client chunk size.
    /// </summary>
    /// <param name="size">The internal buffer size used by the http client when sending and receiving data from the network.</param>
    /// <remarks>This is a hint -- an implementation may disregard the setting and use some other chunk size.</remarks>
    void set_chunksize(size_t size)
    {
        m_chunksize = size;
    }

#if !defined(__cplusplus_winrt)
    /// <summary>
    /// Gets the server certificate validation property.
    /// </summary>
    /// <returns>True if certificates are to be verified, false otherwise.</returns>
    bool validate_certificates() const
    {
        return m_validate_certificates;
    }

    /// <summary>
    /// Sets the server certificate validation property.
    /// </summary>
    /// <param name="validate_cert">False to turn ignore all server certificate validation errors, true otherwise.</param>
    /// <remarks>Note ignoring certificate errors can be dangerous and should be done with caution.</remarks>
    void set_validate_certificates(bool validate_certs)
    {
        m_validate_certificates = validate_certs;
    }
#endif

    /// <summary>
    /// Sets a callback to enable custom setting of winhttp options
    /// </summary>
    /// <param name="callback">A user callback allowing for customization of the request</param>
    void set_nativehandle_options(std::function<void(native_handle)> callback)
    {
         m_set_user_nativehandle_options = callback;
    }

private:
    web_proxy m_proxy;
    http::client::credentials m_credentials;
    // Whether or not to guarantee ordering, i.e. only using one underlying TCP connection.
    bool m_guarantee_order;

    // IXmlHttpRequest2 doesn't allow configuration of certificate verification.
#if !defined(__cplusplus_winrt)
    bool m_validate_certificates;
#endif
    std::function<void(native_handle)> m_set_user_nativehandle_options;

    utility::seconds m_timeout;
    size_t m_chunksize;

#ifdef _MS_WINDOWS
#ifdef __cplusplus_winrt
    friend class details::winrt_client;
#else
    friend class details::winhttp_client;
#endif // __cplusplus_winrt  
#endif // _MS_WINDOWS


    /// <summary>
    /// Invokes a user callback to allow for customization of the requst
    /// </summary>
    /// <param name="handle">The internal http_request handle</param>
    /// <returns>True if users set WinHttp/IXAMLHttpRequest2 options correctly, false otherwise.</returns>
    void call_user_nativehandle_options(native_handle handle) const
    {
         m_set_user_nativehandle_options(handle);
    }
};

/// <summary>
/// HTTP client class, used to maintain a connection to an HTTP service for an extended session.
/// </summary>
class http_client
{
public:
    /// <summary>
    /// Creates a new http_client connected to specified uri.
    /// </summary>
    /// <param name="base_uri">A string representation of the base uri to be used for all requests. Must start with either "http://" or "https://"</param>
    _ASYNCRTIMP http_client(uri base_uri);

    /// <summary>
    /// Creates a new http_client connected to specified uri.
    /// </summary>
    /// <param name="base_uri">A string representation of the base uri to be used for all requests. Must start with either "http://" or "https://"</param>
    /// <param name="client_config">The http client configuration object containing the possible configuration options to intitialize the <c>http_client</c>. </param>
    _ASYNCRTIMP http_client(uri base_uri, http_client_config client_config);

    /// <summary>
    /// Note the destructor doesn't necessarily close the connection and release resources.
    /// The connection is reference counted with the http_responses.
    /// </summary>
    ~http_client() {}

    /// <summary>
    /// Gets the base uri
    /// </summary>
    /// <returns>
    /// A base uri initialized in constructor
    /// </return>
    const uri& base_uri() const
    {
        return _base_uri;
    }

    /// <summary>
    /// Get client configuration object
    /// </summary>
    /// <returns>A reference to the client configuration object.</returns>
    _ASYNCRTIMP const http_client_config& client_config() const;

    /// <summary>
    /// Adds an HTTP pipeline stage to the client.
    /// </summary>
    /// <param name="handler">A function object representing the pipeline stage.</param>
    void add_handler(std::function<pplx::task<http_response>(http_request, std::shared_ptr<http::http_pipeline_stage>)> handler)
    {
        m_pipeline->append(std::make_shared< ::web::http::details::function_pipeline_wrapper>(handler));
    }

    /// <summary>
    /// Adds an HTTP pipeline stage to the client.
    /// </summary>
    /// <param name="stage">A shared pointer to a pipeline stage.</param>
    void add_handler(std::shared_ptr<http::http_pipeline_stage> stage)
    {
        m_pipeline->append(stage);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="request">Request to send.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    _ASYNCRTIMP pplx::task<http_response> request(http_request request, pplx::cancellation_token token = pplx::cancellation_token::none());

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(method mtd, pplx::cancellation_token token = pplx::cancellation_token::none())
    {
        http_request msg(std::move(mtd));
        return request(msg, token);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        pplx::cancellation_token token = pplx::cancellation_token::none())
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        return request(msg, token);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body_data">The data to be used as the message body, represented using the json object library.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment, 
        const json::value &body_data,
        pplx::cancellation_token token = pplx::cancellation_token::none())
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body_data);
        return request(msg, token);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <param name="body_data">String containing the text to use in the message body.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        const utility::string_t &body_data,
        utility::string_t content_type = _XPLATSTR("text/plain"),
        pplx::cancellation_token token = pplx::cancellation_token::none())
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body_data, std::move(content_type));
        return request(msg, token);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body_data">String containing the text to use in the message body.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        const utility::string_t &body_data,
        pplx::cancellation_token token)
    {
        return request(mtd, path_query_fragment, body_data, _XPLATSTR("text/plain"), token);
    }

#if !defined (__cplusplus_winrt)
    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        concurrency::streams::istream body,
        utility::string_t content_type = _XPLATSTR("application/octet-stream"),
        pplx::cancellation_token token = pplx::cancellation_token::none())
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body, std::move(content_type));
        return request(msg, token);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        concurrency::streams::istream body,
        pplx::cancellation_token token)
    {
        return request(mtd, path_query_fragment, body, _XPLATSTR("application/octet-stream"), token);
    }
#endif // __cplusplus_winrt

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <param name="content_length">Size of the message body.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    /// <remarks>Winrt requires to provide content_length.</remarks>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        concurrency::streams::istream body,
        size_t content_length,
        utility::string_t content_type= _XPLATSTR("application/octet-stream"),
        pplx::cancellation_token token = pplx::cancellation_token::none())
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body, content_length, std::move(content_type));
        return request(msg, token);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <param name="content_length">Size of the message body.</param>
    /// <param name="token">Cancellation token for cancellation of this request operation.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    /// <remarks>Winrt requires to provide content_length.</remarks>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        concurrency::streams::istream body,
        size_t content_length,
        pplx::cancellation_token token)
    {
        return request(mtd, path_query_fragment, body, content_length, _XPLATSTR("application/octet-stream"), token);
    }

private:

    void build_pipeline(uri base_uri, http_client_config client_config);
    
    std::shared_ptr<::web::http::http_pipeline> m_pipeline;

    uri _base_uri;
};

} // namespace client
}} // namespace web::http

#endif  /* _CASA_HTTP_CLIENT_H */
