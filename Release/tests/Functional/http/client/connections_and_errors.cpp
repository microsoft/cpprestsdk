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
* connections_and_errors.cpp
*
* Tests cases for covering issues dealing with http_client lifetime, underlying TCP connections, and general connection errors.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#ifndef __cplusplus_winrt
#include "cpprest/http_listener.h"
#endif

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

// Test implementation for pending_requests_after_client.
static void pending_requests_after_client_impl(uri address, bool guarantee_order)
{
    test_http_server::scoped_server scoped(address);
    const method mtd = methods::GET;

    const size_t num_requests = 10;
    std::vector<pplx::task<http_response>> responses;
    {
        http_client_config config;
        config.set_guarantee_order(guarantee_order);
        http_client client(address, config);

        // send requests.
        for(size_t i = 0; i < num_requests; ++i)
        {
            responses.push_back(client.request(mtd));
        }
    }

    // send responses.
    for(size_t i = 0; i < num_requests; ++i)
    {
        scoped.server()->next_request().then([&](test_request *request)
        {
            http_asserts::assert_test_request_equals(request, mtd, U("/"));
            VERIFY_ARE_EQUAL(0u, request->reply(status_codes::OK));
        });
    }

    // verify responses.
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_asserts::assert_response_equals(responses[i].get(), status_codes::OK);
    }
}

SUITE(connections_and_errors)
{

// Tests requests still outstanding after the http_client has been destroyed.
TEST_FIXTURE(uri_address, pending_requests_after_client)
{
    pending_requests_after_client_impl(m_uri, true);
    pending_requests_after_client_impl(m_uri, false);
}

TEST_FIXTURE(uri_address, server_doesnt_exist)
{
    http_client client(m_uri);
    VERIFY_THROWS_HTTP_ERROR_CODE(client.request(methods::GET).wait(), std::errc::host_unreachable);
}

TEST_FIXTURE(uri_address, open_failure)
{
    http_client client(U("http://localhost323:-1"));

    // This API should not throw. The exception should be surfaced
    // during task.wait/get
    auto t = client.request(methods::GET);
    VERIFY_THROWS(t.wait(), web::http::http_exception);
}

TEST_FIXTURE(uri_address, server_close_without_responding)
{
    test_http_server server(m_uri);
    VERIFY_ARE_EQUAL(0u, server.open());
    http_client client(m_uri);

    // Send request.
    auto request = client.request(methods::PUT);

    // Close server connection.
    server.wait_for_request();
    server.close();
    VERIFY_THROWS_HTTP_ERROR_CODE(request.wait(), std::errc::connection_aborted);

    // Try sending another request.
    VERIFY_THROWS_HTTP_ERROR_CODE(client.request(methods::GET).wait(), std::errc::host_unreachable);
}

TEST_FIXTURE(uri_address, request_timeout)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config config;
    config.set_timeout(utility::seconds(1));

    http_client client(m_uri, config);

    VERIFY_THROWS_HTTP_ERROR_CODE(client.request(methods::GET).get(), std::errc::timed_out);
}

TEST_FIXTURE(uri_address, invalid_method)
{
    web::http::uri uri(U("http://www.bing.com/"));
    http_client client(uri);
    string_t invalid_chars = U("\a\b\f\v\n\r\t\x20\x7f");

    for (auto iter = invalid_chars.begin(); iter < invalid_chars.end(); iter++)
    {
        string_t method = U("my method");
        method[2] = *iter;
        VERIFY_THROWS(client.request(method).get(), http_exception);
    }
}

// This test sends an SSL request to a non-SSL server and should fail on handshaking
TEST_FIXTURE(uri_address, handshake_fail, "Ignore:Linux", "TFS#747982")
{
    web::http::uri ssl_uri(U("https://localhost:34568/"));

    test_http_server::scoped_server scoped(ssl_uri);

    http_client client(ssl_uri);
    auto request = client.request(methods::GET);

    VERIFY_THROWS(request.get(), http_exception);
}

#if !defined(__cplusplus_winrt)
// Test case needs a server with invalid certificate running.
TEST(server_cert_invalid, "Ignore", "Manual")
{
    http_client client(U("https://localhost"));

    auto requestTask = client.request(methods::GET);
    VERIFY_THROWS(requestTask.get(), http_exception);
}

// Test case needs a server with invalid certificate running.
TEST(ignore_server_cert_invalid, "Ignore", "Manual")
{
    http_client_config config;
    config.set_validate_certificates(false);
    http_client client(U("https://localhost"), config);

    auto request = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, request.status_code());
}
#endif

#if !defined(__cplusplus_winrt)
// This test still sometimes segfaults on Linux, but I'm re-enabling it [AL]
TEST_FIXTURE(uri_address, content_ready_timeout)
{
    web::http::experimental::listener::http_listener listener(m_uri);
    listener.open().wait();

    streams::producer_consumer_buffer<uint8_t> buf;

    listener.support([buf](http_request request)
    {
        http_response response(200);
        response.set_body(streams::istream(buf), U("text/plain"));
        response.headers().add(header_names::connection, U("close")); 
        request.reply(response);
    });

    {
        http_client_config config;
        config.set_timeout(utility::seconds(1));
        http_client client(m_uri, config);
        http_request msg(methods::GET);
        http_response rsp = client.request(msg).get();

        // The response body should timeout and we should recieve an exception
        VERIFY_THROWS_HTTP_ERROR_CODE(rsp.content_ready().wait(), std::errc::timed_out);
    }

    buf.close(std::ios_base::out).wait();
    listener.close().wait();
}

TEST_FIXTURE(uri_address, stream_timeout)
{
    web::http::experimental::listener::http_listener listener(m_uri);
    listener.open().wait();

    streams::producer_consumer_buffer<uint8_t> buf;

    listener.support([buf](http_request request)
    {
        http_response response(200);
        response.set_body(streams::istream(buf), U("text/plain"));
        response.headers().add(header_names::connection, U("close")); 
        request.reply(response);
    });

    {
        http_client_config config;
        config.set_timeout(utility::seconds(1));
        http_client client(m_uri, config);
        http_request msg(methods::GET);
        http_response rsp = client.request(msg).get();

        // The response body should timeout and we should receive an exception
        VERIFY_THROWS_HTTP_ERROR_CODE(rsp.body().read_to_end(streams::producer_consumer_buffer<uint8_t>()).wait(), std::errc::timed_out);
    }

    buf.close(std::ios_base::out).wait();
    listener.close().wait();
}
#endif

} // SUITE(connections_and_errors)

}}}}
