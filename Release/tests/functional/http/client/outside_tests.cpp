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
* Tests cases for using http_clients to outside websites.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#if defined(_MSC_VER) && !defined(__cplusplus_winrt)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#endif
#include "cpprest/rawptrstream.h"
#include "os_utilities.h"
#include <stdexcept>

using namespace web;
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::common::utilities;
using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(outside_tests)
{

TEST_FIXTURE(uri_address, outside_cnn_dot_com)
{
    handle_timeout([]
    {
        http_client client(U("http://www.cnn.com"));

        // CNN's main page doesn't use chunked transfer encoding.
        http_response response = client.request(methods::GET).get();
        VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
        response.content_ready().wait();

        // CNN's other pages do use chunked transfer encoding.
        response = client.request(methods::GET, U("US")).get();
        VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
        response.content_ready().wait();
    });
}

TEST_FIXTURE(uri_address, outside_google_dot_com)
{
    http_client client(U("http://www.google.com"));
    http_request request(methods::GET);
    for (int i = 0; i < 2; ++i)
    {
        http_response response = client.request(request).get();
        VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    }
}
    
TEST_FIXTURE(uri_address, multiple_https_requests)
{
    handle_timeout([&]
    {
        http_client client(U("https://www.google.com"));
    
        http_response response;
        for(int i = 0; i < 5; ++i)
        {
            response = client.request(methods::GET).get();
            VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
            response.content_ready().wait();
        }
    });
}

TEST_FIXTURE(uri_address, reading_google_stream)
{
    handle_timeout([&]
    {
        http_client simpleclient(U("http://www.google.com"));
        utility::string_t path = m_uri.query();
        http_response response = simpleclient.request(::http::methods::GET).get();

        uint8_t chars[71];
        memset(chars, 0, sizeof(chars));

        streams::rawptr_buffer<uint8_t> temp(chars, sizeof(chars));

        VERIFY_ARE_EQUAL(response.body().read(temp, 70).get(), 70);
        VERIFY_ARE_EQUAL(strcmp((const char *) chars, "<!doctype html><html itemscope=\"\" itemtype=\"http://schema.org/WebPage\""), 0);
    });
}

TEST_FIXTURE(uri_address, no_transfer_encoding_content_length)
{
    handle_timeout([]
    {
        http_client client(U("http://ws.audioscrobbler.com/2.0/?method=artist.gettoptracks&artist=cher&api_key=6fcd59047568e89b1615975081258990&format=json"));

        client.request(methods::GET).then([](http_response response){
            VERIFY_ARE_EQUAL(response.status_code(), status_codes::OK);
            VERIFY_IS_FALSE(response.headers().has(header_names::content_length)
                && response.headers().has(header_names::transfer_encoding));
            return response.extract_string();
        }).then([](string_t result){
            // Verify that the body size isn't empty.
            VERIFY_IS_TRUE(result.size() > 0);
        }).wait();
    });
}

// Note additional sites for testing can be found at:
// https://www.ssllabs.com/ssltest/
// http://www.internetsociety.org/deploy360/resources/dane-test-sites/
// https://onlinessl.netlock.hu/#
TEST(server_selfsigned_cert)
{
    handle_timeout([]
    {
        http_client client(U("https://www.pcwebshop.co.uk/"));
        auto requestTask = client.request(methods::GET);
        VERIFY_THROWS(requestTask.get(), http_exception);
    });
}

TEST(server_hostname_mismatch, "Ignore", "Site fixed certificate. Improve test (new site or alternate method).")
{
    handle_timeout([]
    {
        http_client client(U("https://swordsoftruth.com/"));
        auto requestTask = client.request(methods::GET);
        VERIFY_THROWS(requestTask.get(), http_exception);
    });
}

TEST(server_cert_expired)
{
    handle_timeout([]
    {
        http_client client(U("https://tv.eurosport.com/"));
        auto requestTask = client.request(methods::GET);
        VERIFY_THROWS(requestTask.get(), http_exception);
    });
}

#if !defined(__cplusplus_winrt)
TEST(ignore_server_cert_invalid,
     "Ignore:Android", "229",
     "Ignore:Apple", "229",
     "Ignore:Linux", "229")
{
    handle_timeout([]
    {
        http_client_config config;
        config.set_validate_certificates(false);
        http_client client(U("https://www.pcwebshop.co.uk/"), config);

        auto request = client.request(methods::GET).get();
        VERIFY_ARE_EQUAL(status_codes::OK, request.status_code());
    });
}
#endif

TEST_FIXTURE(uri_address, outside_ssl_json)
{
    // Create URI for:
    // https://www.googleapis.com/youtube/v3/playlistItems?part=snippet&playlistId=UUF1hMUVwlrvlVMjUGOZExgg&key=AIzaSyAviHxf_y0SzNoAq3iKqvWVE4KQ0yylsnk
    uri_builder playlistUri(U("https://www.googleapis.com/youtube/v3/playlistItems?"));
    playlistUri.append_query(U("part"),U("snippet"));
    playlistUri.append_query(U("playlistId"), U("UUF1hMUVwlrvlVMjUGOZExgg"));
    playlistUri.append_query(U("key"), U("AIzaSyAviHxf_y0SzNoAq3iKqvWVE4KQ0yylsnk"));

    // Send request
    web::http::client::http_client playlistClient(playlistUri.to_uri());

    handle_timeout([&]
    {
        // Retry up to 4 times.
        for (int i = 0; i < 4; ++i)
        {
            try
            {
                playlistClient.request(methods::GET).then([=](http_response playlistResponse) -> pplx::task < json::value >
                {
                    return playlistResponse.extract_json();
                }).then([=](json::value v)
                {
                    int count = 0;
                    auto& obj = v.as_object();

                    VERIFY_ARE_NOT_EQUAL(obj.find(U("pageInfo")), obj.end());
                    VERIFY_ARE_NOT_EQUAL(obj.find(U("items")), obj.end());

                    auto& items = obj[U("items")];

                    for (auto iter = items.as_array().cbegin(); iter != items.as_array().cend(); ++iter)
                    {
                        const auto& item = *iter;
                        auto iSnippet = item.as_object().find(U("snippet"));
                        if (iSnippet == item.as_object().end())
                        {
                            throw std::runtime_error("snippet key not found");
                        }
                        auto iTitle = iSnippet->second.as_object().find(U("title"));
                        if (iTitle == iSnippet->second.as_object().end())
                        {
                            throw std::runtime_error("title key not found");
                        }
                        auto name = iTitle->second.serialize();
                        count++;
                    }
                    VERIFY_ARE_EQUAL(3, count); // Update this accordingly, if the number of items changes
                }).wait();
                break;
            }
            catch (web::http::http_exception const& e)
            {
#if defined(_MSC_VER) && !defined(__cplusplus_winrt)
                if (e.error_code().value() != API_QUERY_DATA_AVAILABLE || i == 3)
                {
                    // If we didn't get a "connection broken" error (or we are on the last retry), rethrow it
                    throw;
                }
#else
                CASABLANCA_UNREFERENCED_PARAMETER(e);
                throw;
#endif
                os_utilities::sleep(1000);
            }
        }
    });
}

} // SUITE(outside_tests)

}}}}
