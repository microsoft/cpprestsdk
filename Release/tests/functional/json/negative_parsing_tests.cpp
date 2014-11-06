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
* negative_parsing_tests.cpp
*
* Negative tests for JSON parsing.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; 
using namespace utility;

namespace tests { namespace functional { namespace json_tests {

#ifndef VERIFY_JSON_THROWS
#define VERIFY_JSON_THROWS(parseString) \
    { \
        std::error_code ec_; \
        VERIFY_THROWS(json::value::parse(parseString), json::json_exception); \
        auto value_ = json::value::parse(parseString, ec_); \
        VERIFY_IS_TRUE(ec_.value() > 0); \
        VERIFY_IS_TRUE(value_.is_null()); \
    }

#endif

SUITE(negative_parsing_tests)
{

TEST(string_t)
{
    VERIFY_JSON_THROWS(U("\"\\k\""));
    VERIFY_JSON_THROWS(U("\" \" \""));
    VERIFY_JSON_THROWS(U("\"\\u23A\""));
    VERIFY_JSON_THROWS(U("\"\\uXY1A\""));
    VERIFY_JSON_THROWS(U("\"asdf"));
    VERIFY_JSON_THROWS(U("\\asdf"));
    VERIFY_JSON_THROWS(U("\"\"\"\""));

    // '\', '"', and control characters must be escaped (0x1F and below).
    VERIFY_JSON_THROWS(U("\"\\\""));
    VERIFY_JSON_THROWS(U("\""));
    utility::string_t str(U("\""));
    str.append(1, 0x1F);
    str.append(U("\""));
    VERIFY_JSON_THROWS(str.c_str());
}

TEST(numbers)
{
    VERIFY_JSON_THROWS(U("-"));
    VERIFY_JSON_THROWS(U("-."));
    VERIFY_JSON_THROWS(U("-e1"));
    VERIFY_JSON_THROWS(U("-1e"));
    VERIFY_JSON_THROWS(U("+1.1"));
    VERIFY_JSON_THROWS(U("1.1 E"));
    VERIFY_JSON_THROWS(U("1.1E-"));
    VERIFY_JSON_THROWS(U("1.1E.1"));
    VERIFY_JSON_THROWS(U("1.1E1.1"));
    VERIFY_JSON_THROWS(U("001.1"));
    VERIFY_JSON_THROWS(U("-.100"));
    VERIFY_JSON_THROWS(U("-.001"));
    VERIFY_JSON_THROWS(U(".1"));
    VERIFY_JSON_THROWS(U("0.1.1"));
}

// TFS 535589
void parse_help(utility::string_t str)
{
    utility::stringstream_t ss1;
    ss1 << str;
    VERIFY_JSON_THROWS(ss1.str());
}

TEST(objects)
{
    VERIFY_JSON_THROWS(U("}"));
    parse_help(U("{"));
    parse_help(U("{ 1, 10 }"));
    parse_help(U("{ : }"));
    parse_help(U("{ \"}"));
    VERIFY_JSON_THROWS(U("{"));
    VERIFY_JSON_THROWS(U("{ 1"));
    VERIFY_JSON_THROWS(U("{ \"}"));
    VERIFY_JSON_THROWS(U("{\"2\":"));
    VERIFY_JSON_THROWS(U("{\"2\":}"));
    VERIFY_JSON_THROWS(U("{\"2\": true"));
    VERIFY_JSON_THROWS(U("{\"2\": true false"));
    VERIFY_JSON_THROWS(U("{\"2\": true :false"));
    VERIFY_JSON_THROWS(U("{\"2\": false,}"));
}

TEST(arrays)
{
    VERIFY_JSON_THROWS(U("]"));
    VERIFY_JSON_THROWS(U("["));
    VERIFY_JSON_THROWS(U("[ 1"));
    VERIFY_JSON_THROWS(U("[ 1,"));
    VERIFY_JSON_THROWS(U("[ 1,]"));
    VERIFY_JSON_THROWS(U("[ 1 2]"));
    VERIFY_JSON_THROWS(U("[ \"1\" : 2]"));
    parse_help(U("[,]"));
    parse_help(U("[ \"]"));
    parse_help(U("[\"2\", false,]"));
}

TEST(literals_not_lower_case)
{
    VERIFY_JSON_THROWS(U("NULL"));
    VERIFY_JSON_THROWS(U("FAlse"));
    VERIFY_JSON_THROWS(U("TRue"));
}

TEST(incomplete_literals)
{
    VERIFY_JSON_THROWS(U("nul"));
    VERIFY_JSON_THROWS(U("fal"));
    VERIFY_JSON_THROWS(U("tru"));
}

// TFS#501321
TEST(exception_string)
{
    utility::string_t json_ip_str=U("");
    VERIFY_JSON_THROWS(json_ip_str);
}

TEST(boundary_chars)
{
    utility::string_t str(U("\""));
    str.append(1, 0x1F);
    str.append(U("\""));
    parse_help(str);
}

TEST(stream_left_over_chars)
{
    std::stringbuf buf;
    buf.sputn("[false]false", 12);
    std::istream stream(&buf);
    VERIFY_THROWS(web::json::value::parse(stream), json::json_exception);
}

// Test using Windows only API.
#ifdef _MS_WINDOWS
TEST(wstream_left_over_chars)
{
    std::wstringbuf buf;
    buf.sputn(L"[false]false", 12);
    std::wistream stream(&buf);
    VERIFY_THROWS(web::json::value::parse(stream), json::json_exception);
}
#endif

void garbage_impl(wchar_t ch)
{
    utility::string_t ss(U("{\"a\" : 10, \"b\":"));

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<unsigned int> dist(0, ch);

    for (int i = 0; i < 2500; i++)
        ss.push_back(static_cast<char_t>(dist(eng)));

    VERIFY_JSON_THROWS(ss.c_str());
}

TEST(garbage_1)
{
    garbage_impl(0x7F);
}

TEST(garbage_2)
{
    garbage_impl(0xFF);
}

TEST(garbage_3)
{
    garbage_impl(0xFFFF);
}

}

}}}