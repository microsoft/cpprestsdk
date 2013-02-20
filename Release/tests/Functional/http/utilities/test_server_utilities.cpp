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

#include "stdafx.h"

#include "test_server_utilities.h"
#include "http_asserts.h"

using namespace web; using namespace utility;

namespace tests { namespace functional { namespace http { namespace utilities {

void test_server_utilities::verify_request(
        ::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        test_http_server *p_server,
        unsigned short code)
{
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, path);
        VERIFY_ARE_EQUAL(0, p_request->reply(code));
    });
    http_asserts::assert_response_equals(p_client->request(method, path).get(), code);
}

void test_server_utilities::verify_request(
        ::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        test_http_server *p_server,
        unsigned short code,
        const utility::string_t &reason)
{
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, path);
        VERIFY_ARE_EQUAL(0, p_request->reply(code, reason));
    });
    http_asserts::assert_response_equals(p_client->request(method, path).get(), code, reason);
}

void test_server_utilities::verify_request(
        ::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        const utility::string_t &request_content_type,
        const utility::string_t &request_data,
        test_http_server *p_server,
        unsigned short code,
        const utility::string_t &reason)
{
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, path, request_content_type, request_data);
        VERIFY_ARE_EQUAL(0, p_request->reply(code, reason));
    });
    http_asserts::assert_response_equals(p_client->request(method, path, request_data, request_content_type).get(), code, reason);
}

void test_server_utilities::verify_request(
        ::http::client::http_client *p_client,
        const utility::string_t &method,
        const utility::string_t &path,
        test_http_server *p_server,
        unsigned short code,
        const std::map<utility::string_t, utility::string_t> &response_headers)
{
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, path);
        VERIFY_ARE_EQUAL(0, p_request->reply(code, U(""), response_headers));
    });
    http_asserts::assert_response_equals(p_client->request(method, path).get(), code, response_headers);
}

}}}}
