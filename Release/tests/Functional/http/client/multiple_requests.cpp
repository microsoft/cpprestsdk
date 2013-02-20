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
* multiple_requests.cpp
*
* Tests cases for multiple requests and responses from an http_client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;
using namespace utility::conversions;
using namespace web::http;
using namespace web::http::client;

using namespace tests::common::utilities;
using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

// Helper function to initialize an array of strings to contain 1 MB data.
static void initialize_data(std::string *data_arrays, const size_t count)
{
    // 10k
    std::string data;
    for(int j = 0; j < 1024 * 10; ++j)
    {
        data.push_back('A' + (j % 26));
    }

    for(size_t i = 0; i < count; ++i)
    {
        data_arrays[i] = data;
        data_arrays[i].push_back('a' + (char)i);
    }
}

SUITE(multiple_requests)
{

TEST_FIXTURE(uri_address, single_tcp_connection)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config config;
    config.set_guarantee_order(true);
    http_client client(m_uri, config);

    const size_t num_requests = 20;
    std::string request_bodies[num_requests];
    initialize_data(request_bodies, num_requests);
    const method method = methods::PUT;
    const web::http::status_code code = status_codes::OK;

    // setup to handle requests on the server side.
    // If any of the requests come in before the response is sent the test fails.
    auto requests = scoped.server()->next_requests(num_requests);
    volatile unsigned long response_sent = 0;
    requests[0].then([&](test_request *request)
    {
        http_asserts::assert_test_request_equals(request, method, U("/"), U("text/plain"), to_string_t(request_bodies[0]));
        // wait a bit to see if the other requests will come.
        os_utilities::sleep(500);
        os_utilities::interlocked_increment(&response_sent);
        VERIFY_ARE_EQUAL(0u, request->reply(code));
    });
    for(size_t i = 1; i < num_requests; ++i)
    {
        requests[i].then([i, &response_sent, &code, &method, &request_bodies](test_request *request)
        {
            if(os_utilities::interlocked_increment(&response_sent) == 1)
            {
                VERIFY_IS_TRUE(false);
            }
            http_asserts::assert_test_request_equals(request, method, U("/"), U("text/plain"), to_string_t(request_bodies[i]));
            VERIFY_ARE_EQUAL(0u, request->reply(code));
        });
    }

    // send requests
    std::vector<pplx::task<http_response>> responses;
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_request msg(method);
        msg.set_body(request_bodies[i]);
        responses.push_back(client.request(msg));
    }

    // wait for requests.
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_asserts::assert_response_equals(responses[i].get(), code);
    }
}

TEST_FIXTURE(uri_address, requests_with_data)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config config;
    http_client client(m_uri, config);

    const size_t num_requests = 20;
    std::string request_body;
    initialize_data(&request_body, 1);
    const method method = methods::PUT;
    const web::http::status_code code = status_codes::OK;

    // send requests
    std::vector<pplx::task<http_response>> responses;
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_request msg(method);
        msg.set_body(request_body);
        responses.push_back(client.request(msg));
    }

    // Wait a bit to let requests get queued up in WinHTTP.
    os_utilities::sleep(500);

    // response to requests
    for(size_t i = 0; i < num_requests; ++i)
    {
        test_request *request = scoped.server()->wait_for_request();
        http_asserts::assert_test_request_equals(request, method, U("/"), U("text/plain"), to_string_t(request_body));
        VERIFY_ARE_EQUAL(0u, request->reply(code));
    }
    
    // wait for requests.
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_asserts::assert_response_equals(responses[i].get(), code);
    }
}

// Tests multiple requests with responses containing data.
TEST_FIXTURE(uri_address, responses_with_data)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    const size_t num_requests = 20;
    std::string request_body;
    initialize_data(&request_body, 1);
    const method method = methods::PUT;
    const web::http::status_code code = status_codes::OK;
    std::map<utility::string_t, utility::string_t> headers;
    headers[U("Content-Type")] = U("text/plain");

    // send requests
    std::vector<pplx::task<http_response>> responses;
    for(size_t i = 0; i < num_requests; ++i)
    {
        responses.push_back(client.request(method));
    }

    // response to requests
    for(size_t i = 0; i < num_requests; ++i)
    {
        test_request *request = scoped.server()->wait_for_request();
        http_asserts::assert_test_request_equals(request, method, U("/"));
        VERIFY_ARE_EQUAL(0u, request->reply(code, U(""), headers, request_body));
    }
    
    // wait for requests.
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_response rsp = responses[i].get();
        http_asserts::assert_response_equals(rsp, code, headers);
        VERIFY_ARE_EQUAL(to_string_t(request_body), rsp.extract_string().get());
    }
}

} // SUITE(multiple_requests)

}}}}
