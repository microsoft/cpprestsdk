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
    uri_address() : m_uri(U("http://localhost:34567/")) 
    {
        utility::logging::log::get_default()->set_synchronous(true);
    }

    web::http::uri m_uri;
};

}}}}