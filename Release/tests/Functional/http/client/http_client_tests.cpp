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
* http_client_tests.cpp
*
* Common definitions and helper functions for http_client test cases.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

void test_connection(test_http_server *p_server, http_client *p_client, const utility::string_t &path)
{
    p_server->next_request().then([path](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::GET, path);
        VERIFY_ARE_EQUAL(0u, p_request->reply(200));
    });
    http_asserts::assert_response_equals(p_client->request(methods::GET).get(), status_codes::OK);
}

// Helper function send a simple request to test the connection.
// Take in the path to request and what path should be received in the server.
void test_connection(test_http_server *p_server, http_client *p_client, const utility::string_t &request_path, const utility::string_t &expected_path)
{
    p_server->next_request().then([expected_path](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::GET, expected_path);
        VERIFY_ARE_EQUAL(0u, p_request->reply(200));
    });
    http_asserts::assert_response_equals(p_client->request(methods::GET, request_path).get(), status_codes::OK);
}

}}}}