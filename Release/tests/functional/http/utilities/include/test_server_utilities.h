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
* test_server_utilities.h - Utility class to send and verify requests and responses working with the http_test_server.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "cpprest/http_client.h"
#include "test_http_server.h"

#include "http_test_utilities_public.h"

namespace tests { namespace functional { namespace http { namespace utilities {
    
class test_server_utilities
{
public:

    /// <summary>
    /// Sends request with specified values using given http_client and verifies
    /// they are properly received by the test server.
    /// </summary>
    TEST_UTILITY_API static void __cdecl verify_request(
        web::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        test_http_server *p_server,
        unsigned short code);
    
    TEST_UTILITY_API static void __cdecl verify_request(
        web::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        test_http_server *p_server,
        unsigned short code,
        const utility::string_t &reason);

    TEST_UTILITY_API static void __cdecl verify_request(
        web::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        const utility::string_t &request_content_type,
        const utility::string_t &request_data,
        test_http_server *p_server,
        unsigned short code,
        const utility::string_t &reason);

    TEST_UTILITY_API static void __cdecl verify_request(
        web::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        test_http_server *p_server,
        unsigned short code,
        const std::map<utility::string_t, utility::string_t> &response_headers);
};

}}}}