/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* connections_and_errors.cpp
*
* Tests cases the underlying connections and error cases with the connection using then http_listener.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#include <cpprest/http_client.h>

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace listener {

SUITE(connections_and_errors)
{

  TEST_FIXTURE(uri_address, close_listener_race, "Ignore:Linux", "724744")
{
    ::http::experimental::listener::http_listener listener(m_uri);
    listener.open().wait();

    listener.support([](http_request)
    {
        // Let the connection timeout
    });

    // close() racing with a new connection
    auto closeTask = pplx::create_task([&listener]()
    {
        listener.close().wait();
    });

    auto clientTask = pplx::create_task([this]
    {
        ::http::client::http_client_config config;
        config.set_timeout(utility::seconds(1));
        ::http::client::http_client client(m_uri, config);
        
        try
        {
            // Depending on timing this might not succeed. The 
            // exception will be caught and ignored below
            auto rsp = client.request(methods::GET).get();

            // The response body should timeout and we should recieve an exception
            rsp.content_ready().wait();

            // If we reach here then it is an error
            VERIFY_IS_FALSE(true);
        }
        catch(std::exception)
        {
        }
    });

    (closeTask && clientTask).wait();
}

TEST_FIXTURE(uri_address, send_response_later)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    std::vector<http_request> requests;
    pplx::extensibility::event_t request_event;
    listener.support([&](http_request r)
    {
        requests.push_back(r);
        request_event.set();
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::POST, U("")));
    request_event.wait();
    requests[0].reply(status_codes::OK, U("HEHEHE")).wait();
    p_client->next_response().then([&](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK, U("text/plain; charset=utf-8"), U("HEHEHE"));
    }).wait();
}

TEST_FIXTURE(uri_address, reply_twice)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    listener.support([](http_request request)
    {
        request.reply(status_codes::OK);
        VERIFY_THROWS(request.reply(status_codes::Accepted).get(), http_exception);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("/path")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    listener.close().wait();
}

TEST(default_port_admin_access, "Ignore", "750539")
{
    uri address(U("http://localhost/"));
    http_listener listener(address);
    VERIFY_THROWS_HTTP_ERROR_CODE(listener.open().wait(), std::errc::io_error);
}

TEST_FIXTURE(uri_address, try_port_already_in_use, "Ignore", "750539")
{
    test_http_server::scoped_server scoped(m_uri);
    http_listener listener(m_uri);
    VERIFY_THROWS(listener.open().wait(), http_exception);
}

TEST_FIXTURE(uri_address, reply_after_starting_close, "Ignore", "750539")
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    pplx::task_completion_event<http_request> requestReceivedEvent;
    listener.support([&](http_request request)
    {
        requestReceivedEvent.set(request);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("/path")));
    
    // Once the request has been received, start closing the listener and send response.
    pplx::task<http_request> requestReceivedTask(requestReceivedEvent);
    requestReceivedTask.then([&](http_request request)
    {
        listener.close();
        request.reply(status_codes::OK).wait();
    });
    
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
}

static void close_stream_early_with_length_impl(const uri &u, bool useException)
{
    http_listener listener(u);
    listener.open().wait();
    listener.support([=](http_request request)
    {
        concurrency::streams::producer_consumer_buffer<unsigned char> body;
        concurrency::streams::istream instream = body.create_istream();
        body.putc('A').wait();
        body.putc('B').wait();
        auto responseTask = request.reply(status_codes::OK, instream, 4);
        
        if(useException)
        {
            body.close(std::ios::out, std::make_exception_ptr(std::invalid_argument("test exception"))).wait();
            VERIFY_THROWS(responseTask.get(), std::invalid_argument);
        }
        else
        {
            body.close(std::ios::out).wait();
            VERIFY_THROWS(responseTask.get(), http_exception);
        }
    });

    web::http::client::http_client client(u);
    client.request(methods::GET, U("/path")).then([](http_response response) -> pplx::task<std::vector<unsigned char>>
    {
        VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
        return response.extract_vector();
    }).then([=](pplx::task<std::vector<unsigned char>> bodyTask)
    {
        VERIFY_THROWS(bodyTask.get(), http_exception);    
    }).wait();

    listener.close().wait();
}

TEST_FIXTURE(uri_address, close_stream_early_with_length, "Ignore:Linux", "760544")
{
    close_stream_early_with_length_impl(m_uri, true);
    close_stream_early_with_length_impl(m_uri, false);
}

static void close_stream_early_impl(const uri &u, bool useException)
{
    http_listener listener(u);
    listener.open().wait();
    listener.support([=](http_request request)
    {
        concurrency::streams::producer_consumer_buffer<unsigned char> body;
        concurrency::streams::istream instream = body.create_istream();
        body.putc('A').wait();
        body.putc('B').wait();
        auto responseTask = request.reply(status_codes::OK, instream);
        
        if(useException)
        {
            body.close(std::ios::out, std::make_exception_ptr(std::invalid_argument("test exception"))).wait();
            VERIFY_THROWS(responseTask.get(), std::invalid_argument);
        }
        else
        {
            body.close(std::ios::out).wait();
            responseTask.get();
        }
    });

    web::http::client::http_client client(u);
    client.request(methods::GET, U("/path")).then([](http_response response) -> pplx::task<std::vector<unsigned char>>
    {
        VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
        return response.extract_vector();
    }).then([=](pplx::task<std::vector<unsigned char>> bodyTask)
    {
        if(useException)
        {
            VERIFY_THROWS(bodyTask.get(), http_exception);    
        }
        else
        {
            std::vector<unsigned char> body = bodyTask.get();
            VERIFY_ARE_EQUAL(2, body.size());
            VERIFY_ARE_EQUAL('A', body[0]);
            VERIFY_ARE_EQUAL('B', body[1]);
        }
    }).wait();

    listener.close().wait();
}

TEST_FIXTURE(uri_address, close_stream_with_exception, "Ignore:Linux", "760544")
{
    close_stream_early_impl(m_uri, true);
    close_stream_early_impl(m_uri, false);
}

}

}}}}
