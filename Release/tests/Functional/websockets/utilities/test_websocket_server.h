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
* test_websocket_server.h -- Defines a test server to handle incoming and outgoing messages.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <map>
#include <sstream>

#include "unittestpp.h"
#include "cpprest/uri.h"
#include "cpprest/ws_msg.h"

#include <iostream>
#include <mutex>
#include <condition_variable>

#ifndef TEST_UTILITY_API
#ifdef WEBSOCKETTESTUTILITY_EXPORTS
    #define TEST_UTILITY_API __declspec(dllexport)
#else
    #define TEST_UTILITY_API __declspec(dllimport)
#endif
#endif

namespace tests { namespace functional { namespace websocket { namespace utilities {

class _test_websocket_server;

// The different types of a websocket message.
enum test_websocket_message_type
{
  WEB_SOCKET_BINARY_MESSAGE_TYPE,
  WEB_SOCKET_BINARY_FRAGMENT_TYPE,
  WEB_SOCKET_UTF8_MESSAGE_TYPE,
  WEB_SOCKET_UTF8_FRAGMENT_TYPE,
  WEB_SOCKET_CLOSE_TYPE
};

// Represents a websocket message at the test server.
// Contains a vector that can contain text/binary data 
// and a type variable to denote the message type.
class test_websocket_msg
{
public:
    const std::vector<unsigned char>& data() const { return m_data; }
    void set_data(std::vector<unsigned char> data) { m_data = std::move(data); }

    test_websocket_message_type msg_type() const { return m_msg_type; }
    void set_msg_type(test_websocket_message_type type) { m_msg_type = type; }

private:
    std::vector<unsigned char> m_data;
    test_websocket_message_type m_msg_type;
};

class websocket_asserts
{
public:
    static void assert_message_equals(test_websocket_msg& msg, const std::string& expected_data, test_websocket_message_type expected_flag)
    {
        std::vector<unsigned char> temp_vec(expected_data.begin(), expected_data.end());
        assert_message_equals(msg, temp_vec, expected_flag);
    }

    static void assert_message_equals(test_websocket_msg& msg, const std::vector<unsigned char>& expected_data, test_websocket_message_type expected_flag)
    {
        VERIFY_ARE_EQUAL(msg.msg_type(), expected_flag);
        auto& data = msg.data();
        VERIFY_ARE_EQUAL(data.size(), expected_data.size());
        VERIFY_IS_TRUE(std::equal(expected_data.begin(), expected_data.end(), data.begin()));
    }

private:
    websocket_asserts() {}
    ~websocket_asserts() _noexcept {}
};

// Test websocket server. 
class test_websocket_server 
{
public:
    TEST_UTILITY_API test_websocket_server(); 

    // Tests can add a handler to handle (verify) the next message received by the server.
    // If the test plans to send n messages, n handlers must be registered.
    // The server will call the handler in order, for each incoming message.
    TEST_UTILITY_API void next_message(std::function<void(test_websocket_msg)> msg_handler);
    TEST_UTILITY_API std::function<void(test_websocket_msg)> get_next_message_handler();
    bool handler_exists() { return !m_handler_queue.empty(); }

    // Tests can use this API to send a message from the server to the client.
    TEST_UTILITY_API void send_msg(const test_websocket_msg& msg);
    std::shared_ptr<_test_websocket_server> get_impl();

private:
    // Queue to maintain the request handlers.
    // Note: This queue is not thread-safe
    std::queue<std::function<void(test_websocket_msg)>> m_handler_queue;
    std::shared_ptr<_test_websocket_server> m_p_impl;
};
}}}}