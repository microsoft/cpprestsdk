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
* receive_msg_tests.cpp
*
* Test cases covering receiving messages from websocket server.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace concurrency;
using namespace concurrency::streams;

using namespace web::experimental::web_sockets;
using namespace web::experimental::web_sockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(receive_msg_tests)
{

pplx::task<void> receive_text_msg_helper(websocket_client& client, 
                                         test_websocket_server& server, 
                                         const std::string& body_str, 
                                         bool connect_client = true)
{
    std::vector<unsigned char> body(body_str.begin(), body_str.end());

    if (connect_client)
        client.connect().wait();

    auto t = client.receive().then([body_str](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();

        VERIFY_ARE_EQUAL(ret_msg.length(), body_str.length());
        VERIFY_ARE_EQUAL(body_str.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });
    
    test_websocket_msg msg;
    msg.set_data(std::move(body));
    msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    server.send_msg(msg);

    return t;
}

pplx::task<void> receive_msg_stream_helper(websocket_client& client, 
                                          test_websocket_server& server, 
                                          const std::vector<unsigned char>& body, 
                                          test_websocket_message_type type,
                                          bool connect_client = true)
{
    if (connect_client)
        client.connect().wait();

    auto t = client.receive().then([body, type](websocket_incoming_message ret_msg)
    {
        auto is = ret_msg.body();
        streams::container_buffer<std::vector<uint8_t>> ret_data;
        is.read_to_end(ret_data).wait();

        VERIFY_ARE_EQUAL(ret_msg.length(), body.size());
        VERIFY_IS_TRUE(body == ret_data.collection());
        if (type == test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE)
            VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::binary_message);
        else if (type == test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE)
            VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });
    
    test_websocket_msg msg;
    msg.set_data(std::move(body));
    msg.set_msg_type(type);
    server.send_msg(msg);

    return t;
}

// Receive text message (no fragmentation)
TEST_FIXTURE(uri_address, receive_text_msg)
{    
    test_websocket_server server;
    websocket_client client(m_uri);

    receive_text_msg_helper(client, server, "hello").wait();
    client.close().wait();
}

// Receive text message (no fragmentation)
// Test the stream interface to read data
TEST_FIXTURE(uri_address, receive_text_msg_stream)
{
    std::string body_str("hello");
    std::vector<unsigned char> body(body_str.begin(), body_str.end());
    test_websocket_server server;
    websocket_client client(m_uri);

    receive_msg_stream_helper(client, server, body, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE).wait();
    client.close().wait();
}

// Receive binary message (no fragmentation)
TEST_FIXTURE(uri_address, receive_binary_msg)
{
    std::vector<uint8_t> body;
    body.resize(6);
    memcpy(&body[0], "a\0b\0c\0", 6);

    test_websocket_server server;

    websocket_client_config config;
    config.set_message_type(websocket_message_type::binary_message);
    websocket_client client(m_uri, config);

    receive_msg_stream_helper(client, server, body, test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE).wait();
    client.close().wait();
}

// Server sends text message fragmented in 2 fragments
TEST_FIXTURE(uri_address, receive_text_msg_fragments, "Ignore", "898451")
{
    std::string body_str("hello");
    std::vector<unsigned char> body(body_str.begin(), body_str.end());
    test_websocket_server server;

    websocket_client client(m_uri);

    client.connect().wait();

    auto t = client.receive().then([&](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();

        VERIFY_ARE_EQUAL(body_str.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });

    test_websocket_msg msg1;
    msg1.set_data(std::move(body));
    msg1.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_FRAGMENT_TYPE);
    server.send_msg(msg1);

    test_websocket_msg msg2;
    msg2.set_data(std::move(body));
    msg2.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_FRAGMENT_TYPE);
    server.send_msg(msg2);

    t.wait();
    client.close().wait();
}

// Server sends message of length 0
TEST_FIXTURE(uri_address, receive_zero_length_msg)
{
    test_websocket_server server;
    websocket_client client(m_uri);

    receive_text_msg_helper(client, server, "").wait();

    client.close().wait();
}

// Receive UTF-8 string with special characters
TEST_FIXTURE(uri_address, receive_multi_byte_utf8_msg)
{
    utf16string utf16str(U("аш"));
    auto body_str = utility::conversions::to_utf8string(utf16str);
    test_websocket_server server;
    websocket_client client(m_uri);

    receive_text_msg_helper(client, server, body_str).wait();

    client.close().wait();
}

// Receive multiple messages
TEST_FIXTURE(uri_address, receive_multiple_msges)
{
    test_websocket_server server;
    websocket_client client(m_uri);

    auto t1 = receive_text_msg_helper(client, server, "hello1");
    auto t2 = receive_text_msg_helper(client, server, "hello2", false);

    t1.wait();
    t2.wait();

    client.close().wait();
}

// Start the receive task after the server has sent a message
TEST_FIXTURE(uri_address, receive_after_server_send)
{
    std::string body_str("hello");
    std::vector<unsigned char> body(body_str.begin(), body_str.end());
    
    test_websocket_server server;

    websocket_client client(m_uri);

    client.connect().wait();
    
    test_websocket_msg msg;
    msg.set_data(std::move(body));
    msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    server.send_msg(msg);

    // We dont have a way of knowing if the message has been received by our client. 
    // Hence Sleep for 5 secs and then initiate the receive
    std::chrono::milliseconds dura(5000);
    std::this_thread::sleep_for(dura);

    client.receive().then([&](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();
        VERIFY_ARE_EQUAL(body_str.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    }).wait();

    client.close().wait();
}

// Start task to receive text message before connecting.
TEST_FIXTURE(uri_address, receive_before_connect)
{
    test_websocket_server server;
    websocket_client client(m_uri);

    std::string body_str("hello");
    std::vector<unsigned char> body(body_str.begin(), body_str.end());

    auto t = client.receive().then([body_str](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();

        VERIFY_ARE_EQUAL(ret_msg.length(), body_str.length());
        VERIFY_ARE_EQUAL(body_str.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });
    
    // Connect after the client is waiting on a receive task.
    client.connect().wait();

    // Now send the message from the server
    test_websocket_msg msg;
    msg.set_data(std::move(body));
    msg.set_msg_type(test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    server.send_msg(msg);

    t.wait();
    client.close().wait();
}
} // SUITE(receive_msg_tests)

}}}}