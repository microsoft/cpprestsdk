/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* requests_tests.cpp
*
* Tests cases for covering sending various requests to http_listener.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::experimental::listener;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace listener {

SUITE(requests_tests)
{

TEST_FIXTURE(uri_address, http_methods)
{
    http_listener listener(m_uri);

    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        U("GET"),
        methods::DEL,
        methods::HEAD,
        U("HeAd"),
        methods::POST,
        methods::PUT,
        U("CUstomMETHOD")
    };
    utility::string_t recv_methods[] = 
    {
        U("GET"),
        U("GET"),
        U("DELETE"),
        U("HEAD"),
        U("HEAD"),
        U("POST"),
        U("PUT"),
        U("CUstomMETHOD")
    };
    const size_t num_methods = sizeof(send_methods) / sizeof(send_methods[0]);

    utility::string_t actual_method;
    listener.support([&](http_request request)
    {
        actual_method = request.method();
        request.reply(status_codes::OK).wait();
    });

    for(int i = 0; i < num_methods; ++i)
    {
        pplx::extensibility::event_t ev;
        VERIFY_ARE_EQUAL(0, p_client->request(send_methods[i], U("")));
        p_client->next_response().then([&ev](test_response *p_response)
        {
            http_asserts::assert_test_response_equals(p_response, status_codes::OK);
            ev.set();
        }).wait();
        VERIFY_ARE_EQUAL(recv_methods[i], actual_method);
        ev.wait();
    }

    listener.close().wait();
}

TEST_FIXTURE(uri_address, http_body_and_body_size)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // request with no body
    listener.support([](http_request request)
    {
        http_asserts::assert_request_equals(request, U("GET"), U("/"));
        VERIFY_ARE_EQUAL(0, request.body().streambuf().in_avail());
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    // request with body size explicitly 0
    listener.support([](http_request request)
    {
        http_asserts::assert_request_equals(request, U("GET"), U("/"));
        VERIFY_ARE_EQUAL(0, request.body().streambuf().in_avail());
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U(""), ""));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    // request with body data
    std::string data("HEHE");
    listener.support([&](http_request request)
    {
        http_asserts::assert_request_equals(request, U("GET"), U("/"));

        auto stream = request.body();
        VERIFY_IS_TRUE(stream.is_valid());
        auto buf = stream.streambuf();
        VERIFY_IS_TRUE(buf);

        // Can't extract once you have the request stream. From here, it's
        // stream reading that counts.
#ifndef _MS_WINDOWS
		// TFS#535124 the assert below is temporarily disabled
		//VERIFY_THROWS(request.extract_string().get(), pplx::invalid_operation);
#endif
        request.content_ready().wait();
        
        VERIFY_ARE_EQUAL(data.size(), buf.in_avail());
        VERIFY_ARE_EQUAL('H', (char)buf.sbumpc());
        VERIFY_ARE_EQUAL('E', (char)buf.sbumpc());
        VERIFY_ARE_EQUAL('H', (char)buf.sbumpc());
        VERIFY_ARE_EQUAL('E', (char)buf.sbumpc());
        request.reply(status_codes::OK);

    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U(""), data));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
}

TEST_FIXTURE(uri_address, large_body)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    std::string data_piece("abcdefghijklmnopqrstuvwxyz");
    std::string send_data;
    // 26 * 160 is greater than 4k which is the chunk size.
    for(int i = 0; i < 160; ++i)
    {
        send_data.append(data_piece);
    }
    listener.support([&](http_request request)
    {
        std::string recv_data = utility::conversions::to_utf8string(request.extract_string().get());
        VERIFY_ARE_EQUAL(send_data, recv_data);
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U(""), U("text/plain"), send_data));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
}

    }

}}}}
