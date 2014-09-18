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
* status_code_reason_phrase_tests.cpp
*
* Tests cases for covering HTTP status codes and reason phrases.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(status_code_reason_phrase_tests)
{

TEST_FIXTURE(uri_address, status_code)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    // custom status code.
    test_server_utilities::verify_request(&client, methods::GET, U("/"), scoped.server(), 666);    
}

TEST_FIXTURE(uri_address, reason_phrase)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    test_server_utilities::verify_request(&client, methods::GET, U("/"), scoped.server(), status_codes::OK, U("Reasons!!"));
}

} // SUITE(status_code_reason_phrase_tests)

}}}}