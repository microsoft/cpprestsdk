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
* authentication_tests.cpp
*
* Tests cases for covering authentication using websocket_client
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(__cplusplus_winrt) || !defined(_M_ARM)

using namespace web::websockets;
using namespace web::websockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(authentication_tests)
{

// Authorization not implemented in non WinRT websocket_client yet - CodePlex 254
#if defined(__cplusplus_winrt)
void auth_helper(test_websocket_server& server, const utility::string_t &username = U(""), const utility::string_t &password = U(""))
{
    server.set_http_handler([username, password](test_http_request request)
    {
        test_http_response resp;
        if (request->username().empty()) // No credentials -> challenge the request
        {
            resp.set_status_code(401); // Unauthorized.
            resp.set_realm("My Realm");
        }
        else if (request->username().compare(utility::conversions::to_utf8string(username))
            || request->password().compare(utility::conversions::to_utf8string(password)))
        {
            resp.set_status_code(403); // User name/password did not match: Forbidden - auth failure.
        }
        else
        {
            resp.set_status_code(200); // User name and passwords match. Successful auth.
        }
        return resp;
    });
}

// connect without credentials, when the server expects credentials
TEST_FIXTURE(uri_address, auth_no_credentials, "Ignore", "245")
{
    test_websocket_server server;
    websocket_client client;
    auth_helper(server);
    VERIFY_THROWS(client.connect(m_uri).wait(), websocket_exception);
}

// Connect with credentials
TEST_FIXTURE(uri_address, auth_with_credentials, "Ignore", "245")
{
    test_websocket_server server;
    websocket_client_config config;
    web::credentials cred(U("user"), U("password"));
    config.set_credentials(cred);
    websocket_client client(config);

    auth_helper(server, cred.username(), U("password"));
    client.connect(m_uri).wait();
    client.close().wait();
}
#endif

// helper function to check if failure is due to timeout.
bool is_timeout(const std::string &msg)
{
    if (msg.find("set_fail_handler") != std::string::npos)
    {
        if (msg.find("handshake timed out") != std::string::npos || msg.find("Timer Expired") != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

TEST(ssl_test)
{
    websocket_client client;
    std::string body_str("hello");

    try
    {
        client.connect(U("wss://echo.websocket.org/")).wait();
        auto receive_task = client.receive().then([body_str](websocket_incoming_message ret_msg)
        {
            VERIFY_ARE_EQUAL(ret_msg.length(), body_str.length());
            auto ret_str = ret_msg.extract_string().get();

            VERIFY_ARE_EQUAL(body_str.compare(ret_str), 0);
            VERIFY_ARE_EQUAL(ret_msg.message_type(), websocket_message_type::text_message);
        });

        websocket_outgoing_message msg;
        msg.set_utf8_message(body_str);
        client.send(msg).wait();

        receive_task.wait();
        client.close().wait();
    }
    catch (const websocket_exception &e)
    {
        if (is_timeout(e.what()))
        {
            // Since this test depends on an outside server sometimes it sporadically can fail due to timeouts
            // especially on our build machines.
            return;
        }
        throw;
    }
}

// These tests are specific to our websocketpp based implementation.
#if !defined(__cplusplus_winrt)

void sni_test_impl(websocket_client &client)
{
    try
    {
        client.connect(U("wss://swordsoftruth.com")).wait();

        // Should never be reached.
        VERIFY_IS_TRUE(false);
    }
    catch (const websocket_exception &e)
    {
        if (is_timeout(e.what()))
        {
            // Since this test depends on an outside server sometimes it sporadically can fail due to timeouts
            // especially on our build machines.
            return;
        }

        // This test just covers establishing the TLS connection and verifying
        // the server certificate, expect it to return an unexpected HTTP status code.
        if (e.error_code().value() == 20)
        {
            return;
        }
        throw;
    }
}

// Test specifically for server SignalR team hit interesting cases with.
TEST(sni_with_older_server_test)
{
    websocket_client client;
    sni_test_impl(client);
}

// WinRT doesn't expose option for disabling.
TEST(disable_sni)
{
    websocket_client_config config;
    config.disable_sni();
    websocket_client client(config);

    try
    {
        client.connect(U("wss://swordsoftruth.com")).wait();

        // Should never be reached.
        VERIFY_IS_TRUE(false);
    }
    catch (const websocket_exception &e)
    {
        // Should fail for a reason different than invalid HTTP status. 
        if (e.error_code().value() != 20)
        {
            return;
        }
        throw;
    }
}

// Winrt doesn't allow explicitly setting server host for SNI.
TEST(sni_explicit_hostname)
{
    websocket_client_config config;
    const auto &name = utf8string("swordsoftruth.com");
    config.set_server_name(name);
    VERIFY_ARE_EQUAL(name, config.server_name());
    websocket_client client(config);
    sni_test_impl(client);
}

void handshake_error_test_impl(const ::utility::string_t &host)
{
    websocket_client client;
    try
    {
        client.connect(host).wait();
        VERIFY_IS_TRUE(false);
    }
    catch (const websocket_exception &e)
    {
        if (is_timeout(e.what()))
        {
            // Since this test depends on an outside server sometimes it sporadically can fail due to timeouts
            // especially on our build machines.
            return;
        }
        VERIFY_ARE_EQUAL("TLS handshake failed", e.error_code().message());
    }
}

TEST(self_signed_cert)
{
    handshake_error_test_impl(U("wss://www.pcwebshop.co.uk/"));
}

TEST(hostname_mismatch)
{
    handshake_error_test_impl(U("wss://jabbr.net"));
}

TEST(cert_expired)
{
    handshake_error_test_impl(U("wss://tv.eurosport.com/"));
}

#endif

} // SUITE(authentication_tests)

}}}}

#endif
