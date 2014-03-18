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
* client_construction.cpp
*
* Tests cases for covering creating http_clients.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace concurrency::streams;

using namespace web::experimental::web_sockets;
using namespace web::experimental::web_sockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(error_tests)
{

// Send before connecting
TEST_FIXTURE(uri_address, send_before_connect)
{
    websocket_client client(m_uri);

    websocket_outgoing_message msg;
    msg.set_utf8_message("xyz");
    
    VERIFY_THROWS(client.send(msg).wait(), websocket_exception);
}

// Server does not exist
TEST_FIXTURE(uri_address, server_doesnt_exist)
{
    websocket_client client(m_uri);
    VERIFY_THROWS(client.connect().get(), websocket_exception);
}

// Send after close
TEST_FIXTURE(uri_address, send_after_close, "Ignore", "902663")
{
    std::string body("hello");
    test_websocket_server server;

    server.next_message([&](test_websocket_msg msg)
    {
        websocket_asserts::assert_message_equals(msg, body, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    });
    websocket_client client(m_uri);

    client.connect().wait();
    client.close().wait();
    
    websocket_outgoing_message msg;
    msg.set_utf8_message(body);
    VERIFY_THROWS(client.send(msg).wait(), websocket_exception);
}

// Receive after close
TEST_FIXTURE(uri_address, receive_after_close)
{
    test_websocket_server server;
    websocket_client client(m_uri);
    client.connect().wait();
    auto t = client.receive();
    client.close().wait();
    VERIFY_THROWS(t.wait(), websocket_exception);
}

// Start receive task after client has closed
TEST_FIXTURE(uri_address, try_receive_after_close)
{
    test_websocket_server server;
    websocket_client client(m_uri);
    client.connect().wait();
    client.close().wait();
    auto t = client.receive();
    VERIFY_THROWS(t.wait(), websocket_exception);
}

// Start the receive task after server has sent a close frame
TEST_FIXTURE(uri_address, try_receive_after_server_initiated_close)
{
    test_websocket_server server;
    websocket_client client(m_uri);
    client.connect().wait();

    // Send close frame from server
    test_websocket_msg msg;
    msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_CLOSE_TYPE);
    server.send_msg(msg);

    std::chrono::milliseconds dura(3000);
    std::this_thread::sleep_for(dura);

    auto t = client.receive();
    VERIFY_THROWS(t.wait(), websocket_exception);

    client.close().wait();
}

// Destroy the client without closing it explicitly
TEST_FIXTURE(uri_address, destroy_without_close)
{
    test_websocket_server server;
    pplx::task<websocket_incoming_message> t;

    {
        websocket_client client(m_uri);
        client.connect().wait();

        t = client.receive();
    }
    
    VERIFY_THROWS(t.wait(), websocket_exception);
}

// connect fails while user is waiting on receive
TEST_FIXTURE(uri_address, connect_fail_with_receive)
{
    websocket_client client(U("ws://localhost:9981/ws"));
    auto t = client.receive();

    VERIFY_THROWS(client.connect().get(), websocket_exception);
    VERIFY_THROWS(t.get(), websocket_exception);
}
} // SUITE(error_tests)

}}}}