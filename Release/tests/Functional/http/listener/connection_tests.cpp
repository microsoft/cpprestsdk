/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* connection_tests.cpp
*
* Tests cases the underlying connections with http_listener.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;
using namespace web::http;
using namespace web::http::experimental::listener;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace listener {

SUITE(connection_tests)
{

TEST_FIXTURE(uri_address, close_listener_race)
{
    auto listener = ::http::experimental::listener::http_listener::create(m_uri);
    VERIFY_ARE_EQUAL(0, listener.open());

    listener.support([](http_request)
    {
        // Let the connection timeout
    });

    // close() racing with a new connection
    auto closeTask = pplx::create_task([&listener]()
    {
        VERIFY_ARE_EQUAL(0, listener.close());
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
    http_listener listener = http_listener::create(m_uri);
    VERIFY_ARE_EQUAL(0, listener.open());
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

}

}}}}