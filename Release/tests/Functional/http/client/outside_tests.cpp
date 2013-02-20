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
* outside_tests.cpp
*
* Tests cases for using http_clients to outside websites.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "rawptrstream.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(outside_tests)
{

// TFS 553378
#ifdef _MS_WINDOWS
# define casablanca_printf wprintf
#else
# define casablanca_printf printf
#endif

TEST_FIXTURE(uri_address, outside_cnn_dot_com,
             "Ignore", "Manual")
{
    http_client client(U("http://www.cnn.com"));

    // CNN's main page doesn't use chunked transfer encoding.
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);
    casablanca_printf(U("CNN Main page: %s\n"), response.to_string().c_str());

    // CNN's other pages do use chunked transfer encoding.
    response = client.request(methods::GET, U("US")).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);
    casablanca_printf(U("CNN US page: %s\n"), response.to_string().c_str());
}

TEST_FIXTURE(uri_address, outside_google_dot_com,
             "Ignore", "Manual")
{
    http_client client(U("http://www.google.com"));

    // Google's main page.
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);
    casablanca_printf(U("Google's Main page: %s\n"), response.to_string().c_str());

    // Google's maps page.
    response = client.request(methods::GET, U("maps")).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);
    casablanca_printf(U("Google's Maps page: %s\n"), response.to_string().c_str());
}

TEST_FIXTURE(uri_address, reading_google_stream,
             "Ignore", "Manual")
{
    http_client simpleclient(U("http://www.google.com"));
    utility::string_t path = m_uri.query();
    http_response response = simpleclient.request(::http::methods::GET).get();
 
    uint8_t chars[81];
    memset(chars, 0, sizeof(chars));

	streams::rawptr_buffer<uint8_t> temp(chars, sizeof(chars));
    VERIFY_ARE_EQUAL(response.body().read(temp, 80).get(), 80);
    VERIFY_ARE_EQUAL(strcmp((const char *)chars, "<!doctype html><html itemscope=\"itemscope\" itemtype=\"http://schema.org/WebPage\">"), 0);
}

} // SUITE(outside_tests)

}}}}