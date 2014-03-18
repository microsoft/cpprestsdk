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
* close_tests.cpp
*
* Tests cases for closing websocket_client objects.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace concurrency::streams;

using namespace web::experimental::web_sockets;
using namespace web::experimental::web_sockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(close_tests)
{

// Test close websocket connection: client sends an empty close and server responds with close frame
TEST_FIXTURE(uri_address, close_client_websocket)
{
    test_websocket_server server;
    
    websocket_client client(m_uri);

    client.connect().wait();

    client.close().wait();
}

// Test close websocket connection: client sends a close with reason and server responds with close frame
TEST_FIXTURE(uri_address, close_with_reason)
{
    test_websocket_server server;
    
    websocket_client client(m_uri);

    client.connect().wait();

    client.close(websocket_close_status::going_away, U("Client disconnecting")).wait();
}

// Server sends a close frame (server initiated close)
TEST_FIXTURE(uri_address, close_from_server)
{
    std::string body("hello");
    test_websocket_server server;

    websocket_client client(m_uri);

    client.connect().wait();

    // Send close frame from server
    test_websocket_msg msg;
    msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_CLOSE_TYPE);
    server.send_msg(msg);

    client.close().wait();
}

} // SUITE(close_tests)

}}}}