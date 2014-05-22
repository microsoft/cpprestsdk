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
#include <thread>
#ifndef _MS_WINDOWS
#include <unistd.h>
#endif

#include <os_utilities.h>

#include "cpprest/uri.h"
#include "test_websocket_server.h"

#if defined(__cplusplus_winrt)

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
#include "Poco/Net/HTTPBasicCredentials.h"

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

    test_http_request get_http_request_msg(HTTPServerRequest& request)
    {
        test_http_request msg;
        if (request.hasCredentials())
        {
            HTTPBasicCredentials cred(request);
            msg.set_username(cred.getUsername());
            msg.set_password(cred.getPassword());
        }
        for(auto iter = request.begin(); iter != request.end(); iter++)
        {
            msg.add_header(iter->first, iter->second); 
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
    RequestHandlerFactory(test_websocket_server* testserver) { m_testserver = testserver; }

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request);

private:

    test_websocket_server* m_testserver;
};

// POCO test websocket server.
class _test_websocket_server
{
public:
    // Create RequestHandlerFactory (to handle HTTP requests) and WebSocketRequestHandler to handle websocket requests. 
    // The POCO HTTPServer will manage these objects and ensure they are destroyed upon server exit.
    _test_websocket_server(test_websocket_server* test_srv): m_svs(9980), m_srv(new RequestHandlerFactory(test_srv), m_svs, new Poco::Net::HTTPServerParams)
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
    // If a handler exists to handle the HTTP handshake request, call the same.
    if (m_testserver->get_http_handler())
    {
        test_http_request req = get_http_request_msg(request);
        test_http_response resp = m_testserver->get_http_handler()(req);
        if (resp.status_code() != 200) // send an error code in the HTTP response else continue with the websocket connection
        {
            if (!resp.realm().empty())
            {
                // Set the realm
                std::string auth("Basic realm=\"");
                auth.append(resp.realm());
                auth.append("\"");
                response.set("WWW-Authenticate", auth);
            }
            response.setStatusAndReason((HTTPResponse::HTTPStatus)resp.status_code());
            response.send();
            return;
        }
    }

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
    return new WebSocketRequestHandler(m_testserver);
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

#else /* defined(WINAPI_FAMILY) && WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP */

#ifdef _WIN32
#pragma warning( disable : 4503 )
#pragma warning( push )
#pragma warning( disable : 4100 4127 4996 4512 4996 4267 4067 )
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#define _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_INITIALIZER_LISTS_
#define _WEBSOCKETPP_NOEXCEPT_TOKEN_
#define _WEBSOCKETPP_CONSTEXPR_TOKEN_
#else
#define _WEBSOCKETPP_NULLPTR_TOKEN_ 0
#endif
#endif /* _WIN32 */

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#ifdef _WIN32
#pragma warning( pop )
#endif

using namespace web;
using namespace utility;
using namespace utility::conversions;

#define WEBSOCKETS_TEST_SERVER_PORT 9980

// Websocketpp typedefs
typedef websocketpp::server<websocketpp::config::asio> server;

namespace tests {
namespace functional {
namespace websocket {
namespace utilities {

    class _test_websocket_server
    {
    public:
        _test_websocket_server(test_websocket_server* test_srv)
            : m_test_srv(test_srv), m_work(new boost::asio::io_service::work(m_service))
        {
            m_srv.clear_access_channels(websocketpp::log::alevel::all);
            m_srv.clear_error_channels(websocketpp::log::elevel::all);
            connect();
        }

        void connect()
        {
            m_srv.set_open_handler([this](websocketpp::connection_hdl hdl)
            {
                m_con = hdl;
                m_server_connected.set();
            });

            m_srv.set_fail_handler([this](websocketpp::connection_hdl hdl)
            {
                m_con = hdl;
                m_server_connected.set_exception(std::runtime_error("Connection attempt failed."));
            });

            m_srv.set_message_handler([this](websocketpp::connection_hdl hdl, server::message_ptr msg)
            {
                auto pay = msg->get_payload();

                auto fn = m_test_srv->get_next_message_handler();

                test_websocket_msg wsmsg;

                std::vector<unsigned char> data(pay.begin(), pay.end());

                wsmsg.set_data(data);
                switch (msg->get_opcode())
                {
                case websocketpp::frame::opcode::binary:
                    wsmsg.set_msg_type(utilities::WEB_SOCKET_BINARY_MESSAGE_TYPE);
                    break;
                case websocketpp::frame::opcode::text:
                    wsmsg.set_msg_type(utilities::WEB_SOCKET_UTF8_MESSAGE_TYPE);
                    break;
                case websocketpp::frame::opcode::close:
                    wsmsg.set_msg_type(utilities::WEB_SOCKET_CLOSE_TYPE);
                    break;
                default:
                    // Websocketspp does not currently support explicit fragmentation. We should not get here.
                    std::abort();
                }

                fn(wsmsg);
            });

            m_srv.init_asio(&m_service);

            m_thread = std::thread([this]()
            {
                m_service.run();
                return 0;
            });

            m_srv.set_reuse_addr(true);

            websocketpp::lib::error_code ec;
            m_srv.listen(WEBSOCKETS_TEST_SERVER_PORT, ec);
            if (ec)
            {
                std::abort();
            }
            m_srv.start_accept();
        }

        ~_test_websocket_server()
        {
            close("destructor");

            m_work.reset();
            m_service.stop();
            _ASSERTE(m_thread.joinable());
            m_thread.join();
        }

        void send_msg(const test_websocket_msg& msg);


        void close(const std::string& reasoning)
        {
            websocketpp::lib::error_code ec;
            m_srv.close(m_con, websocketpp::close::status::going_away, reasoning, ec);
            // Ignore the error code.
        }

    private:

        test_websocket_server* m_test_srv;

        boost::asio::io_service m_service;
        std::unique_ptr<boost::asio::io_service::work> m_work;
        std::thread m_thread;

        server m_srv;
        websocketpp::connection_hdl m_con;
        // Once the WebSocket object has been initialized,
        // the below event wil be used to signal that the server has been initialized.
        // The server can now send messages to the client.
        pplx::task_completion_event<void> m_server_connected;
    };

    test_websocket_server::test_websocket_server()
        : m_p_impl(std::make_shared<_test_websocket_server>(this))
    { }

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
        pplx::task<void>(m_server_connected).wait();
        const auto& data = msg.data();
        auto flags = websocketpp::frame::opcode::close;
        switch (msg.msg_type())
        {
        case test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE:
            flags = websocketpp::frame::opcode::text; // WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_TEXT;
            break;
        case test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE:
            flags = websocketpp::frame::opcode::binary; // WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_BINARY;
            break;
        case test_websocket_message_type::WEB_SOCKET_CLOSE_TYPE:
            flags = websocketpp::frame::opcode::close; // WebSocket::FRAME_OP_CLOSE;
            break;
        case test_websocket_message_type::WEB_SOCKET_UTF8_FRAGMENT_TYPE:
        case test_websocket_message_type::WEB_SOCKET_BINARY_FRAGMENT_TYPE:
        default:
            throw std::runtime_error("invalid message type");
        }

        std::string strmsg(data.begin(), data.end());

        if (msg.msg_type() == test_websocket_message_type::WEB_SOCKET_CLOSE_TYPE)
        {
            close(strmsg);
        }
        else
        {
            // std::cerr << "Sending message from server: " << strmsg << std::endl;
            m_srv.send(m_con, strmsg, flags);
        }
    }

}
}
}
}

#endif /* defined(WINAPI_FAMILY) && WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP */
