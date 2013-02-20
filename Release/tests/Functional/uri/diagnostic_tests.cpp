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
* diagnostic_tests.cpp
*
* Tests for diagnostic functions like is_host_loopback of the http::uri class.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;
using namespace http;

namespace tests { namespace functional { namespace uri_tests {

SUITE(diagnostic_tests)
{

TEST(empty_components)
{
    VERIFY_IS_TRUE(uri().is_empty());

    VERIFY_IS_FALSE(uri().is_authority());

    VERIFY_IS_FALSE(uri().is_host_loopback());
    VERIFY_IS_FALSE(uri().is_host_wildcard());
    VERIFY_IS_FALSE(uri().is_host_portable());

    VERIFY_IS_FALSE(uri().is_port_default());
}

TEST(is_authority)
{
    VERIFY_IS_TRUE(uri(U("http://first.second/")).is_authority());
    VERIFY_IS_TRUE(uri(U("http://first.second")).is_authority());

    VERIFY_IS_FALSE(uri(U("http://first.second/b")).is_authority());
    VERIFY_IS_FALSE(uri(U("http://first.second?qstring")).is_authority());
    VERIFY_IS_FALSE(uri(U("http://first.second#third")).is_authority());
}

TEST(has_same_authority)
{
    VERIFY_IS_TRUE(uri(U("http://first.second/")).has_same_authority(uri(U("http://first.second/path"))));
    VERIFY_IS_TRUE(uri(U("http://first.second:83/")).has_same_authority(uri(U("http://first.second:83/path:83"))));

    VERIFY_IS_FALSE(uri(U("http://first.second:82/")).has_same_authority(uri(U("http://first.second/path"))));
    VERIFY_IS_FALSE(uri(U("tcp://first.second:82/")).has_same_authority(uri(U("http://first.second/path"))));
    VERIFY_IS_FALSE(uri(U("http://path.:82/")).has_same_authority(uri(U("http://first.second/path"))));
}

TEST(has_same_authority_empty)
{
    VERIFY_IS_FALSE(uri().has_same_authority(uri()));
    VERIFY_IS_FALSE(uri(U("http://first.second/")).has_same_authority(uri()));
    VERIFY_IS_FALSE(uri().has_same_authority(uri(U("http://first.second/"))));
}

TEST(is_host_wildcard)
{
    VERIFY_IS_TRUE(uri(U("http://*/")).is_host_wildcard());
    VERIFY_IS_TRUE(uri(U("http://+/?qstring")).is_host_wildcard());

    VERIFY_IS_FALSE(uri(U("http://bleh/?qstring")).is_host_wildcard());
    VERIFY_IS_FALSE(uri(U("http://+*/?qstring")).is_host_wildcard());
}

TEST(is_host_loopback)
{
    VERIFY_IS_TRUE(uri(U("http://localhost/")).is_host_loopback());
    VERIFY_IS_TRUE(uri(U("http://LoCALHoST/")).is_host_loopback());

    VERIFY_IS_FALSE(uri(U("http://127")).is_host_loopback());
    VERIFY_IS_FALSE(uri(U("http://bleh/?qstring")).is_host_loopback());
    VERIFY_IS_FALSE(uri(U("http://+*/?qstring")).is_host_loopback());
    VERIFY_IS_TRUE(uri(U("http://127.0.0.1/")).is_host_loopback());
    VERIFY_IS_TRUE(uri(U("http://127.155.0.1/")).is_host_loopback());
    VERIFY_IS_FALSE(uri(U("http://128.0.0.1/")).is_host_loopback());
}

TEST(is_host_portable)
{
    VERIFY_IS_TRUE(uri(U("http://bleh/?qstring")).is_host_portable());

    VERIFY_IS_FALSE(uri(U("http://localhost/")).is_host_portable());
    VERIFY_IS_FALSE(uri(U("http://+/?qstring")).is_host_portable());
}

TEST(is_port_default)
{
    VERIFY_IS_TRUE(uri(U("http://bleh/?qstring")).is_port_default());
    VERIFY_IS_TRUE(uri(U("http://localhost:0/")).is_port_default());

    VERIFY_IS_FALSE(uri(U("http://+:85/?qstring")).is_port_default());
}

TEST(is_path_empty)
{
    VERIFY_IS_TRUE(uri(U("http://bleh/?qstring")).is_path_empty());
    VERIFY_IS_TRUE(uri(U("http://localhost:0")).is_path_empty());

    VERIFY_IS_FALSE(uri(U("http://+:85/path/?qstring")).is_path_empty());
}

} // SUITE(diagnostic_tests)

}}}
