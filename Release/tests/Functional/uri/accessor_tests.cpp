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
* constructor_string_tests.cpp
*
* Tests for constructors of the uri class
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;

namespace tests { namespace functional { namespace uri_tests {

SUITE(accessor_tests)
{

TEST(authority_string)
{
    http::uri u(U("http://testname.com:81/path?baz"));
    http::uri a = u.authority();

    VERIFY_ARE_EQUAL(U("/path"), u.path());
    VERIFY_ARE_EQUAL(U("http"), a.scheme());
    VERIFY_ARE_EQUAL(U("testname.com"), a.host());
    VERIFY_ARE_EQUAL(81, a.port());
    VERIFY_ARE_EQUAL(http::uri(U("http://testname.com:81")), a);
}

TEST(authority_wstring)
{
    http::uri u(U("http://testname.com:81/path?baz"));
    http::uri a = u.authority();

    VERIFY_ARE_EQUAL(U("/path"), u.path());
    VERIFY_ARE_EQUAL(U("http"), a.scheme());
    VERIFY_ARE_EQUAL(U("testname.com"), a.host());
    VERIFY_ARE_EQUAL(81, a.port());
    VERIFY_ARE_EQUAL(http::uri(U("http://testname.com:81")), a);
}

} // SUITE(accessor_tests)

}}}
