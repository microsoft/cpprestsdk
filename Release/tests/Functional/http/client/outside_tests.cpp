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
#include "cpprest/rawptrstream.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(outside_tests)
{

TEST_FIXTURE(uri_address, outside_cnn_dot_com,
             "Ignore", "Manual")
{
    http_client client(U("http://www.cnn.com"));

    // CNN's main page doesn't use chunked transfer encoding.
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);

    // CNN's other pages do use chunked transfer encoding.
#ifdef _MS_WINDOWS
    response = client.request(methods::GET, U("US")).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);
#else
    // Linux won't handle 301 header automatically 
    response = client.request(methods::GET, U("US")).get();
    VERIFY_ARE_EQUAL(status_codes::MovedPermanently, response.status_code());
#endif
}

TEST_FIXTURE(uri_address, outside_google_dot_com,
             "Ignore", "Manual")
{
    http_client client(U("http://www.google.com"));

    // Google's main page.
    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);

#ifdef _MS_WINDOWS
    // Google's maps page.
    response = client.request(methods::GET, U("maps")).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    while(response.body().streambuf().in_avail() == 0);
#else
    // Linux won't handle 302 header automatically 
    response = client.request(methods::GET, U("maps")).get();
    VERIFY_ARE_EQUAL(status_codes::Found, response.status_code());
#endif
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
    VERIFY_ARE_EQUAL(strcmp((const char *)chars, "<!doctype html><html itemscope=\"\" itemtype=\"http://schema.org/WebPage\"><head><me"), 0);
}

TEST_FIXTURE(uri_address, outside_ssl_json,
             "Ignore", "Manual")
{
    // Create URI for:
    // https://www.googleapis.com/youtube/v3/playlistItems?part=snippet&playlistId=UUF1hMUVwlrvlVMjUGOZExgg&key=AIzaSyAviHxf_y0SzNoAq3iKqvWVE4KQ0yylsnk
    uri_builder playlistUri(U("https://www.googleapis.com/youtube/v3/playlistItems?"));
    playlistUri.append_query(U("part"),U("snippet"));
    playlistUri.append_query(U("playlistId"), U("UUF1hMUVwlrvlVMjUGOZExgg"));
    playlistUri.append_query(U("key"), U("AIzaSyAviHxf_y0SzNoAq3iKqvWVE4KQ0yylsnk"));

    // Send request
    web::http::client::http_client playlistClient(playlistUri.to_uri());
    playlistClient.request(methods::GET).then([=](http_response playlistResponse) -> pplx::task<json::value>
    {
        return playlistResponse.extract_json();
    }).then([=](json::value jsonArray)
    {
        int count = 0;
        auto& items = jsonArray[U("items")];
        for(auto iter = items.cbegin(); iter != items.cend(); ++iter)
        {
            const auto& i = *iter;
            auto name = i.second[U("snippet")][U("title")].to_string();
            count++;
        }
        VERIFY_ARE_EQUAL(3, count); // Update this accordingly, if the number of items changes
    }).wait();
}

} // SUITE(outside_tests)

}}}}
