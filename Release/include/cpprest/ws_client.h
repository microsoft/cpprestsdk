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
#if WINAPI_FAMILY == WINAPI_FAMILY_APP

#ifndef _CASA_WS_CLIENT_H
#define _CASA_WS_CLIENT_H

#include <memory>
#include <limits>
#include <condition_variable>
#include <mutex>

#include "cpprest/xxpublic.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else 
#include "pplx/pplxtasks.h"
#endif

#include "cpprest/uri.h"
#include "cpprest/basic_types.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/ws_msg.h"

namespace web 
{
namespace experimental 
{
namespace web_sockets
{
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
    websocket_client_config()
#ifdef __cplusplus_winrt
        :m_msg_type(websocket_message_type::text_message)
#endif
    {}

#ifdef __cplusplus_winrt
    /// <summary>
    /// Gets the websocket message type.
    /// Note: This property exists only for WinRT implementation. 
    /// A non store client must be able to send both binary and text message.
    /// </summary>
    websocket_message_type message_type() const
    {
        return m_msg_type;
    }

    /// <summary>
    /// Sets the websocket message type. 
    /// </summary>
    void set_message_type(const websocket_message_type type)
    {
        m_msg_type = type;
    }
#endif

private:
#ifdef __cplusplus_winrt
    websocket_message_type m_msg_type;
#endif
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

    ~websocket_exception() _noexcept {}

    const char* what() const _noexcept
    {
        return m_msg.c_str();
    }

    const std::error_code & error_code() const
    {
        return m_errorCode;
    }

private:
    std::string m_msg;
    std::error_code m_errorCode;
};

namespace details
{
class winrt_client;

// Interface to be implemented by the websocket client implementations.
class _websocket_client_impl
{
public:
    virtual ~_websocket_client_impl() _noexcept {}

    virtual pplx::task<void> connect() = 0;

    virtual pplx::task<void> send(websocket_outgoing_message msg) = 0;

    virtual pplx::task<websocket_incoming_message> receive() = 0; 

    virtual pplx::task<void> close() = 0;

    virtual pplx::task<void> close(websocket_close_status close_status, const utility::string_t &close_reason=_XPLATSTR("")) = 0;

    /// <summary>
    /// Gets the base uri
    /// </summary>
    /// <returns>
    /// A base uri initialized in constructor
    /// </return>
    const web::uri& uri() const
    {
        return m_uri;
    }

    /// <summary>
    /// Get client configuration object
    /// </summary>
    /// <returns>A reference to the client configuration object.</returns>
    const websocket_client_config& config() const
    {
        return m_client_config;
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
    _websocket_client_impl(web::uri address, websocket_client_config client_config)
        : m_uri(std::move(address)), m_client_config(std::move(client_config))
    {
    }

    web::uri m_uri;
    websocket_client_config m_client_config;    
};
}

/// <summary>
/// Websocket client class, used to maintain a connection to a remote host for an extended session.
/// </summary>
class websocket_client
{
public:
    /// <summary>
    /// Creates a new websocket_client to connect to the specified uri.
    /// </summary>
    /// <param name="base_uri">A string representation of the base uri to be used for all messages. Must start with either "ws://" or "wss://"</param>
    _ASYNCRTIMP websocket_client(web::uri base_uri);

    /// <summary>
    /// Creates a new websocket_client to connect to the specified uri.
    /// </summary>
    /// <param name="base_uri">A string representation of the base uri to be used for all requests. Must start with either "ws://" or "wss://"</param>
    /// <param name="client_config">The client configuration object containing the possible configuration options to intitialize the <c>websocket_client</c>. </param>
    _ASYNCRTIMP websocket_client(web::uri base_uri, websocket_client_config client_config);

    /// <summary>
    /// </summary>
    ~websocket_client() { }

    /// <summary>
    /// Move constructor.
    /// </summary>
    websocket_client(websocket_client &&other) 
        : m_client(std::move(other.m_client))
    {
    }

    /// <summary>
    /// Move assignment operator.
    /// </summary>
    websocket_client &operator=(websocket_client &&other)
    {
        if(this != &other)
        {
            m_client = std::move(other.m_client);
        }
        return *this;
    }

    /// <summary>
    /// Connects to the remote network destination. The connect method initiates the websocket handshake with the 
    /// remote network destination, takes care of the protocol upgrade request.
    /// </summary>
    /// <returns>An asynchronous operation that is completed once the client has successfully connected to the websocket server.</returns>
    pplx::task<void> connect() { return m_client->connect(); }

    /// <summary>
    /// Sends a websocket message to the server .
    /// </summary>
    /// <returns>An asynchronous operation that is completed once the message is sent.</returns>
    pplx::task<void> send(websocket_outgoing_message msg) { return m_client->send(msg); } 

    /// <summary>
    /// Receive a websocket message.
    /// </summary>
    /// <returns>An asynchronous operation that is completed when a message has been received by the client endpoint.</returns>
    pplx::task<websocket_incoming_message> receive() { return m_client->receive(); }

    /// <summary>
    /// Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server.
    /// </summary>
    /// <returns>An asynchronous operation that is completed the connection has been successfully closed.</returns>
    pplx::task<void> close() { return m_client->close(); }

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
    /// <returns>
    /// A base uri initialized in constructor
    /// </return>
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

#endif  /* _CASA_WS_CLIENT_H */

#endif /* WINAPI_FAMILY == WINAPI_FAMILY_APP */
