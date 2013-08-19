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

namespace web { namespace http
{
namespace client
{

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

    web_proxy() : m_address(_XPLATSTR("")), m_mode(use_default_) {}
    web_proxy( web_proxy_mode mode ) : m_address(_XPLATSTR("")), m_mode(static_cast<web_proxy_mode_internal>(mode)) {}
    web_proxy( http::uri address ) : m_address(address), m_mode(user_provided_) {}

    const http::uri& address() const { return m_address; }

    const http::client::credentials& credentials() const { return m_credentials; }
    void set_credentials(http::client::credentials cred) {
        if( m_mode == disabled_ )
        {
            throw std::invalid_argument("Cannot attach credentials to a disabled proxy");
        }
        m_credentials = std::move(cred);
    }

    bool is_default() const { return m_mode == use_default_; }
    bool is_disabled() const { return m_mode == disabled_; }
    bool is_auto_discovery() const { return m_mode == use_auto_discovery_; }
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

private:
    web_proxy m_proxy;
    http::client::credentials m_credentials;
    // Whether or not to guarantee ordering, i.e. only using one underlying TCP connection.
    bool m_guarantee_order;

    // IXmlHttpRequest2 doesn't allow configuration of certificate verification.
#if !defined(__cplusplus_winrt)
    bool m_validate_certificates;
#endif

    utility::seconds m_timeout;
    size_t m_chunksize;
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
    _ASYNCRTIMP http_client(const uri &base_uri);

    /// <summary>
    /// Creates a new http_client connected to specified uri.
    /// </summary>
    /// <param name="base_uri">A string representation of the base uri to be used for all requests. Must start with either "http://" or "https://"</param>
            /// <param name="client_config">The http client configuration object containing the possible configuration options to intitialize the <c>http_client</c>. </param>
    _ASYNCRTIMP http_client(const uri &base_uri, const http_client_config& client_config);

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
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    _ASYNCRTIMP pplx::task<http_response> request(http_request request);

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(method mtd)
    {
        http_request msg(std::move(mtd));
        return request(msg);
    }

    /// <summary>
    /// Get client configuration object
    /// </summary>
    /// <returns>A reference to the client configuration object.</returns>
    _ASYNCRTIMP const http_client_config& client_config() const;

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(method mtd, const utility::string_t &path_query_fragment)
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        return request(msg);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body_data">The data to be used as the message body, represented using the json object library.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(method mtd, const utility::string_t &path_query_fragment, const json::value &body_data)
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body_data);
        return request(msg);
    }

    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <param name="body_data">String containing the text to use in the message body.</param>
    /// <returns>An asynchronous operation that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        const utility::string_t &body_data,
        utility::string_t content_type = _XPLATSTR("text/plain"))
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body_data, std::move(content_type));
        return request(msg);
    }

#if !defined (__cplusplus_winrt)
    /// <summary>
    /// Asynchronously sends an HTTP request.
    /// </summary>
    /// <param name="mtd">HTTP request method.</param>
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        concurrency::streams::istream body,
        utility::string_t content_type = _XPLATSTR("application/octet-stream"))
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body, std::move(content_type));
        return request(msg);
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
    /// <returns>A task that is completed once a response from the request is received.</returns>
    /// <remarks>Winrt requires to provide content_length.</remarks>
    pplx::task<http_response> request(
        method mtd, 
        const utility::string_t &path_query_fragment,
        concurrency::streams::istream body,
        size_t content_length,
        utility::string_t content_type= _XPLATSTR("application/octet-stream"))
    {
        http_request msg(std::move(mtd));
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body, content_length, std::move(content_type));
        return request(msg);
    }

private:

    void build_pipeline(const uri &base_uri, const http_client_config& client_config);
    
    std::shared_ptr< ::web::http::http_pipeline> m_pipeline;

    uri _base_uri;
};

} // namespace client
}} // namespace web::http

#endif  /* _CASA_HTTP_CLIENT_H */
