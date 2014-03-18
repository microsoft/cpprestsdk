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

using namespace Windows::Foundation;
using namespace Windows::System::Threading;

using namespace web;
using namespace web::experimental::web_sockets;
using namespace web::experimental::web_sockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(client_construction)
{

// Helper function verifies that when constructing a websocket_client with invalid
// URI std::invalid_argument is thrown.
static void verify_client_invalid_argument(const uri &address)
{
    try
    {
        websocket_client client(address);
        VERIFY_IS_TRUE(false);
    } catch(std::invalid_argument &)
    {
        // expected
    }
}

TEST_FIXTURE(uri_address, client_construction_error_cases)
{
    uri address(U("notws://localhost:34567/"));

    // Invalid scheme.
    verify_client_invalid_argument(address);
    
    // empty host.
    address = uri(U("ws://:34567/"));
    verify_client_invalid_argument(address);
}

// Verify that we can read the config from the websocket_client
TEST_FIXTURE(uri_address, get_client_config)
{
    websocket_client_config config;
    
    config.set_message_type(websocket_message_type::binary_message);
    websocket_client client(m_uri, config);

    const websocket_client_config& config2 = client.config();
    VERIFY_ARE_EQUAL(config2.message_type(), websocket_message_type::binary_message);
}

// Verify that we can get the baseuri from websocket_client constructors
TEST_FIXTURE(uri_address, uri_test)
{
    websocket_client client1(m_uri);
    VERIFY_ARE_EQUAL(client1.uri(), m_uri);

    websocket_client_config config;
    websocket_client client2(m_uri, config);
    VERIFY_ARE_EQUAL(client2.uri(), m_uri);
}

TEST_FIXTURE(uri_address, move_operations)
{
    std::string body("hello");
    std::vector<unsigned char> body_vec(body.begin(), body.end());

    test_websocket_server server;
    websocket_client client(m_uri);

    client.connect().wait();

    // Move constructor
    websocket_client client2 = std::move(client);

    server.next_message([&](test_websocket_msg msg) // Handler to verify the message sent by the client.
    {
        websocket_asserts::assert_message_equals(msg, body, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    });

    websocket_outgoing_message msg;
    msg.set_utf8_message(body);
    client2.send(std::move(msg)).wait();

    auto t = client2.receive().then([&](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();

        VERIFY_ARE_EQUAL(ret_msg.length(), body.length());
        VERIFY_ARE_EQUAL(body.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });
    
    test_websocket_msg rmsg;
    rmsg.set_data(body_vec);
    rmsg.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    server.send_msg(rmsg);
    t.wait();

    // Move assignment
    client = std::move(client2);
    server.next_message([&](test_websocket_msg msg) // Handler to verify the message sent by the client.
    {
        websocket_asserts::assert_message_equals(msg, body, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    });

    websocket_outgoing_message msg1;
    msg1.set_utf8_message(body);
    client.send(std::move(msg1)).wait();

    test_websocket_msg rmsg1;
    rmsg1.set_data(body_vec);
    rmsg1.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    server.send_msg(rmsg1);
    auto t1 = client.receive().then([&](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();

        VERIFY_ARE_EQUAL(ret_msg.length(), body.length());
        VERIFY_ARE_EQUAL(body.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });
    t1.wait();
    client.close().wait();
}

} // SUITE(client_construction)

}}}}