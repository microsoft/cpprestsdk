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
* constructor_tests.cpp
*
* Tests for constructors of the uri class.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace utility;

namespace tests { namespace functional { namespace uri_tests {

SUITE(constructor_tests)
{

TEST(parsing_constructor_char)
{
    uri u(uri::encode_uri(U("net.tcp://steve:@testname.com:81/bleh%?qstring#goo")));

    VERIFY_ARE_EQUAL(U("net.tcp"), u.scheme());
    VERIFY_ARE_EQUAL(U("steve:"), u.user_info());
    VERIFY_ARE_EQUAL(U("testname.com"), u.host());
    VERIFY_ARE_EQUAL(81, u.port());
    VERIFY_ARE_EQUAL(U("/bleh%25"), u.path());
    VERIFY_ARE_EQUAL(U("qstring"), u.query());
    VERIFY_ARE_EQUAL(U("goo"), u.fragment());
}

TEST(parsing_constructor_encoded_string)
{
    uri u(uri::encode_uri(U("net.tcp://testname.com:81/bleh%?qstring#goo")));

    VERIFY_ARE_EQUAL(U("net.tcp"), u.scheme());
    VERIFY_ARE_EQUAL(U("testname.com"), u.host());
    VERIFY_ARE_EQUAL(81, u.port());
    VERIFY_ARE_EQUAL(U("/bleh%25"), u.path());
    VERIFY_ARE_EQUAL(U("qstring"), u.query());
    VERIFY_ARE_EQUAL(U("goo"), u.fragment());
}

TEST(parsing_constructor_string_string)
{
    uri u(uri::encode_uri(U("net.tcp://testname.com:81/bleh%?qstring#goo")));

    VERIFY_ARE_EQUAL(U("net.tcp"), u.scheme());
    VERIFY_ARE_EQUAL(U("testname.com"), u.host());
    VERIFY_ARE_EQUAL(81, u.port());
    VERIFY_ARE_EQUAL(U("/bleh%25"), u.path());
    VERIFY_ARE_EQUAL(U("qstring"), u.query());
    VERIFY_ARE_EQUAL(U("goo"), u.fragment());
}

TEST(empty_strings)
{
    VERIFY_IS_TRUE(uri(U("")).is_empty());
    VERIFY_IS_TRUE(uri(U("")).is_empty());
    VERIFY_IS_TRUE(uri(uri::encode_uri(U(""))).is_empty());
}

TEST(default_constructor)
{
    VERIFY_IS_TRUE(uri().is_empty());
}

TEST(relative_ref_string)
{
    uri u(uri::encode_uri(U("first/second#boff")));

    VERIFY_ARE_EQUAL(U(""), u.scheme());
    VERIFY_ARE_EQUAL(U(""), u.host());
    VERIFY_ARE_EQUAL(0, u.port());
    VERIFY_ARE_EQUAL(U("first/second"), u.path());
    VERIFY_ARE_EQUAL(U(""), u.query());
    VERIFY_ARE_EQUAL(U("boff"), u.fragment());
}

TEST(absolute_ref_string)
{
    uri u(uri::encode_uri(U("/first/second#boff")));

    VERIFY_ARE_EQUAL(U(""), u.scheme());
    VERIFY_ARE_EQUAL(U(""), u.host());
    VERIFY_ARE_EQUAL(0, u.port());
    VERIFY_ARE_EQUAL(U("/first/second"), u.path());
    VERIFY_ARE_EQUAL(U(""), u.query());
    VERIFY_ARE_EQUAL(U("boff"), u.fragment());
}

TEST(copy_constructor)
{
    uri original(U("http://st:pass@localhost:456/path1?qstring#goo"));
    uri new_uri(original);

    VERIFY_ARE_EQUAL(original, new_uri);
}

TEST(move_constructor)
{
    const utility::string_t uri_str(U("http://localhost:456/path1?qstring#goo"));
    uri original(uri_str);
    uri new_uri = std::move(original);

    VERIFY_ARE_EQUAL(uri_str, new_uri.to_string());
    VERIFY_ARE_EQUAL(uri(uri_str), new_uri);
}

TEST(assignment_operator)
{
    uri original(U("http://localhost:456/path?qstring#goo"));
    uri new_uri = original;

    VERIFY_ARE_EQUAL(original, new_uri);
}

// Tests invalid URI being passed in constructor.
TEST(parsing_constructor_invalid)
{
    VERIFY_THROWS(uri(U("123http://localhost:345/")), uri_exception);
    VERIFY_THROWS(uri(U("h*ttp://localhost:345/")), uri_exception);
    VERIFY_THROWS(uri(U("http://localhost:345/\"")), uri_exception);
    VERIFY_THROWS(uri(U("http://localhost:345/path?\"")), uri_exception);
    VERIFY_THROWS(uri(U("http://local\"host:345/")), uri_exception);
}

// Tests a variety of different URIs using the examples in RFC 3986.
TEST(RFC_3968_examples_string)
{
    uri ftp(U("ftp://ftp.is.co.za/rfc/rfc1808.txt"));
    VERIFY_ARE_EQUAL(U("ftp"), ftp.scheme());
    VERIFY_ARE_EQUAL(U(""), ftp.user_info());
    VERIFY_ARE_EQUAL(U("ftp.is.co.za"), ftp.host());
    VERIFY_ARE_EQUAL(0, ftp.port());
    VERIFY_ARE_EQUAL(U("/rfc/rfc1808.txt"), ftp.path());
    VERIFY_ARE_EQUAL(U(""), ftp.query());
    VERIFY_ARE_EQUAL(U(""), ftp.fragment());

    // TFS #371892
    //uri ldap(U("ldap://[2001:db8::7]/?c=GB#objectClass?one"));
    //VERIFY_ARE_EQUAL(U("ldap"), ldap.scheme());
    //VERIFY_ARE_EQUAL(U(""), ldap.user_info());
    //VERIFY_ARE_EQUAL(U("2001:db8::7"), ldap.host());
    //VERIFY_ARE_EQUAL(0, ldap.port());
    //VERIFY_ARE_EQUAL(U("/"), ldap.path());
    //VERIFY_ARE_EQUAL(U("c=GB"), ldap.query());
    //VERIFY_ARE_EQUAL(U("objectClass?one"), ldap.fragment());

    // We don't support anything scheme specific like in C# so
    // these common ones don't have a great experience yet.
    uri mailto(U("mailto:John.Doe@example.com"));
    VERIFY_ARE_EQUAL(U("mailto"), mailto.scheme());
    VERIFY_ARE_EQUAL(U(""), mailto.user_info());
    VERIFY_ARE_EQUAL(U(""), mailto.host());
    VERIFY_ARE_EQUAL(0, mailto.port());
    VERIFY_ARE_EQUAL(U("John.Doe@example.com"), mailto.path());
    VERIFY_ARE_EQUAL(U(""), mailto.query());
    VERIFY_ARE_EQUAL(U(""), mailto.fragment());

    uri tel(U("tel:+1-816-555-1212"));
    VERIFY_ARE_EQUAL(U("tel"), tel.scheme());
    VERIFY_ARE_EQUAL(U(""), tel.user_info());
    VERIFY_ARE_EQUAL(U(""), tel.host());
    VERIFY_ARE_EQUAL(0, tel.port());
    VERIFY_ARE_EQUAL(U("+1-816-555-1212"), tel.path());
    VERIFY_ARE_EQUAL(U(""), tel.query());
    VERIFY_ARE_EQUAL(U(""), tel.fragment());

    uri telnet(U("telnet://192.0.2.16:80/"));
    VERIFY_ARE_EQUAL(U("telnet"), telnet.scheme());
    VERIFY_ARE_EQUAL(U(""), telnet.user_info());
    VERIFY_ARE_EQUAL(U("192.0.2.16"), telnet.host());
    VERIFY_ARE_EQUAL(80, telnet.port());
    VERIFY_ARE_EQUAL(U("/"), telnet.path());
    VERIFY_ARE_EQUAL(U(""), telnet.query());
    VERIFY_ARE_EQUAL(U(""), telnet.fragment());
}

TEST(user_info_string)
{
    uri ftp(U("ftp://johndoe:testname@ftp.is.co.za/rfc/rfc1808.txt"));
    VERIFY_ARE_EQUAL(U("ftp"), ftp.scheme());
    VERIFY_ARE_EQUAL(U("johndoe:testname"), ftp.user_info());
    VERIFY_ARE_EQUAL(U("ftp.is.co.za"), ftp.host());
    VERIFY_ARE_EQUAL(0, ftp.port());
    VERIFY_ARE_EQUAL(U("/rfc/rfc1808.txt"), ftp.path());
    VERIFY_ARE_EQUAL(U(""), ftp.query());
    VERIFY_ARE_EQUAL(U(""), ftp.fragment());
}

// Test query component can be separated with '&' or ';'.
TEST(query_seperated_with_semi_colon)
{
    uri u(U("http://localhost/path1?key1=val1;key2=val2"));
    VERIFY_ARE_EQUAL(U("key1=val1;key2=val2"), u.query());
}

} // SUITE(constructor_tests)

}}}
