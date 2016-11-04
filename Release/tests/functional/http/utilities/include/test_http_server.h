/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* test_http_server.h -- Defines a test server to handle requests and sending responses.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <map>
#include <sstream>

#include "unittestpp.h"
#include "cpprest/uri.h"
#include "http_test_utilities_public.h"

namespace tests { namespace functional { namespace http { namespace utilities {

/// <summary>
/// Actual implementation of test_http_server is in this class.
/// This wrapping is done to hide the fact we are using Windows HTTP Server APIs
/// from users of this test library.
/// </summary>
class _test_http_server;

/// <summary>
/// Structure for storing HTTP request information and responding to requests.
/// </summary>
class test_request
{
public:

    // APIs to send responses.
    TEST_UTILITY_API unsigned long reply(const unsigned short status_code);
    TEST_UTILITY_API unsigned long reply(const unsigned short status_code, const utility::string_t &reason_phrase);
    TEST_UTILITY_API unsigned long reply(
        const unsigned short status_code,
        const utility::string_t &reason_phrase,
        const std::map<utility::string_t, utility::string_t> &headers);
    TEST_UTILITY_API unsigned long reply(
        const unsigned short status_code,
        const utility::string_t &reason_phrase,
        const std::map<utility::string_t, utility::string_t> &headers,
        const utf8string &data);
    TEST_UTILITY_API unsigned long reply(
        const unsigned short status_code,
        const utility::string_t &reason_phrase,
        const std::map<utility::string_t, utility::string_t> &headers,
        const utf16string &data);

    // API to check if a specific header exists and get it.
    template <typename T>
    bool match_header(const utility::string_t & header_name, T & header_value)
    {
        auto iter = m_headers.find(header_name);
        if (iter != m_headers.end())
        {
            utility::istringstream_t iss(iter->second);
            iss >> header_value;
            if (iss.fail() || !iss.eof())
            {
                return false;
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    // Request data.
    utility::string_t m_method;
    utility::string_t m_path;
    std::map<utility::string_t, utility::string_t> m_headers;
    std::vector<unsigned char> m_body;
private:
    friend class _test_http_server;
    _test_http_server * m_p_server;

    // This is the HTTP Server API Request Id, we don't want to bring in the header file.
    unsigned long long m_request_id;

    // Helper to send replies.
    unsigned long reply_impl(
        const unsigned short status_code, 
        const utility::string_t &reason_phrase, 
        const std::map<utility::string_t, utility::string_t> &headers,
        void * data,
        size_t data_length);
};

/// <summary>
/// Basic HTTP server for testing. Supports waiting and collecting together requests.
///
/// NOTE: this HTTP server is not concurrency safe. I.e. only one thread at a time should use it.
/// </summary>
class test_http_server
{
public:
    TEST_UTILITY_API test_http_server(const web::http::uri &uri);
    TEST_UTILITY_API ~test_http_server();

    // APIs to open and close requests.
    TEST_UTILITY_API unsigned long open();
    TEST_UTILITY_API unsigned long close();

    // APIs to receive requests.
    TEST_UTILITY_API test_request * wait_for_request();
    TEST_UTILITY_API pplx::task<test_request *> next_request();
    TEST_UTILITY_API std::vector<test_request *> wait_for_requests(const size_t count);
    TEST_UTILITY_API std::vector<pplx::task<test_request *>> next_requests(const size_t count);

    // RAII pattern for test_http_server.
    class scoped_server
    {
    public:
        scoped_server(const web::http::uri &uri) 
        {
            m_p_server = new test_http_server(uri);
            VERIFY_ARE_EQUAL(0u, m_p_server->open());
        }
        ~scoped_server()
        {
            VERIFY_ARE_EQUAL(0u, m_p_server->close());
            delete m_p_server;
        }
        test_http_server *server() { return m_p_server; }
    private:
        test_http_server * m_p_server;
    };

private:
    _test_http_server *m_p_impl;
};

}}}}
