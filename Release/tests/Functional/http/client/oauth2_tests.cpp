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
* oauth2_tests.cpp
*
* Test cases for oauth2.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace utility;
using namespace concurrency;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(oauth2_tests)
{

TEST_FIXTURE(uri_address, oauth2_bearer_token)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config config;
    config.set_oauth2(oauth2_config("12345678"));
    http_client client(m_uri, config);

    scoped.server()->next_request().then([](test_request *request)
    {
        VERIFY_ARE_EQUAL(U("Bearer 12345678"), request->m_headers[U("Authorization")]);
        request->reply(status_codes::OK);
    });

    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
}

} // SUITE(oauth2_tests)


}}}}
