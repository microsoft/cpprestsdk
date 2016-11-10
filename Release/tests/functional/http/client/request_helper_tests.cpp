/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* request_helper_tests.cpp
*
* Tests cases for the convenience helper functions for making requests on http_client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#include <fstream>

#include "cpprest/version.h"
#include "cpprest/details/http_helpers.h"

using namespace web; 
using namespace utility;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(request_helper_tests)
{

TEST_FIXTURE(uri_address, compress_and_decompress)
{
    if (web::http::details::compression::stream_compressor::is_supported())
    {
        auto compress_and_decompress = [](web::http::details::compression::compression_algorithm alg)
        {
            auto compressor = std::make_shared<web::http::details::compression::stream_compressor>(alg);
            auto decompressor = std::make_shared<web::http::details::compression::stream_decompressor>(alg);

            const size_t buffer_size = 100;
            const size_t split_pos = buffer_size / 2;

            web::http::details::compression::data_buffer input_buffer;
            input_buffer.reserve(buffer_size);

            for (size_t i = 0; i < buffer_size; ++i)
            {
                input_buffer.push_back(static_cast<uint8_t>(i));
            }

            web::http::details::compression::data_buffer buffer1(input_buffer.begin(), input_buffer.begin() + split_pos);
            web::http::details::compression::data_buffer buffer2(input_buffer.begin() + split_pos, input_buffer.end());

            auto compressed_data1 = compressor->compress(buffer1, false);
            VERIFY_IS_FALSE(compressed_data1.empty());
            VERIFY_IS_FALSE(compressor->has_error());

            auto compressed_data2 = compressor->compress(buffer2, true);
            VERIFY_IS_FALSE(compressed_data2.empty());
            VERIFY_IS_FALSE(compressor->has_error());

            auto decompressed_data1 = decompressor->decompress(compressed_data1);
            VERIFY_IS_FALSE(decompressed_data1.empty());
            VERIFY_IS_FALSE(decompressor->has_error());

            auto decompressed_data2 = decompressor->decompress(compressed_data2);
            VERIFY_IS_FALSE(decompressed_data2.empty());
            VERIFY_IS_FALSE(decompressor->has_error());

            decompressed_data1.insert(decompressed_data1.end(), decompressed_data2.begin(), decompressed_data2.end());

            VERIFY_ARE_EQUAL(input_buffer, decompressed_data1);
        };

        compress_and_decompress(web::http::details::compression::compression_algorithm::gzip);
        compress_and_decompress(web::http::details::compression::compression_algorithm::deflate);
    }
}

TEST_FIXTURE(uri_address, non_rvalue_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    // Without content type.
    utility::string_t send_body = U("YES NOW SEND THE TROOPS!");
    p_server->next_request().then([&send_body](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("text/plain; charset=utf-8"), send_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), send_body).get(), status_codes::OK);
    
    // With content type.
    utility::string_t content_type = U("custom_content");
    test_server_utilities::verify_request(&client, methods::PUT, U("/"), content_type, send_body, p_server, status_codes::OK, U("OK"));

    // Empty body type
    send_body.clear();
    content_type = U("haha_type");
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), content_type);
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        VERIFY_ARE_EQUAL(0u, p_request->reply(status_codes::OK, U("OK")));
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U("/"), send_body, content_type).get(), status_codes::OK, U("OK"));
}

TEST_FIXTURE(uri_address, rvalue_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    // Without content type.
    utility::string_t send_body = U("YES NOW SEND THE TROOPS!");
    utility::string_t move_body = send_body;
    p_server->next_request().then([&send_body](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("text/plain; charset=utf-8"), send_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), std::move(move_body)).get(), status_codes::OK);

    // With content type.
    utility::string_t content_type = U("custom_content");
    move_body = send_body;
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), content_type, send_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), std::move(move_body), content_type).get(), status_codes::OK);

    // Empty body.
    content_type = U("haha_type");
    send_body.clear();
    move_body = send_body;
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), content_type);
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), std::move(move_body), content_type).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, json_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    // JSON bool value.
    json::value bool_value = json::value::boolean(true);
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("application/json"), bool_value.serialize());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U("/"), bool_value).get(), status_codes::OK);

    // JSON null value.
    json::value null_value = json::value::null();
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("application/json"), null_value.serialize());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), null_value).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, non_rvalue_2k_body)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    std::string body;
    for(int i = 0; i < 2048; ++i)
    {
        body.append(1, (char)('A' + (i % 26)));
    }
    test_server_utilities::verify_request(&client, methods::PUT, U("/"), U("text/plain"), ::utility::conversions::to_string_t(body), p_server, status_codes::OK, U("OK"));
}

TEST_FIXTURE(uri_address, default_user_agent)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    p_server->next_request().then([&](test_request *p_request)
    {
        utility::stringstream_t stream;
        stream << _XPLATSTR("cpprestsdk/") << CPPREST_VERSION_MAJOR << _XPLATSTR(".") << CPPREST_VERSION_MINOR << _XPLATSTR(".") << CPPREST_VERSION_REVISION;
        utility::string_t foundHeader;
        p_request->match_header(U("User-Agent"), foundHeader);
        VERIFY_ARE_EQUAL(stream.str(), foundHeader);
        
        p_request->reply(200);
    });

    http_asserts::assert_response_equals(client.request(methods::GET).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, overwrite_user_agent)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    utility::string_t customUserAgent(U("MyAgent"));
    p_server->next_request().then([&](test_request *p_request)
    {
        utility::string_t foundHeader;
        p_request->match_header(U("User-Agent"), foundHeader);
        VERIFY_ARE_EQUAL(customUserAgent, foundHeader);
        
        p_request->reply(200);
    });

    http_request request(methods::GET);
    request.headers()[U("User-Agent")] = customUserAgent;
    http_asserts::assert_response_equals(client.request(request).get(), status_codes::OK);
}

} // SUITE(request_helper_tests)

}}}}
