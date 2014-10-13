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
* conversion_tests.cpp
*
* Tests to string functions and implicit conversions of the http::uri class.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace utility;

namespace tests { namespace functional { namespace uri_tests {

SUITE(conversions_tests)
{

TEST(to_string_conversion)
{
    utility::string_t encoded = uri::encode_uri(U("http://testname.com/%%?qstring"));
    uri u1(U("http://testname.com/%25%25?qstring"));
    
    VERIFY_ARE_EQUAL(uri::decode(encoded), uri::decode(u1.to_string()));
}

TEST(to_encoded_string)
{
    utility::string_t encoded = uri::encode_uri(U("http://testname.com/%%?qstring"));
    uri u(U("http://testname.com/%25%25?qstring"));

    VERIFY_ARE_EQUAL(encoded, u.to_string());
}

TEST(empty_to_string)
{
    uri u;   
    VERIFY_ARE_EQUAL(U("/"), u.to_string());
}

} // SUITE(conversions_tests)

}}}
