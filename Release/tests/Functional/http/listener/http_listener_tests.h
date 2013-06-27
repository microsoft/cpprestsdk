/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* http_listener_tests.h
*
* Common declarations and helper functions for http_listener test cases.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "cpprest/http_listener.h"
#include "cpprest/log.h"

namespace tests { namespace functional { namespace http { namespace listener {

class uri_address
{
public:
    uri_address() : m_uri(U("http://localhost:34567/")), m_dummy_listener(U("http://localhost:30000/"))
    {
        utility::experimental::logging::log::get_default()->set_synchronous(true);
        m_dummy_listener.open().wait();
    }

    // By introducing an additional listener, we can avoid having to close the
    // server after each unit test.

    web::http::experimental::listener::http_listener m_dummy_listener;
    web::http::uri m_uri;
};

}}}}