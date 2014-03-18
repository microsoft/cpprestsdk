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
* test_websocket_server.cpp -- Defines a test server to handle websocket messages.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#ifdef _MS_WINDOWS
#include <http.h>
#include <agents.h>
#endif
#include <algorithm>

#include "cpprest/uri.h"
#include "test_websocket_server.h"

#include <os_utilities.h>

#pragma warning(push)

#pragma warning(disable:4512)
#pragma warning(disable:4244)
#pragma warning(disable:6011)

#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"

#pragma warning(pop)

using namespace web; 
using namespace utility;
using namespace utility::conversions;

// POCO namespaces
using namespace Poco::Net;

namespace tests { namespace functional { namespace websocket { namespace utilities {

// Handle a HTTP request by upgrading the connection to websocket connection
// and receive and send to websocket messages
class WebSocketRequestHandler: public HTTPRequestHandler 
{
public:

    WebSocketRequestHandler(test_websocket_server* test_srv) { m_testserver = test_srv; }
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response);

private:
    test_websocket_msg get_test_msg(unsigned char* buffer, int flags, int n)
    {
        test_websocket_msg msg;
        msg.set_data(std::vector<unsigned char>(buffer, buffer + n));

        switch(flags)
        {
        case WebSocket::FRAME_TEXT:
            msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
            break;
        case WebSocket::FRAME_BINARY:
            msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE);
            break;
        }
        return msg;
    }

    test_websocket_server* m_testserver;
};

// Factoy for HTTP request handlers
// POCO HTTP server will be initialized with this factory.
// Any requests received by the server will be provided to the request handler created by this factory
class RequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
{
public:
    RequestHandlerFactory(WebSocketRequestHandler* ws_handler) { m_ws_handler = ws_handler; }

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request);

private:

    WebSocketRequestHandler* m_ws_handler;
};

// POCO test websocket server.
class _test_websocket_server
{
public:
    // Create RequestHandlerFactory (to handle HTTP requests) and WebSocketRequestHandler to handle websocket requests. 
    // The POCO HTTPServer will manage these objects and ensure they are destroyed upon server exit.
    _test_websocket_server(test_websocket_server* test_srv): m_svs(9980), m_srv(new RequestHandlerFactory(new WebSocketRequestHandler(test_srv)), m_svs, new Poco::Net::HTTPServerParams)
    {
        m_srv.start();
    }

    ~_test_websocket_server() 
    {
        m_srv.stopAll();
        m_svs.close();
    }

    void send_msg(const test_websocket_msg& msg);

    void set_websocket(std::shared_ptr<Poco::Net::WebSocket> ws)
    {
        m_ws = ws;
        m_server_connected.set();
    }

private:
    Poco::Net::ServerSocket m_svs;
    Poco::Net::HTTPServer m_srv;
    // Once the HTTP connection is upgraded to websocket connection, 
    // store the websocket here so that the tests can send messages from the server.
    std::shared_ptr<Poco::Net::WebSocket> m_ws;
    // Once the WebSocket object has been initialized and saved to the m_ws pointer above,
    // the below event wil be used to signal that the server has been initialized.
    // The server can now send messages to the client.
	Concurrency::event m_server_connected;
};

void WebSocketRequestHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    try
    {
        std::shared_ptr<WebSocket> ws = std::make_shared<WebSocket>(request, response);
        Poco::Timespan ts = Poco::Timespan::MINUTES * 2;
        ws->setReceiveTimeout(ts);
        m_testserver->get_impl()->set_websocket(ws);
        unsigned char buffer[1024];
        int flags;
        int bytes_received;
        do
        {
            // If the frame's payload is larger than the provided buffer, a WebSocketException is thrown and the WebSocket connection must be terminated.
            bytes_received = ws->receiveFrame(buffer, sizeof(buffer), flags);
            if((flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE)
            {
                // call user callback/handler if one exists and then continue the loop
                if(m_testserver->handler_exists())
                {
                    auto in_msg = get_test_msg(buffer, flags, bytes_received);
                    m_testserver->get_next_message_handler()(in_msg);
                }
            }
        }
        while (((flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE) && bytes_received > 0);
    }
    catch (const WebSocketException& exc)
    {
        // For any errors with HTTP connect, respond to the request. 
        // For any websocket error, exiting this function will destroy the socket.
        switch (exc.code())
        {
        case WebSocket::WS_ERR_HANDSHAKE_UNSUPPORTED_VERSION:
            response.set("Sec-WebSocket-Version", WebSocket::WEBSOCKET_VERSION);
            // fallthrough
        case WebSocket::WS_ERR_NO_HANDSHAKE:
        case WebSocket::WS_ERR_HANDSHAKE_NO_VERSION:
        case WebSocket::WS_ERR_HANDSHAKE_NO_KEY:
            response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
            response.setContentLength(0);
            response.send();
            break;
        }
    }
}

HTTPRequestHandler* RequestHandlerFactory::createRequestHandler(const HTTPServerRequest& request)
{
    VERIFY_ARE_EQUAL(request.getURI(), "/ws");
    return m_ws_handler;
}

test_websocket_server::test_websocket_server()
{
    m_p_impl = std::make_shared<_test_websocket_server>(this);
}

void test_websocket_server::next_message(std::function<void(test_websocket_msg)> handler)
{
    m_handler_queue.push(handler);
}

std::function<void(test_websocket_msg)> test_websocket_server::get_next_message_handler()
{
    auto handler = m_handler_queue.front();
    m_handler_queue.pop();
    return handler;
}

void test_websocket_server::send_msg(const test_websocket_msg& msg)
{
    m_p_impl->send_msg(msg);
}

std::shared_ptr<_test_websocket_server> test_websocket_server::get_impl()
{
    return m_p_impl;
}

void _test_websocket_server::send_msg(const test_websocket_msg& msg)
{
    // Wait for the websocket server to be initialized.
    m_server_connected.wait();
    const auto& data = msg.data();
    int flags = 0;
    switch(msg.msg_type())
    {
    case test_websocket_message_type::WEB_SOCKET_UTF8_FRAGMENT_TYPE:
        flags = WebSocket::FRAME_OP_CONT | WebSocket::FRAME_OP_TEXT;
        break;
    case test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE:
        flags = WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_TEXT;
        break;
    case test_websocket_message_type::WEB_SOCKET_BINARY_FRAGMENT_TYPE:
        flags = WebSocket::FRAME_OP_CONT | WebSocket::FRAME_OP_BINARY;
        break;
    case test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE:
        flags = WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_BINARY;
        break;
    case test_websocket_message_type::WEB_SOCKET_CLOSE_TYPE:
        flags = WebSocket::FRAME_OP_CLOSE;
        break;
    }
    if (msg.msg_type() == test_websocket_message_type::WEB_SOCKET_CLOSE_TYPE)
    {
        m_ws->shutdown();
    }
    else if (data.size() == 0)
    {
        m_ws->sendFrame(nullptr, static_cast<int>(data.size()), flags);
    }
    else
    {
        m_ws->sendFrame(&data[0], static_cast<int>(data.size()), flags);
    }
}

}}}}