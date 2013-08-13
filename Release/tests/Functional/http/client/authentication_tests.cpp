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
* Tests cases for authentication with http_clients.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(authentication_tests)
{

TEST_FIXTURE(uri_address, auth_no_data, "Ignore:Linux", "646268")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config client_config;
    credentials cred(U("some_user"), U("some_password")); // WinHTTP requires non-empty password
    client_config.set_credentials(cred);
    http_client client(m_uri, client_config);
    const method mtd = methods::POST;

    http_request msg(mtd);

    auto replyFunc = [&](test_request *p_request)
        {
            http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
            p_request->reply(200);
        };

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));

        // Auth header
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("WWW-Authenticate")] = U("Basic realm = \"WallyWorld\"");

        // unauthorized
        p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers); 

    }).then([&scoped, replyFunc]()
    {
        // Client resent the request
        scoped.server()->next_request().then(replyFunc);
    });

    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

// TFS 648783
#ifndef __cplusplus_winrt
TEST_FIXTURE(uri_address, proxy_auth_known_contentlength, "Ignore:Linux", "646268")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config client_config;
    credentials cred(U("some_user"), U("some_password")); // WinHTTP requires non-empty password
    client_config.set_credentials(cred);
    http_client client(m_uri, client_config);
    const method mtd = methods::POST;
    utility::string_t contents(U("Hello World"));

    http_request msg(mtd);
    msg.set_body(contents);

    auto replyFunc = [&](test_request *p_request)
        {
            http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"), U("text/plain; charset=utf-8"), contents);

            p_request->reply(200);
        };

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));

        // Auth header
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("WWW-Authenticate")] = U("Basic realm = \"WallyWorld\"");

        // unauthorized
        p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers); 

    }).then([&scoped, replyFunc]()
    {
        // Client resent the request
        scoped.server()->next_request().then(replyFunc);
    });

    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}
#endif

TEST_FIXTURE(uri_address, proxy_auth_noseek,
             "Ignore:Linux", "627612")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri); // In this test, the request cannot be resent, so the username and password are not required
    const method mtd = methods::POST;

    auto buf = streams::producer_consumer_buffer<unsigned char>();
    buf.putc('a').get();
    buf.close(std::ios_base::out).get();

    http_request msg(mtd);
    msg.set_body(buf.create_istream(), 1);

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));

        // Auth header
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("WWW-Authenticate")] = U("Basic realm = \"WallyWorld\"");

        // unauthorized
        p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers); 

    });

    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::Unauthorized);
}

// Must specify content length with winrt client, so this test case isn't possible.
#ifndef __cplusplus_winrt
TEST_FIXTURE(uri_address, proxy_auth_unknown_contentlength, 
            "Ignore:Linux", "646268")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config client_config;
    credentials cred(U("some_user"), U("some_password")); // WinHTTP requires non-empty password
    client_config.set_credentials(cred);
    http_client client(m_uri, client_config);
    const method mtd = methods::POST;

    std::vector<uint8_t> msg_body;
    msg_body.push_back('a');

    http_request msg(mtd);
    msg.set_body(streams::container_stream<std::vector<uint8_t>>::open_istream(std::move(msg_body)));

    auto replyFunc = [&](test_request *p_request)
        {
            utility::string_t contents(U("a"));
            http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"), U("application/octet-stream"), contents);

            p_request->reply(200);
        };

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));

        // Auth header
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("WWW-Authenticate")] = U("Basic realm = \"WallyWorld\"");

        // unauthorized
        p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers); 

    }).then([&scoped, replyFunc]()
    {
        // Client resent the request
        scoped.server()->next_request().then(replyFunc);
    });

    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

// Accessing a server that returns 401 with an empty user name should not resend the request with an empty password
TEST_FIXTURE(uri_address, empty_username_password)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("h1")] = U("data1");
        // Auth header
        headers[U("WWW-Authenticate")] = U("Basic realm = \"myRealm\"");
        // unauthorized
        p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers, "a" ); 
    });

    http_response response = client.request(methods::GET).get();
    auto str_body = response.extract_vector().get();
    auto h1 = response.headers()[U("h1")];
    VERIFY_ARE_EQUAL(status_codes::Unauthorized, response.status_code());
    VERIFY_ARE_EQUAL(str_body[0], 'a');
    VERIFY_ARE_EQUAL(h1, U("data1"));
}

// Fails on WinRT due to TFS 648278
// Accessing a server that supports auth, but returns 401, even after the user has provided valid creds
// We're making sure the error is reported properly, and the response data from the second response is received
TEST_FIXTURE(uri_address, error_after_valid_credentials, "Ignore:Linux", "646268")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config client_config;
    credentials cred(U("some_user"), U("some_password"));
    client_config.set_credentials(cred);
    http_client client(m_uri, client_config);

    auto replyFunc = [&](test_request *p_request)
        {
            std::map<utility::string_t, utility::string_t> headers;
            // Auth header
            headers[U("WWW-Authenticate")] = U("Basic realm = \"WallyWorld\"");
            headers[U("h1")] = U("data2");
            // still unauthorized after the user has resent the request with the credentials
            p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers, "def" ); 
        };

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("h1")] = U("data1");
        // Auth header
        headers[U("WWW-Authenticate")] = U("Basic realm = \"myRealm\"");
        // unauthorized
        p_request->reply(status_codes::Unauthorized, U("Authentication Failed"), headers, "abc" ); 
    }).then([&scoped, &replyFunc]()
    {
        // Client resent the request
        scoped.server()->next_request().then(replyFunc);
    });

    http_response response = client.request(methods::GET).get();
    auto str_body = response.extract_vector().get();
    auto h1 = response.headers()[U("h1")];
    VERIFY_ARE_EQUAL(status_codes::Unauthorized, response.status_code());
    VERIFY_ARE_EQUAL(str_body[0], 'd');
    VERIFY_ARE_EQUAL(str_body[1], 'e');
    VERIFY_ARE_EQUAL(str_body[2], 'f');
    VERIFY_ARE_EQUAL(h1, U("data2"));
}

#endif

// These tests are disabled since they require a server with authentication running.
// The server portion to use is the C# AuthenticationListener.
#pragma region Manual Server Authentication Tests

class server_properties
{
public:
    server_properties() {}

    // Helper function to retrieve all parameters necessary for setup tests.
    void load_parameters()
    {
        m_uri = uri(utility::conversions::to_string_t(UnitTest::GlobalSettings::Get("Server")));
        if(UnitTest::GlobalSettings::Has("UserName"))
        {
            m_username = utility::conversions::to_string_t(UnitTest::GlobalSettings::Get("UserName"));
        }
        if(UnitTest::GlobalSettings::Has("Password"))
        {
            m_password = utility::conversions::to_string_t(UnitTest::GlobalSettings::Get("Password"));
        }
    }

    web::http::uri m_uri;
    string_t m_username;
    string_t m_password;
};

// This test should be executed for NTLM, Negotiate, IntegratedWindowsAuth, and Anonymous.
TEST_FIXTURE(server_properties, successful_auth_no_cred, "Requires", "Server")
{
    load_parameters();

    http_client client(m_uri);
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

TEST_FIXTURE(server_properties, digest_basic_auth_no_cred, "Requires", "Server")
{
    load_parameters();

    http_client client(m_uri);
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::Unauthorized, response.status_code());
}

TEST_FIXTURE(server_properties, none_auth_no_cred, "Requires", "Server")
{
    load_parameters();

    http_client client(m_uri);
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::Forbidden, response.status_code());
}

// This test should be executed for NTLM, Negotiate, IntegratedWindowsAuth, and Digest.
TEST_FIXTURE(server_properties, unsuccessful_auth_with_basic_cred, "Requires", "Server;UserName;Password")
{
    load_parameters();

    http_client_config config;
    config.set_credentials(credentials(m_username, m_password));
    
    http_client client(m_uri, config);
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::Unauthorized, response.status_code());
}

TEST_FIXTURE(server_properties, basic_anonymous_auth_with_basic_cred, "Requires", "Server;UserName;Password")
{
    load_parameters();

    http_client_config config;
    config.set_credentials(credentials(m_username, m_password));
    http_client client(m_uri, config);
    http_request req(methods::GET);
    req.headers().add(U("UserName"), m_username);
    req.headers().add(U("Password"), m_password);
    http_response response = client.request(req).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

TEST_FIXTURE(server_properties, none_auth_with_cred, "Requires", "Server;UserName;Password")
{
    load_parameters();

    http_client_config config;
    config.set_credentials(credentials(m_username, m_password));
    http_client client(m_uri, config);
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::Forbidden, response.status_code());
}

// This test should be executed for all authentication schemes except None.
TEST_FIXTURE(server_properties, successful_auth_with_domain_cred, "Requires", "Server;UserName;Password")
{
    load_parameters();

    http_client_config config;
    config.set_credentials(credentials(m_username, m_password));
    http_client client(m_uri, config);
    http_request req(methods::GET);
    req.headers().add(U("UserName"), m_username);
	req.headers().add(U("Password"), m_password);
    http_response response = client.request(req).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

#pragma endregion 

// Fix for 522831 AV after failed authentication attempt
TEST_FIXTURE(uri_address, failed_authentication_attempt, "Ignore:Linux", "549349", "Ignore", "Manual")
{
    http_client_config config;
    credentials cred(U("user"),U("schmuser"));
    config.set_credentials(cred);
    http_client client(U("https://apis.live.net"),config);
    http_response response = client.request(methods::GET, U("V5.0/me/skydrive/files")).get();
    VERIFY_ARE_EQUAL(status_codes::Unauthorized, response.status_code());
    auto v = response.extract_vector().get();
    std::string s(v.begin(), v.end());
    // The resulting data must be non-empty (an error about missing access token)
    VERIFY_IS_FALSE(s.empty());
}

} // SUITE(authentication_tests)

}}}}
