/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* pipeline_stage_tests.cpp
*
* Tests cases for using pipeline stages with http_listener.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::experimental::listener;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace listener {

SUITE(pipeline_stage_tests)
{

pplx::task<http_response> stopit(http_request request, std::shared_ptr<http_pipeline_stage> next_stage)
{
    request.reply(status_codes::Forbidden).wait();
    return request.get_response();
}

TEST_FIXTURE(uri_address, http_short_circuit)
{
    size_t count = 0;

    // We're testing that the request never makes it to the listener's handler, but is intercepted and
    // rejected by the pipeline stage preceding it.
    auto request_counter = 
        [&count](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            ++count;
            return next_stage->propagate(request);
        };

    http_listener listener(m_uri);
    listener.add_handler(request_counter);
    listener.add_handler(stopit);

    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        methods::DEL,
        methods::HEAD,
        methods::POST,
        methods::PUT,
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
        VERIFY_ARE_EQUAL(0, p_client->request(send_methods[i], U("")));
        p_client->next_response().then([](test_response *p_response)
        {
            http_asserts::assert_test_response_equals(p_response, status_codes::Forbidden);
        }).wait();
    }

    listener.close().wait();
    VERIFY_ARE_EQUAL(num_methods, count);
}

TEST_FIXTURE(uri_address, http_short_circuit_no_count)
{
    size_t count = 0;

    // We're testing that the request never makes it to the listener's handler, but is intercepted and
    // rejected by the pipeline stage preceding it.
    auto request_counter = 
        [&count](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            ++count;
            return next_stage->propagate(request);
        };

    http_listener listener(m_uri);
    listener.add_handler(stopit);
    listener.add_handler(request_counter);

    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        methods::DEL,
        methods::HEAD,
        methods::POST,
        methods::PUT,
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
        VERIFY_ARE_EQUAL(0, p_client->request(send_methods[i], U("")));
        p_client->next_response().then([](test_response *p_response)
        {
            http_asserts::assert_test_response_equals(p_response, status_codes::Forbidden);
        }).wait();
    }

    listener.close().wait();
    VERIFY_ARE_EQUAL(0, count);
}

TEST_FIXTURE(uri_address, http_counting_methods)
{
    size_t count = 0;

    auto response_counter = 
        [&count](pplx::task<http_response> r_task) -> pplx::task<http_response>
        {
            ++count;
            return r_task; 
        };
    auto request_counter = 
        [&count,response_counter](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            ++count;
            return next_stage->propagate(request).then(response_counter);
        };

    http_listener listener(m_uri);
    listener.add_handler(request_counter);

    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        methods::DEL,
        methods::HEAD,
        methods::POST,
        methods::PUT,
    };
    utility::string_t recv_methods[] = 
    {
        U("GET"),
        U("DELETE"),
        U("HEAD"),
        U("POST"),
        U("PUT"),
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
        VERIFY_ARE_EQUAL(0, p_client->request(send_methods[i], U("")));
        p_client->next_response().then([](test_response *p_response)
        {
            http_asserts::assert_test_response_equals(p_response, status_codes::OK);
        }).wait();
        VERIFY_ARE_EQUAL(recv_methods[i], actual_method);
    }

    listener.close().wait();
    VERIFY_ARE_EQUAL(num_methods*2, count);
}

}

}}}}