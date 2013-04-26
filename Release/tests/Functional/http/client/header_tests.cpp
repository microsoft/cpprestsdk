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
* header_tests.cpp
*
* Tests cases for http_headers.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(outside_tests)
{

TEST_FIXTURE(uri_address, request_headers)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    http_request msg(methods::POST);

#ifndef __cplusplus_winrt
    // The WinRT-based HTTP stack does not support headers that have no
    // value, which means that there is no point in making this particular
    // header test, it is an unsupported feature on WinRT.
    msg.headers().add(U("HEHE"), U(""));
#endif 
    
    msg.headers().add(U("MyHeader"), U("hehe;blach"));
    msg.headers().add(U("Yo1"), U("You, Too"));
    msg.headers().add(U("Yo2"), U("You2"));
    msg.headers().add(U("Yo3"), U("You3"));
    msg.headers().add(U("Yo4"), U("You4"));
    msg.headers().add(U("Yo5"), U("You5"));
    msg.headers().add(U("Yo6"), U("You6"));
    msg.headers().add(U("Yo7"), U("You7"));
    msg.headers().add(U("Yo8"), U("You8"));
    msg.headers().add(U("Yo9"), U("You9"));
    msg.headers().add(U("Yo10"), U("You10"));
    msg.headers().add(U("Yo11"), U("You11"));
    msg.headers().add(U("Accept"), U("text/plain"));
    VERIFY_ARE_EQUAL(U("You5"), msg.headers()[U("Yo5")]);
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
        http_asserts::assert_test_request_contains_headers(p_request, msg.headers());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, field_name_casing)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);
    const method mtd = methods::GET;
    const utility::string_t field_name1 = U("CustomHeader");
    const utility::string_t field_name2 = U("CUSTOMHEADER");
    const utility::string_t field_name3 = U("CuSTomHEAdeR");
    const utility::string_t value1 = U("value1");
    const utility::string_t value2 = U("value2");
    const utility::string_t value3 = U("value3");

    http_request msg(mtd);
    msg.headers()[field_name1] = value1;
    msg.headers()[field_name2].append(U(", ") + value2);
    msg.headers()[field_name3].append(U(", ") + value3);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));
        std::map<utility::string_t, utility::string_t> expected_headers;
        expected_headers[field_name1] = value1 + U(", ") + value2 + U(", ") + value3;
        http_asserts::assert_test_request_contains_headers(p_request, expected_headers);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, copy_move)
{
    // copy constructor
    http_headers h1;
    h1.add(U("key1"), U("key2"));
    http_headers h2(h1);
    http_asserts::assert_http_headers_equals(h1, h2);

    // move constructor
    http_headers h3(std::move(h1));
    VERIFY_ARE_EQUAL(1u, h3.size());
    VERIFY_ARE_EQUAL(U("key2"), h3[U("key1")]);

    // assignment operator
    h1 = h3;
    VERIFY_ARE_EQUAL(1u, h1.size());
    VERIFY_ARE_EQUAL(U("key2"), h1[U("key1")]);
    http_asserts::assert_http_headers_equals(h1, h3);

    // move assignment operator
    h1 = http_headers();
    h1 = std::move(h2);
    VERIFY_ARE_EQUAL(1u, h1.size());
    VERIFY_ARE_EQUAL(U("key2"), h1[U("key1")]);
}

TEST_FIXTURE(uri_address, match_types)
{
    // wchar
    http_headers h1;
    h1[U("key1")] = U("string");
    utility::char_t buf[12];
    VERIFY_IS_TRUE(h1.match(U("key1"), buf));
    VERIFY_ARE_EQUAL(U("string"), utility::string_t(buf));
    
    // utility::string_t
    utility::string_t wstr;
    VERIFY_IS_TRUE(h1.match(U("key1"), wstr));
    VERIFY_ARE_EQUAL(U("string"), wstr); 

    // int
    h1[U("key2")] = U("22");
    int i;
    VERIFY_IS_TRUE(h1.match(U("key2"), i));
    VERIFY_ARE_EQUAL(22, i);

    // unsigned long
    unsigned long l;
    VERIFY_IS_TRUE(h1.match(U("key2"), l));
    VERIFY_ARE_EQUAL(22ul, l);
}

TEST_FIXTURE(uri_address, match_edge_cases)
{
    // match with empty string
    http_headers h;
    h[U("here")] = U("");
    utility::string_t value(U("k"));
    VERIFY_IS_TRUE(h.match(U("HeRE"), value));
    VERIFY_ARE_EQUAL(U(""), value);

    // match with string containing spaces
    h.add(U("blah"), U("spaces ss"));
    VERIFY_IS_TRUE(h.match(U("blah"), value));
    VERIFY_ARE_EQUAL(U("spaces ss"), value);

    // match failing
    value = utility::string_t();
    VERIFY_IS_FALSE(h.match(U("hahah"), value));
    VERIFY_ARE_EQUAL(U(""), value);
}

TEST_FIXTURE(uri_address, headers_find)
{
    // Find when empty.
    http_headers h;
    VERIFY_ARE_EQUAL(h.end(), h.find(U("key1")));

    // Find that exists.
    h[U("key1")] = U("yes");
    VERIFY_ARE_EQUAL(U("yes"), h.find(U("key1"))->second);

    // Find that doesn't exist.
    VERIFY_ARE_EQUAL(h.end(), h.find(U("key2")));
}

TEST_FIXTURE(uri_address, headers_add)
{
    // Add multiple
    http_headers h;
    h.add(U("key1"), 22);
    h.add(U("key2"), U("str2"));
    VERIFY_ARE_EQUAL(U("22"), h[U("key1")]);
    VERIFY_ARE_EQUAL(U("str2"), h[U("key2")]);

    // Add one that already exists
    h.add(U("key2"), U("str3"));
    VERIFY_ARE_EQUAL(U("str3"), h[U("key2")]);

    // Add with different case
    h.add(U("KEY2"), U("str4"));
    VERIFY_ARE_EQUAL(U("str4"), h[U("keY2")]);

    // Add with spaces in string
    h.add(U("key3"), U("value with spaces"));
    VERIFY_ARE_EQUAL(U("value with spaces"), h[U("key3")]);
}

TEST_FIXTURE(uri_address, headers_iterators)
{
    // begin when empty
    http_headers h;
    VERIFY_ARE_EQUAL(h.begin(), h.end());
    
    // with some values.
    h.add(U("key1"), U("value1"));
    h.add(U("key2"), U("value2"));
    h.add(U("key3"), U("value3"));
    http_headers::const_iterator iter = h.begin();
    VERIFY_ARE_EQUAL(U("value1"), iter->second);
    ++iter;
    VERIFY_ARE_EQUAL(U("value2"), iter->second);
    ++iter;
    VERIFY_ARE_EQUAL(U("value3"), iter->second);
    ++iter;
    VERIFY_ARE_EQUAL(h.end(), iter);
}

TEST_FIXTURE(uri_address, headers_foreach)
{
    // begin when empty
    http_headers h;
    VERIFY_ARE_EQUAL(h.begin(), h.end());
    
    // with some values.
    h.add(U("key1"), U("value"));
    h.add(U("key2"), U("value"));
    h.add(U("key3"), U("value"));

    std::for_each(std::begin(h), std::end(h),
    [=](http_headers::const_reference kv)
    {
        VERIFY_ARE_EQUAL(U("value"), kv.second);
    });

    std::for_each(std::begin(h), std::end(h),
    [=](http_headers::reference kv)
    {
        VERIFY_ARE_EQUAL(U("value"), kv.second);
    });
}

TEST_FIXTURE(uri_address, response_headers)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    std::map<utility::string_t, utility::string_t> headers;
    headers[U("H1")] = U("");
    headers[U("H2")] = U("hah");
    headers[U("H3")] = U("es");
    headers[U("H4")] = U("es;kjr");
    headers[U("H5")] = U("asb");
    headers[U("H6")] = U("abc");
    headers[U("H7")] = U("eds");
    headers[U("H8")] = U("blue");
    headers[U("H9")] = U("sd");
    headers[U("H10")] = U("res");
    test_server_utilities::verify_request(&client, methods::GET, U("/"), scoped.server(), status_codes::OK, headers);
}

TEST_FIXTURE(uri_address, cache_control_header)
{
    http_headers headers;
    VERIFY_ARE_EQUAL(headers.cache_control(), U(""));
    const utility::string_t value(U("custom value"));
    headers.set_cache_control(value);
    VERIFY_ARE_EQUAL(headers.cache_control(), value);
    utility::string_t foundValue;
    VERIFY_IS_TRUE(headers.match(header_names::cache_control, foundValue));
    VERIFY_ARE_EQUAL(value, foundValue);
}

TEST_FIXTURE(uri_address, content_length_header)
{
    http_headers headers;
    VERIFY_ARE_EQUAL(headers.content_length(), 0);
    const size_t value = 44;
    headers.set_content_length(value);
    VERIFY_ARE_EQUAL(headers.content_length(), value);
    size_t foundValue;
    VERIFY_IS_TRUE(headers.match(header_names::content_length, foundValue));
    VERIFY_ARE_EQUAL(value, foundValue);
}

TEST_FIXTURE(uri_address, date_header)
{
    http_headers headers;
    VERIFY_ARE_EQUAL(headers.date(), U(""));
    const utility::datetime value(utility::datetime::utc_now());
    headers.set_date(value);
    VERIFY_ARE_EQUAL(headers.date(), value.to_string());
    utility::string_t foundValue;
    VERIFY_IS_TRUE(headers.match(header_names::date, foundValue));
    VERIFY_ARE_EQUAL(value.to_string(), foundValue);
}

} // SUITE(header_tests)

}}}}