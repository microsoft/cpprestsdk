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
#include <agents.h> // TODO why are these needed???
#endif

#include <algorithm>
#include <thread>
#ifndef _MS_WINDOWS
#include <unistd.h>
#endif

#include <os_utilities.h>

#include "cpprest/uri.h"
#include "test_websocket_server.h"

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

            m_srv.set_reuse_addr(true);

            websocketpp::lib::error_code ec;
            m_srv.listen(WEBSOCKETS_TEST_SERVER_PORT, ec);
            if (ec)
            {
                throw std::runtime_error(ec.message());
            }

            m_srv.start_accept();

            m_thread = std::thread([this]()
            {
                m_service.run();
                return 0;
            });
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

}}}}

