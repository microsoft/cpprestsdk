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
* send_msg_tests.cpp
*
* Tests cases for covering sending messages from websocket client.
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

SUITE(send_msg_tests)
{
void fill_buffer(streams::streambuf<uint8_t> rbuf, const std::vector<uint8_t>& body, size_t repetitions = 1)
{
    size_t len = body.size();
    for (size_t i = 0; i < repetitions; i++)
        rbuf.putn((const uint8_t *)&body[0], len).wait();
}

pplx::task<void> send_text_msg_helper(websocket_client& client, test_websocket_server& server, const std::string& body, bool connect_client = true)
{
    server.next_message([body](test_websocket_msg msg) // Handler to verify the message sent by the client.
    {
        websocket_asserts::assert_message_equals(msg, body, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    });

    if (connect_client)
        client.connect().wait();

    websocket_outgoing_message msg;
    msg.set_utf8_message(body);
    return client.send(msg);
}

pplx::task<void> send_msg_from_stream_helper(websocket_client& client,
                                             test_websocket_server& server,
                                             const std::vector<uint8_t>& body, 
                                             streams::producer_consumer_buffer<uint8_t> rbuf,
                                             test_websocket_message_type type,
                                             bool connect_client = true)
{
    server.next_message([body, type](test_websocket_msg msg)
    {
        websocket_asserts::assert_message_equals(msg, body, type);
    });
    if (connect_client)
        client.connect().wait();

    fill_buffer(rbuf, body);

    websocket_outgoing_message msg;
    if (type == test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE)
        msg.set_utf8_message(streams::istream(rbuf), body.size());
    else if (type == test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE)
        msg.set_binary_message(streams::istream(rbuf), body.size());

    return client.send(msg);
}

// Send text message (no fragmentation)
TEST_FIXTURE(uri_address, send_text_msg)
{
    test_websocket_server server;
    websocket_client client(m_uri);
    send_text_msg_helper(client, server, "hello").wait();
    client.close().wait();
}

// Send text message (no fragmentation)
// Test the stream interface to send data
TEST_FIXTURE(uri_address, send_text_msg_stream)
{
    test_websocket_server server;
    streams::producer_consumer_buffer<uint8_t> rbuf;
    std::vector<uint8_t> body(26);
    memcpy(&body[0], "abcdefghijklmnopqrstuvwxyz", 26);

    websocket_client client(m_uri);
    send_msg_from_stream_helper(client, server, body, rbuf, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE).wait();

    rbuf.close(std::ios::out).wait();
    client.close().wait();
}

// Send Binary message (no fragmentation)
TEST_FIXTURE(uri_address, send_binary_msg)
{
    test_websocket_server server;
    streams::producer_consumer_buffer<uint8_t> rbuf;
    std::vector<uint8_t> body(6);
    memcpy(&body[0], "a\0b\0c\0", 6);

    websocket_client_config config;
    config.set_message_type(websocket_message_type::binary_message);
    websocket_client client(m_uri, config);

    send_msg_from_stream_helper(client, server, body, rbuf, test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE).wait();
    rbuf.close(std::ios::out);
    client.close().wait();
}

// Send empty text message
// WinRT client does not handle empty messages. Verify websocket_exception is thrown.
TEST_FIXTURE(uri_address, send_empty_text_msg)
{
    test_websocket_server server;
    websocket_client client(m_uri);

    client.connect().wait();

    websocket_outgoing_message msg;
    msg.set_utf8_message("");
    VERIFY_THROWS(client.send(msg).wait(), websocket_exception);

    client.close().wait();
}

// Send multiple text messages
TEST_FIXTURE(uri_address, send_multiple_text_msges)
{
    test_websocket_server server;
    websocket_client client(m_uri);

    send_text_msg_helper(client, server, "hello1").wait();
    send_text_msg_helper(client, server, "hello2", false).wait();

    client.close().wait();
}

// Send multiple text messages
TEST_FIXTURE(uri_address, send_multiple_text_msges_async)
{
    test_websocket_server server;
    websocket_client client(m_uri);

    auto t1 = send_text_msg_helper(client, server, "hello1");
    auto t2 = send_text_msg_helper(client, server, "hello2", false);

    t2.wait();
    t1.wait();
    client.close().wait();
}

// Send multiple text messages from a stream
TEST_FIXTURE(uri_address, send_multiple_text_msges_stream)
{
    test_websocket_server server;
    streams::producer_consumer_buffer<uint8_t> rbuf;
    std::vector<uint8_t> body1(26);
    memcpy(&body1[0], "abcdefghijklmnopqrstuvwxyz", 26);
    std::vector<uint8_t> body2(26);
    memcpy(&body2[0], "zyxwvutsrqponmlkjihgfedcba", 26);

    websocket_client client(m_uri);

    auto t1 = send_msg_from_stream_helper(client, server, body1, rbuf, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE);
    auto t2 = send_msg_from_stream_helper(client, server, body2, rbuf, test_websocket_message_type::WEB_SOCKET_UTF8_MESSAGE_TYPE, false);

    t1.wait();
    t2.wait();
    client.close().wait();
}

// Send multiple binary messages from the same stream
TEST_FIXTURE(uri_address, send_multiple_binary_msg_same_stream)
{
    test_websocket_server server;
    streams::producer_consumer_buffer<uint8_t> rbuf;
    std::vector<uint8_t> body1(6);
    memcpy(&body1[0], "a\0b\0c\0", 6);
    std::vector<uint8_t> body2(6);
    memcpy(&body2[0], "a\0b\0c\0", 6);

    websocket_client_config config;
    config.set_message_type(websocket_message_type::binary_message);
    websocket_client client(m_uri, config);

    auto t1 = send_msg_from_stream_helper(client, server, body1, rbuf, test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE);
    auto t2 = send_msg_from_stream_helper(client, server, body2, rbuf, test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE, false);

    t1.wait();
    t2.wait();
    rbuf.close(std::ios_base::out);
    client.close().wait();
}

// Send text message followed by binary message: WinRT does not support this scenario.
TEST_FIXTURE(uri_address, send_text_and_binary)
{
    test_websocket_server server;
    streams::producer_consumer_buffer<uint8_t> rbuf;
    std::vector<uint8_t> body2(6);
    memcpy(&body2[0], "a\0b\0c\0", 6);

    websocket_client client(m_uri);

    send_text_msg_helper(client, server, "hello1").wait();
    auto t = send_msg_from_stream_helper(client, server, body2, rbuf, test_websocket_message_type::WEB_SOCKET_BINARY_MESSAGE_TYPE, false);
    VERIFY_THROWS(t.wait(), websocket_exception);

    rbuf.close(std::ios::out).wait();
    client.close().wait();
}

// Send a multi byte UTF-8 text message 
TEST_FIXTURE(uri_address, send_multi_byte_utf8_msg)
{
    test_websocket_server server;
    utf16string utf16str(U("аш"));
    auto body = utility::conversions::to_utf8string(utf16str);
    websocket_client client(m_uri);

    send_text_msg_helper(client, server, body).wait();
    client.close().wait();
}

} // SUITE(send_msg_tests)

}}}}