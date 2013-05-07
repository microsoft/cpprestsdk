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
* parsing_tests.cpp
*
* Tests for JSON parsing.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;
using namespace utility::conversions;

namespace tests { namespace functional { namespace json_tests {

SUITE(parsing_tests)
{

TEST(stringstream_t)
{
    utility::stringstream_t ss1;
    ss1 << U("17");
    json::value v1 = json::value::parse(ss1);

    utility::stringstream_t ss2;
    ss2 << U("3.1415");
    json::value v2 = json::value::parse(ss2);

    utility::stringstream_t ss3;
    ss3 << U("true");
    json::value v3 = json::value::parse(ss3);

    utility::stringstream_t ss4;
    ss4 << U("\"Hello!\"");
    json::value v4 = json::value::parse(ss4);

    utility::stringstream_t ss8;
    ss8 << U("{ \"a\" : 10 }");
    json::value v8 = json::value::parse(ss8);

    utility::stringstream_t ss9;
    ss9 << U("[1,2,3,true]");
    json::value v9 = json::value::parse(ss9);

    VERIFY_ARE_EQUAL(v1.type(), json::value::Number);
    VERIFY_ARE_EQUAL(v2.type(), json::value::Number);
    VERIFY_ARE_EQUAL(v3.type(), json::value::Boolean);
    VERIFY_ARE_EQUAL(v4.type(), json::value::String);
    VERIFY_ARE_EQUAL(v8.type(), json::value::Object);
    VERIFY_ARE_EQUAL(v9.type(), json::value::Array);
}

TEST(whitespace)
{
    VERIFY_THROWS(json::value::parse(U("  ")), json::json_exception);

    // Try all the whitespace characters before/after all the structural characters
    // whitespace characters according to RFC4627: space, horizontal tab, line feed or new line, carriage return
    // structural characters: [{]}:,
    const int num_whitespace = 4;
    char whitespace_chars[num_whitespace] = { 0x20, 0x09, 0x0A, 0x0D };

    // [,]
    for(int i = 0; i < num_whitespace; ++i)
    {
        utility::string_t input;
        input.append(2, whitespace_chars[i]);
        input.append(U("["));
        input.append(2, whitespace_chars[i]);
        input.append(U("1"));
        input.append(1, whitespace_chars[i]);
        input.append(U(","));
        input.append(4, whitespace_chars[i]);
        input.append(U("2"));
        input.append(1, whitespace_chars[i]);
        input.append(U("]"));
        input.append(2, whitespace_chars[i]);
        json::value val = json::value::parse(input);
        VERIFY_IS_TRUE(val.is_array());
        VERIFY_ARE_EQUAL(U("1"), val[0].to_string());
        VERIFY_ARE_EQUAL(U("2"), val[1].to_string());
    }

    // {:}
    for(int i = 0; i < num_whitespace; ++i)
    {
        utility::string_t input;
        input.append(2, whitespace_chars[i]);
        input.append(U("{"));
        input.append(2, whitespace_chars[i]);
        input.append(U("\"1\""));
        input.append(1, whitespace_chars[i]);
        input.append(U(":"));
        input.append(4, whitespace_chars[i]);
        input.append(U("2"));
        input.append(1, whitespace_chars[i]);
        input.append(U("}"));
        input.append(2, whitespace_chars[i]);
        json::value val = json::value::parse(input);
        VERIFY_IS_TRUE(val.is_object());
        VERIFY_ARE_EQUAL(U("2"), val[U("1"]).to_string());
    }
}

TEST(numbers)
{
    json::value num = json::value::parse(U("-22"));
    VERIFY_ARE_EQUAL(-22, num.as_double());

    num = json::value::parse(U("-1.45E2"));
    VERIFY_IS_TRUE(num.is_number());

    num = json::value::parse(U("-1.45E+1"));
    VERIFY_IS_TRUE(num.is_number());

    num = json::value::parse(U("-1.45E-10"));
    VERIFY_IS_TRUE(num.is_number());

    num = json::value::parse(U("1e01"));
    VERIFY_IS_TRUE(num.is_number());
}

TEST(string_t)
{
    json::value str = json::value::parse(U("\"\\\"\""));
    VERIFY_ARE_EQUAL(U("\""), str.as_string());

    str = json::value::parse(U("\"\""));
    VERIFY_ARE_EQUAL(U(""), str.as_string());

    str = json::value::parse(U("\"\\\"ds\""));
    VERIFY_ARE_EQUAL(U("\"ds"), str.as_string());

    str = json::value::parse(U("\"\\\"\\\"\""));
    VERIFY_ARE_EQUAL(U("\"\""), str.as_string());

    // two character escapes
    str = json::value::parse(U("\"\\\\\""));
    VERIFY_ARE_EQUAL(U("\\"), str.as_string());

    str = json::value::parse(U("\"\\/\""));
    VERIFY_ARE_EQUAL(U("/"), str.as_string());

    str = json::value::parse(U("\"\\b\""));
    VERIFY_ARE_EQUAL(U("\b"), str.as_string());

    str = json::value::parse(U("\"\\f\""));
    VERIFY_ARE_EQUAL(U("\f"), str.as_string());

    str = json::value::parse(U("\"\\n\""));
    VERIFY_ARE_EQUAL(U("\n"), str.as_string());

    str = json::value::parse(U("\"\\r\""));
    VERIFY_ARE_EQUAL(U("\r"), str.as_string());

    str = json::value::parse(U("\"\\t\""));
    VERIFY_ARE_EQUAL(U("\t"), str.as_string());

    str = json::value::parse(U("\"\\u0041\""));
    VERIFY_ARE_EQUAL(U("A"), str.as_string());

    str = json::value::parse(U("\"\\u004B\""));
    VERIFY_ARE_EQUAL(U("K"), str.as_string());
}

TEST(empty_object_array)
{
    json::value obj = json::value::parse(U("{}"));
    VERIFY_IS_TRUE(obj.is_object());
    VERIFY_ARE_EQUAL(0u, obj.size());

    json::value arr = json::value::parse(U("[]"));
    VERIFY_IS_TRUE(arr.is_array());
    VERIFY_ARE_EQUAL(0u, arr.size());
}

TEST(bug_416116)
{
    json::value data2 = json::value::parse(U("\"δοκιμή\""));
    auto s = data2.to_string();

#pragma warning( push )
#pragma warning( disable : 4566 )
    VERIFY_ARE_EQUAL(s, U("\"δοκιμή\""));
#pragma warning( pop )
}

// TODO: re-enable some of these for Linux, TFS#535122.

TEST(byte_ptr_parsing_array)
{
    char s[] = "[ \"foo\", true ]";
    std::stringstream ss;
    ss << s;
    json::value v = json::value::parse(ss);
    auto s2 = v.to_string();

    VERIFY_ARE_EQUAL(s2, U("[ \"foo\", true ]"));

    std::stringstream os;
    v.serialize(os);
    VERIFY_ARE_EQUAL(s2, to_string_t(os.str()));
}

TEST(byte_ptr_parsing_object)
{
    char s[] = "{ \"foo\" : true }";
    std::stringstream ss;
    ss << s;
    json::value v = json::value::parse(ss);
    auto s2 = v.to_string();

    VERIFY_ARE_EQUAL(s2, U("{ \"foo\" : true }"));

    std::stringstream os;
    v.serialize(os);
    VERIFY_ARE_EQUAL(s2, to_string_t(os.str()));
}

TEST(Konnichiwa)
{
    utility::string_t ws = U("\"こんにちは\"");
    std::string s = to_utf8string(ws);

    std::stringstream ss;
    ss << s;
    json::value v = json::value::parse(ss);
    auto s2 = v.to_string();

    VERIFY_ARE_EQUAL(s2, ws);

    std::stringstream os;
    v.serialize(os);
    VERIFY_ARE_EQUAL(s2, to_string_t(os.str()));
}

TEST(Russian)
{
    utility::string_t ws = U("{ \"results\" : [ { \"id\" : 272655310, \"name\" : \"Андрей Иванов\" } ] }");

    json::value v1 = json::value::parse(ws);
    auto s2 = v1.to_string();

    VERIFY_ARE_EQUAL(s2, ws);

    std::string s = to_utf8string(ws);

    std::stringstream ss;
    ss << s;
    json::value v2 = json::value::parse(ss);
    auto s3 = v2.to_string();

    VERIFY_ARE_EQUAL(s3, ws);
}

utility::string_t make_deep_json_string(size_t depth)
{
    utility::string_t strval;
    for(size_t i=0; i<depth; ++i)
    {
        strval += U("{ \"a\" : 10, \"b\" : ");
    }
    strval += U("20");
    for(size_t i=0; i<depth; ++i)
    {
        strval += U("}");
    }
    return strval;
}

TEST(deeply_nested)
{
    auto strGood = make_deep_json_string(127);
    // This should parse without issues:
    json::value::parse(strGood);

    auto strBad = make_deep_json_string(128);

    // But this one should throw:
    VERIFY_THROWS(json::value::parse(strBad), json::json_exception);
}

} // SUITE(parsing_tests)

}}}
