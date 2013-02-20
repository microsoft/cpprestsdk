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
* combining_tests.cpp
*
* Tests for appending/combining features of the http::uri class.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;
using namespace http;

namespace tests { namespace functional { namespace uri_tests {

SUITE(combining_tests)
{

TEST(append_path)
{
    utility::string_t uri_str = U("http://testname.com/path?baz");
    uri_builder ub(uri_str);
    uri combined = ub.append_path(U("/baz")).to_uri();

    VERIFY_ARE_EQUAL(uri(U("http://testname.com/path/baz?baz")), combined);
}

TEST(append_empty_path)
{
    utility::string_t uri_str(U("http://fakeuri.net"));
    uri u = uri_str;
    uri_builder ub(u);
    uri combined = ub.append_path(U("")).to_uri();

    VERIFY_ARE_EQUAL(u, combined);
}

TEST(append_query)
{
    utility::string_t uri_str(U("http://testname.com/path1?key1=value2"));
    uri_builder ub(uri_str);
    uri combined = ub.append_query(uri(U("http://testname2.com/path2?key2=value3")).query()).to_uri();
    
    VERIFY_ARE_EQUAL(U("http://testname.com/path1?key1=value2&key2=value3"), combined.to_string());
}

TEST(append_empty_query)
{
    utility::string_t uri_str(U("http://fakeuri.org/?key=value"));
    uri u(uri_str);
    uri_builder ub(u);
    uri combined = ub.append_query(U("")).to_uri();

    VERIFY_ARE_EQUAL(u, combined);
}

TEST(append)
{
    utility::string_t uri_str(U("http://testname.com/path1?key1=value2"));
    uri_builder ub(uri_str);
    uri combined = ub.append(U("http://testname2.com/path2?key2=value3")).to_uri();
    
    VERIFY_ARE_EQUAL(U("http://testname.com/path1/path2?key1=value2&key2=value3"), combined.to_string());
    VERIFY_ARE_EQUAL(U("/path1/path2?key1=value2&key2=value3"), combined.resource().to_string());
}

TEST(append_empty)
{
    utility::string_t uri_str(U("http://myhost.com"));
    uri u(uri_str);
    uri_builder ub(u);
    uri combined = ub.append(U("")).to_uri();

    VERIFY_ARE_EQUAL(u, combined);
}

} // SUITE(combining_tests)

}}}
