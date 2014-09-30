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
* nonce_generator_tests.cpp
*
* Tests for nonce_generator class.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace utility;

namespace tests { namespace functional { namespace utils_tests {

SUITE(nonce_generator_tests)
{

TEST(nonce_generator_set_length)
{
    utility::nonce_generator gen;
    VERIFY_ARE_EQUAL(utility::nonce_generator::default_length, gen.generate().length());

    gen.set_length(1);
    VERIFY_ARE_EQUAL(1, gen.generate().length());

    gen.set_length(0);
    VERIFY_ARE_EQUAL(0, gen.generate().length());

    gen.set_length(500);
    VERIFY_ARE_EQUAL(500, gen.generate().length());
}

TEST(nonce_generator_unique_strings)
{
    // Generate 100 nonces and check each is unique.
    std::vector<utility::string_t> nonces(100);
    utility::nonce_generator gen;
    for (auto&& v : nonces)
    {
        v = gen.generate();
    }
    for (auto v : nonces)
    {
        VERIFY_ARE_EQUAL(1, std::count(nonces.begin(), nonces.end(), v));
    }
}

} // SUITE(nonce_generator_tests)

}}}
