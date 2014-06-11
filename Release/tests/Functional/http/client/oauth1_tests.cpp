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
* oauth1_tests.cpp
*
* Test cases for oauth1.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::details;
using namespace utility;
using namespace concurrency;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {


SUITE(oauth1_tests)
{


struct oauth1_basic_config
{
    oauth1_basic_config() :
        m_oauth1_config(U("test_key"), U("test_secret"),
            oauth1_token(U("test_token"), U("test_token_secret"), false),
            oauth1_methods::hmac_sha1),
        m_oauth1_handler(m_oauth1_config)
    {}

    oauth1_config m_oauth1_config;
    oauth1_handler m_oauth1_handler;

};

struct oauth1_server_setup
{
    oauth1_server_setup() :
        m_server_uri(U("http://localhost:17778/")),
        m_server(m_server_uri)
    {
    }

    utility::string_t m_server_uri;
    test_http_server::scoped_server m_server;
};

struct oauth1_basic_config_server : public oauth1_basic_config, oauth1_server_setup {};

struct oauth1_auth_config_server : public oauth1_server_setup
{
    oauth1_auth_config_server() :
        m_oauth1_config(U("test_key"), U("test_secret"),
            m_server_uri, m_server_uri, m_server_uri, m_server_uri,
            oauth1_methods::hmac_sha1),
        m_oauth1_handler(m_oauth1_config)
    {}

    oauth1_config m_oauth1_config;
    oauth1_handler m_oauth1_handler;
};


// TODO: This test should be probably moved elsewhere?
TEST(oauth1_unique_nonces)
{
    // Generate 100 nonces and check each is unique.
    std::vector<utility::string_t> nonces(100);
    utility::nonce_generator gen;
    for (auto&& v : nonces)
    {
        v = std::move(gen.generate());
    }
    for (auto v : nonces)
    {
        VERIFY_ARE_EQUAL(1, std::count(nonces.begin(), nonces.end(), v));
    }
}

TEST_FIXTURE(oauth1_basic_config, oauth1_signature_base_string)
{
    // Basic base string generation.
    {
        http_request r;
        r.set_method(methods::POST);
        r.set_request_uri(U("http://example.com:80/request?a=b&c=d")); // Port set to avoid default.

        oauth1_auth_state state = m_oauth1_config._generate_auth_state(U(""), U(""));
        state.set_timestamp(U("12345678"));
        state.set_nonce(U("ABCDEFGH"));

        utility::string_t base_string = m_oauth1_config._build_signature_base_string(r, state);
        utility::string_t correct_base_string(U(
                "POST&http%3A%2F%2Fexample.com%2Frequest&a%3Db%26c%3Dd%26oauth_consumer_key%3Dtest_key%26oauth_nonce%3DABCDEFGH%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D12345678%26oauth_token%3Dtest_token%26oauth_version%3D1.0"
        ));
        VERIFY_ARE_EQUAL(correct_base_string, base_string);
    }
    // Added "extra_param" and proper parameter normalization.
    {
        http_request r;
        r.set_method(methods::POST);
        r.set_request_uri(U("http://example.com:80/request?a=b&c=d"));

        oauth1_auth_state state = m_oauth1_config._generate_auth_state(U("oauth_test"), U("xyzzy"));
        state.set_timestamp(U("12345678"));
        state.set_nonce(U("ABCDEFGH"));

        utility::string_t base_string = m_oauth1_config._build_signature_base_string(r, state);
        utility::string_t correct_base_string(U(
                "POST&http%3A%2F%2Fexample.com%2Frequest&a%3Db%26c%3Dd%26oauth_consumer_key%3Dtest_key%26oauth_nonce%3DABCDEFGH%26oauth_signature_method%3DHMAC-SHA1%26oauth_test%3Dxyzzy%26oauth_timestamp%3D12345678%26oauth_token%3Dtest_token%26oauth_version%3D1.0"
        ));
        VERIFY_ARE_EQUAL(correct_base_string, base_string);
    }
}

TEST_FIXTURE(oauth1_basic_config, oauth1_hmac_sha1_method)
{
    http_request r;
    r.set_method(methods::POST);
    r.set_request_uri(U("http://example.com:80/request?a=b&c=d")); // Port set to avoid default.

    oauth1_auth_state state = m_oauth1_config._generate_auth_state(U(""), U(""));
    state.set_timestamp(U("12345678"));
    state.set_nonce(U("ABCDEFGH"));

    utility::string_t signature = m_oauth1_config._build_hmac_sha1_signature(r, state);

    utility::string_t correct_signature(U("iUq3VlP39UNXoJHXlKjgSTmjEs8="));
    VERIFY_ARE_EQUAL(correct_signature, signature);
}

TEST_FIXTURE(oauth1_basic_config, oauth1_rsa_sha1_method)
{
    // TODO: not implemented
}

TEST_FIXTURE(oauth1_basic_config, oauth1_plaintext_method)
{
    utility::string_t signature(m_oauth1_config._build_plaintext_signature());
    utility::string_t correct_signature(U("test_secret&test_token_secret"));
    VERIFY_ARE_EQUAL(correct_signature, signature);
}

TEST_FIXTURE(oauth1_basic_config_server, oauth1_hmac_sha1_request)
{
    m_oauth1_config.set_method(oauth1_methods::hmac_sha1);

    http_client_config client_config;
    client_config.set_oauth1(m_oauth1_config);
    http_client client(m_server_uri, client_config);

    m_server.server()->next_request().then([](test_request *request)
    {
        const utility::string_t header_authorization(request->m_headers[header_names::authorization]);
        const utility::string_t prefix(U("OAuth oauth_version=\"1.0\", oauth_consumer_key=\"test_key\", oauth_token=\"test_token\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\""));
        VERIFY_ARE_EQUAL(0, header_authorization.find(prefix));
        request->reply(status_codes::OK);
    });

    VERIFY_IS_TRUE(m_oauth1_config.token().is_valid());
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

TEST_FIXTURE(oauth1_basic_config_server, oauth1_rsa_sha1_request)
{
    // TODO: not implemented
}

TEST_FIXTURE(oauth1_basic_config_server, oauth1_plaintext_request)
{
    m_oauth1_config.set_method(oauth1_methods::plaintext);

    http_client_config client_config;
    client_config.set_oauth1(m_oauth1_config);
    http_client client(m_server_uri, client_config);

    m_server.server()->next_request().then([](test_request *request)
    {
        const utility::string_t header_authorization(request->m_headers[header_names::authorization]);
        const utility::string_t prefix(U("OAuth oauth_version=\"1.0\", oauth_consumer_key=\"test_key\", oauth_token=\"test_token\", oauth_signature_method=\"PLAINTEXT\", oauth_timestamp=\""));
        VERIFY_ARE_EQUAL(0, header_authorization.find(prefix));
        request->reply(status_codes::OK);
    });

    VERIFY_IS_TRUE(m_oauth1_config.token().is_valid());
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

TEST_FIXTURE(oauth1_auth_config_server, oauth1_build_authorization_uri)
{
    m_server.server()->next_request().then([](test_request *request)
    {
        const utility::string_t header_authorization(request->m_headers[header_names::authorization]);

        // Verify prefix, and without 'oauth_token'.
        const utility::string_t prefix(U("OAuth oauth_version=\"1.0\", oauth_consumer_key=\"test_key\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\""));
        VERIFY_ARE_EQUAL(0, header_authorization.find(prefix));

        // Verify suffix with proper 'oauth_callback'.
        const utility::string_t suffix(U(", oauth_callback=\"http%3A%2F%2Flocalhost%3A17778%2F\""));
        VERIFY_IS_TRUE(std::equal(suffix.rbegin(), suffix.rend(), header_authorization.rbegin()));

        // Reply with temporary token and secret.
        std::map<utility::string_t, utility::string_t> headers;
        headers[header_names::content_type] = mime_types::application_x_www_form_urlencoded;
        request->reply(status_codes::OK, U(""), headers, "oauth_token=foobar&oauth_token_secret=xyzzy&oauth_callback_confirmed=true");
    });
    
    VERIFY_IS_FALSE(m_oauth1_config.token().is_temporary());
    utility::string_t auth_uri = m_oauth1_config.build_authorization_uri().get();
    VERIFY_ARE_EQUAL(auth_uri, U("http://localhost:17778/?oauth_token=foobar"));
    VERIFY_IS_TRUE(m_oauth1_config.token().is_temporary());
}

// NOTE: This test also covers token_from_verifier().
TEST_FIXTURE(oauth1_auth_config_server, oauth1_token_from_redirected_uri)
{
    m_server.server()->next_request().then([](test_request *request)
    {
        const utility::string_t header_authorization(request->m_headers[header_names::authorization]);
     
        // Verify temporary token prefix.
        const utility::string_t prefix(U("OAuth oauth_version=\"1.0\", oauth_consumer_key=\"test_key\", oauth_token=\"xyzzy\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\""));
        VERIFY_ARE_EQUAL(0, header_authorization.find(prefix));

        // Verify suffix with 'oauth_verifier'.
        const utility::string_t suffix(U(", oauth_verifier=\"simsalabim\""));
        VERIFY_IS_TRUE(std::equal(suffix.rbegin(), suffix.rend(), header_authorization.rbegin()));

        // Verify we have 'oauth_nonce' and 'oauth_signature'.
        VERIFY_ARE_NOT_EQUAL(utility::string_t::npos, header_authorization.find(U("oauth_nonce")));
        VERIFY_ARE_NOT_EQUAL(utility::string_t::npos, header_authorization.find(U("oauth_signature")));

        // Reply with access token and secret.
        std::map<utility::string_t, utility::string_t> headers;
        headers[header_names::content_type] = mime_types::application_x_www_form_urlencoded;
        request->reply(status_codes::OK, U(""), headers, "oauth_token=foo&oauth_token_secret=bar");
    });
    
    m_oauth1_config.set_token(oauth1_token(U("xyzzy"), U(""), true)); // Simulate temporary token.
    VERIFY_IS_TRUE(m_oauth1_config.token().is_temporary());

    const web::http::uri redirected_uri(U("http://localhost:17778/?oauth_token=xyzzy&oauth_verifier=simsalabim"));
    m_oauth1_config.token_from_redirected_uri(redirected_uri).wait();

    VERIFY_IS_TRUE(m_oauth1_config.token().is_valid());
    VERIFY_ARE_EQUAL(m_oauth1_config.token().token(), U("foo"));
    VERIFY_ARE_EQUAL(m_oauth1_config.token().secret(), U("bar"));
}

TEST_FIXTURE(oauth1_auth_config_server, oauth1_core10)
{
    m_oauth1_config.set_use_core10(true);

    // Verify authorization URI is without 'oauth_callback'.
    m_server.server()->next_request().then([](test_request *request)
    {
        const utility::string_t header_authorization(request->m_headers[header_names::authorization]);

        // Verify no token in prefix.
        const utility::string_t prefix(U("OAuth oauth_version=\"1.0\", oauth_consumer_key=\"test_key\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\""));
        VERIFY_ARE_EQUAL(0, header_authorization.find(prefix));

        // Verify no 'oauth_callback'.
        VERIFY_ARE_EQUAL(utility::string_t::npos, header_authorization.find(U("oauth_callback")));

        // Reply with temporary token and secret.
        std::map<utility::string_t, utility::string_t> headers;
        headers[header_names::content_type] = mime_types::application_x_www_form_urlencoded;
        request->reply(status_codes::OK, U(""), headers, "oauth_token=foobar&oauth_token_secret=xyzzy");
    });
    
    VERIFY_IS_FALSE(m_oauth1_config.token().is_temporary());
    utility::string_t auth_uri = m_oauth1_config.build_authorization_uri().get();
    VERIFY_ARE_EQUAL(auth_uri, U("http://localhost:17778/?oauth_token=foobar&oauth_callback=http://localhost:17778/"));
    VERIFY_IS_TRUE(m_oauth1_config.token().is_temporary());

    // Verify token request is sent without 'oauth_verifier' parameter.
    m_server.server()->next_request().then([](test_request *request)
    {
        const utility::string_t header_authorization(request->m_headers[header_names::authorization]);

        // Verify we have token in prefix.
        const utility::string_t prefix(U("OAuth oauth_version=\"1.0\", oauth_consumer_key=\"test_key\", oauth_token=\"foobar\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\""));
        VERIFY_ARE_EQUAL(0, header_authorization.find(prefix));

        // Verify we have 'oauth_nonce' and 'oauth_signature'.
        VERIFY_ARE_NOT_EQUAL(utility::string_t::npos, header_authorization.find(U("oauth_nonce")));
        VERIFY_ARE_NOT_EQUAL(utility::string_t::npos, header_authorization.find(U("oauth_signature")));

        // Reply with access token and secret.
        std::map<utility::string_t, utility::string_t> headers;
        headers[header_names::content_type] = mime_types::application_x_www_form_urlencoded;
        request->reply(status_codes::OK, U(""), headers, "oauth_token=baz&oauth_token_secret=123");
    });

    VERIFY_IS_FALSE(m_oauth1_config.token().is_valid());
    const utility::string_t simulated_redirected_uri(U("http://localhost:17778/?oauth_token=foobar"));
    m_oauth1_config.token_from_redirected_uri(simulated_redirected_uri).wait();

    VERIFY_IS_TRUE(m_oauth1_config.token().is_valid());
    VERIFY_ARE_EQUAL(m_oauth1_config.token().token(), U("baz"));
    VERIFY_ARE_EQUAL(m_oauth1_config.token().secret(), U("123"));
}

} // SUITE(oauth1_tests)


}}}}
