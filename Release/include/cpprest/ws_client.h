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
* ws_client.h
*
* Websocket client side implementation
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _CASA_WS_CLIENT_H
#define _CASA_WS_CLIENT_H

#if defined(__cplusplus_winrt) || !defined(_M_ARM)

#include <memory>
#include <limits>
#include <condition_variable>
#include <mutex>

#include "cpprest/xxpublic.h"

#if _NOT_PHONE8_
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else
#include "pplx/pplxtasks.h"
#endif

#include "cpprest/uri.h"
#include "cpprest/web_utilities.h"
#include "cpprest/http_headers.h"
#include "cpprest/basic_types.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/ws_msg.h"

namespace web
{
/// WebSocket client is currently in beta.
namespace experimental
{
// In the past namespace was accidentally called 'web_sockets'. To avoid breaking code
// alias it. At our next major release this should be deleted.
namespace web_sockets = websockets;
namespace websockets
{
/// WebSocket client side library.
namespace client
{

enum class websocket_close_status
{
    normal = 1000,
    going_away = 1001,
    protocol_error = 1002,
    unsupported = 1003, //or data_mismatch
    inconsistent_datatype = 1007,
    policy_violation = 1008,
    too_large = 1009,
    negotiate_error = 1010,
    server_terminate = 1011,
};

/// <summary>
/// Websocket client configuration class, used to set the possible configuration options
/// used to create an websocket_client instance.
/// </summary>
class websocket_client_config
{
public:

    /// <summary>
    /// Creates a websocket client configuration with default settings.
    /// </summary>
    websocket_client_config() {}

    /// <summary>
    /// Copy constructor.
    /// </summary>
    websocket_client_config(const websocket_client_config &other) :
        m_proxy(other.m_proxy),
        m_credentials(other.m_credentials),
        m_headers(other.m_headers)
    {}

    /// <summary>
    /// Move constructor.
    /// </summary>
    websocket_client_config(websocket_client_config &&other) :
        m_proxy(std::move(other.m_proxy)),
        m_credentials(std::move(other.m_credentials)),
        m_headers(std::move(other.m_headers))
    {}

    /// <summary>
    /// Assignment operator.
    /// </summary>
    websocket_client_config &operator=(const websocket_client_config &other)
    {
        if (this != &other)
        {
            m_proxy = other.m_proxy;
            m_credentials = other.m_credentials;
            m_headers = other.m_headers;
        }
        return *this;
    }

    /// <summary>
    /// Move assignment operator.
    /// </summary>
    websocket_client_config &operator=(websocket_client_config &&other)
    {
        if (this != &other)
        {
            m_proxy = std::move(other.m_proxy);
            m_credentials = std::move(other.m_credentials);
            m_headers = std::move(other.m_headers);
        }
        return  *this;
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
    /// <param name="proxy">The web proxy object.</param>
    void set_proxy(const web_proxy &proxy)
    {
        m_proxy = proxy;
    }

    /// <summary>
    /// Get the client credentials
    /// </summary>
    /// <returns>A reference to the client credentials.</returns>
    const web::credentials& credentials() const
    {
        return m_credentials;
    }

    /// <summary>
    /// Set the client credentials
    /// </summary>
    /// <param name="cred">The client credentials.</param>
    void set_credentials(const web::credentials &cred)
    {
        m_credentials = cred;
    }

    /// <summary>
    /// Gets the headers of the HTTP request message used in the WebSocket protocol handshake.
    /// </summary>
    /// <returns>HTTP headers for the WebSocket protocol handshake.</returns>
    /// <remarks>
    /// Use the <seealso cref="http_headers::add Method"/> to fill in desired headers.
    /// </remarks>
    web::http::http_headers &headers() { return m_headers; }

    /// <summary>
    /// Gets a const reference to the headers of the WebSocket protocol handshake HTTP message.
    /// </summary>
    /// <returns>HTTP headers.</returns>
    const web::http::http_headers &headers() const { return m_headers; }

    /// <summary>
    /// Adds a subprotocol to the request headers.
    /// </summary>
    /// <param name="name">The name of the subprotocol.</param>
    /// <remark>If additional subprotocols have already been specified, the new one will just be added.</remarks>
    _ASYNCRTIMP void add_subprotocol(const ::utility::string_t &name);

    /// <summary>
    /// Gets list of the specified subprotocols.
    /// </summary>
    /// <returns>Vector of all the subprotocols </returns>
    /// <remarks>If you want all the subprotocols in a comma separated string
    /// they can be directly looked up in the headers using 'Sec-WebSocket-Protocol'.</remarks>
    _ASYNCRTIMP std::vector<::utility::string_t> subprotocols() const;

private:
    web::web_proxy m_proxy;
    web::credentials m_credentials;
    web::http::http_headers m_headers;
};

/// <summary>
/// Represents a websocket error. This class holds an error message and an optional error code.
/// </summary>
class websocket_exception : public std::exception
{
public:

    /// <summary>
    /// Creates an <c>websocket_exception</c> with just a string message and no error code.
    /// </summary>
    /// <param name="whatArg">Error message string.</param>
    websocket_exception(const utility::string_t &whatArg)
        : m_msg(utility::conversions::to_utf8string(whatArg)) {}

#ifdef _MS_WINDOWS
    /// <summary>
    /// Creates an <c>websocket_exception</c> with just a string message and no error code.
    /// </summary>
    /// <param name="whatArg">Error message string.</param>
    websocket_exception(std::string whatArg) : m_msg(std::move(whatArg)) {}
#endif

    /// <summary>
    /// Creates a <c>websocket_exception</c> from a error code using the current platform error category.
    /// The message of the error code will be used as the what() string message.
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    websocket_exception(int errorCode)
        : m_errorCode(utility::details::create_error_code(errorCode))
    {
        m_msg = m_errorCode.message();
    }

    /// <summary>
    /// Creates a <c>websocket_exception</c> from a error code using the current platform error category.
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    /// <param name="whatArg">Message to use in what() string.</param>
    websocket_exception(int errorCode, const utility::string_t &whatArg)
        : m_errorCode(utility::details::create_error_code(errorCode)),
          m_msg(utility::conversions::to_utf8string(whatArg))
    {}

#ifdef _MS_WINDOWS
    /// <summary>
    /// Creates a <c>websocket_exception</c> from a error code and string message.
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    /// <param name="whatArg">Message to use in what() string.</param>
    websocket_exception(int errorCode, std::string whatArg)
        : m_errorCode(utility::details::create_error_code(errorCode)),
        m_msg(std::move(whatArg))
    {}
#endif

    /// <summary>
    /// Creates a <c>websocket_exception</c> from a error code and category. The message of the error code will be used
    /// as the <c>what</c> string message.
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    /// <param name="cat">Error category for the code.</param>
    websocket_exception(int errorCode, const std::error_category &cat) : m_errorCode(std::error_code(errorCode, cat))
    {
        m_msg = m_errorCode.message();
    }

    /// <summary>
    /// Destroys the <c>websocket_exception</c> object.
    /// </summary>
    ~websocket_exception() _noexcept {}

    /// <summary>
    /// Gets a string identifying the cause of the exception.
    /// </summary>
    /// <returns>A null terminated character string.</returns>
    const char* what() const _noexcept
    {
        return m_msg.c_str();
    }

    /// <summary>
    /// Gets the underlying error code for the cause of the exception.
    /// </summary>
    /// <returns>The <c>error_code</c> object associated with the exception.</returns>
    const std::error_code & error_code() const _noexcept
    {
        return m_errorCode;
    }

private:
    std::error_code m_errorCode;
    std::string m_msg;
};

namespace details
{

// Interface to be implemented by the websocket client implementations.
class _websocket_client_impl
{
public:

    _websocket_client_impl(websocket_client_config config) :
        m_config(std::move(config)) {}

    virtual ~_websocket_client_impl() _noexcept {}

    virtual pplx::task<void> connect() = 0;

    virtual pplx::task<void> send(websocket_outgoing_message &msg) = 0;

    virtual pplx::task<websocket_incoming_message> receive() = 0;

    virtual pplx::task<void> close() = 0;

    virtual pplx::task<void> close(websocket_close_status close_status, const utility::string_t &close_reason=_XPLATSTR("")) = 0;

    const web::uri& uri() const
    {
        return m_uri;
    }

    void set_uri(const web::uri &uri)
    {
        m_uri = uri;
    }

    const websocket_client_config& config() const
    {
        return m_config;
    }

    static void verify_uri(const web::uri& uri)
    {
        // Most of the URI schema validation is taken care by URI class.
        // We only need to check certain things specific to websockets.
        if(uri.scheme() != _XPLATSTR("ws") && uri.scheme() != _XPLATSTR("wss"))
        {
            throw std::invalid_argument("URI scheme must be 'ws' or 'wss'");
        }

        if(uri.host().empty())
        {
            throw std::invalid_argument("URI must contain a hostname.");
        }

        // Fragment identifiers are meaningless in the context of WebSocket URIs
        // and MUST NOT be used on these URIs.
        if (!uri.fragment().empty())
        {
            throw std::invalid_argument("WebSocket URI must not contain fragment identifiers");
        }
    }

protected:
    web::uri m_uri;
    websocket_client_config m_config;
};
}

/// <summary>
/// Websocket client class, used to maintain a connection to a remote host for an extended session.
/// </summary>
class websocket_client
{
public:
    /// <summary>
    ///  Creates a new websocket_client.
    /// </summary>
    _ASYNCRTIMP websocket_client();

    /// <summary>
    ///  Creates a new websocket_client.
    /// </summary>
    /// <param name="client_config">The client configuration object containing the possible configuration options to initialize the <c>websocket_client</c>. </param>
    _ASYNCRTIMP websocket_client(websocket_client_config client_config);

    /// <summary>
    /// Destructor
    /// </summary>
    ~websocket_client() _noexcept {}

    /// <summary>
    /// Connects to the remote network destination. The connect method initiates the websocket handshake with the
    /// remote network destination, takes care of the protocol upgrade request.
    /// </summary>
    /// <param name="uri">The uri address to connect. </param>
    /// <returns>An asynchronous operation that is completed once the client has successfully connected to the websocket server.</returns>
    pplx::task<void> connect(const web::uri &uri)
    {
        m_client->verify_uri(uri);
        m_client->set_uri(uri);
        return m_client->connect();
    }

    /// <summary>
    /// Sends a websocket message to the server .
    /// </summary>
    /// <returns>An asynchronous operation that is completed once the message is sent.</returns>
    pplx::task<void> send(websocket_outgoing_message msg)
    {
        return m_client->send(msg);
    }

    /// <summary>
    /// Receive a websocket message.
    /// </summary>
    /// <returns>An asynchronous operation that is completed when a message has been received by the client endpoint.</returns>
    pplx::task<websocket_incoming_message> receive()
    {
        return m_client->receive();
    }

    /// <summary>
    /// Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server.
    /// </summary>
    /// <returns>An asynchronous operation that is completed the connection has been successfully closed.</returns>
    pplx::task<void> close()
    {
        return m_client->close();
    }

    /// <summary>
    /// Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server.
    /// </summary>
    /// <param name="close_status">Endpoint MAY use the following pre-defined status codes when sending a Close frame.</param>
    /// <param name="close_reason">While closing an established connection, an endpoint may indicate the reason for closure.</param>
    /// <returns>An asynchronous operation that is completed the connection has been successfully closed.</returns>
    pplx::task<void> close(websocket_close_status close_status, const utility::string_t& close_reason=_XPLATSTR(""))
    {
        return m_client->close(close_status, close_reason);
    }

    /// <summary>
    /// Gets the websocket client URI.
    /// </summary>
    /// <returns>URI connected to.</returns>
    const web::uri& uri() const
    {
        return m_client->uri();
    }

    /// <summary>
    /// Gets the websocket client config object.
    /// </summary>
    /// <returns>A reference to the client configuration object.</returns>
    const websocket_client_config& config() const
    {
        return m_client->config();
    }

private:
    std::shared_ptr<details::_websocket_client_impl> m_client;
};

}}}}

#endif  // _NOT_PHONE8_
#endif
#endif  /* _CASA_WS_CLIENT_H */
