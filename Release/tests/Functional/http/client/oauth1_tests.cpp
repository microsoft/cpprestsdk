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
using namespace utility;
using namespace concurrency;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {


SUITE(oauth1_tests)
{


struct oauth1_test_settings
{
// TODO: modify parameters (secret, token secret) to include encodable characters
    oauth1_test_settings() :
        m_oauth1_config("test_key", "test_secret", "test_token", "test_token_secret", oauth1_methods::hmac_sha1),
        m_oauth1_handler(m_oauth1_config),
        m_test_server_uri(U("http://localhost:34568/"))
    {}

    oauth1_config m_oauth1_config;
    oauth1_handler m_oauth1_handler;
    web::http::uri m_test_server_uri;
};


TEST_FIXTURE(oauth1_test_settings, oauth1_unique_nonces)
{
    // Generate 100 nonces and check each is unique.
    std::vector<utility::string_t> nonces(100);
    for (auto&& v : nonces)
    {
        v = std::move(m_oauth1_handler._generate_nonce());
    }
    for (auto v : nonces)
    {
        VERIFY_ARE_EQUAL(1, std::count(nonces.begin(), nonces.end(), v));
    }
}

TEST_FIXTURE(oauth1_test_settings, oauth1_signature_base_string)
{
	http_request r;
	r.set_method(methods::POST);
	r.set_request_uri("http://example.com:80/request?a=b&c=d"); // Port set to avoid default.
	utility::string_t base_string = m_oauth1_handler._build_signature_base_string(r, "12345678", "ABCDEFGH");

	utility::string_t correct_base_string("POST&http%3A%2F%2Fexample.com%2Frequest%3F&a%3Db%26c%3Dd%26oauth_consumer_key%3Dtest_key%26oauth_nonce%3DABCDEFGH%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D12345678%26oauth_token%3Dtest_token%26oauth_version%3D1.0");
	VERIFY_ARE_EQUAL(correct_base_string, base_string);
}

TEST_FIXTURE(oauth1_test_settings, oauth1_hmac_sha1_method)
{
    http_request r;
    r.set_method(methods::POST);
    r.set_request_uri("http://example.com:80/request?a=b&c=d"); // Port set to avoid default.
    utility::string_t signature = m_oauth1_handler._build_hmac_sha1_signature(r, "12345678", "ABCDEFGH");

    utility::string_t correct_signature("8pefFJrk/oFkdfbNSC8odn3+GJ4=");
    VERIFY_ARE_EQUAL(correct_signature, signature);
}

TEST_FIXTURE(oauth1_test_settings, oauth1_rsa_sha1_method)
{
// TODO: verify signature over base string
}

TEST_FIXTURE(oauth1_test_settings, oauth1_plaintext_method)
{
    utility::string_t signature(m_oauth1_handler._build_plaintext_signature());
    utility::string_t correct_signature("test_secret&test_token_secret");
    VERIFY_ARE_EQUAL(correct_signature, signature);
}

TEST_FIXTURE(oauth1_test_settings, oauth1_hmac_sha1_request)
{
    test_http_server::scoped_server scoped(m_test_server_uri);
    http_client_config client_config;
    client_config.set_oauth1(m_oauth1_config);
    http_client client(m_test_server_uri, client_config);

    scoped.server()->next_request().then([](test_request *request)
    {
// TODO: verify Authorization field
//        VERIFY_ARE_EQUAL(correct_header, request->m_headers[U("Authorization")]);
//        std::cout << request->m_headers[U("Authorization")] << std::endl;
        request->reply(status_codes::OK);
    });

    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

TEST_FIXTURE(oauth1_test_settings, oauth1_rsa_sha1_request)
{
// TODO: create test server, send request, verity headers are correct
}

TEST_FIXTURE(oauth1_test_settings, oauth1_plaintext_request)
{
// TODO: create test server, send request, verity headers are correct
}

} // SUITE(oauth1_tests)


}}}}
