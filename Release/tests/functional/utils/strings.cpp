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
 * base64.cpp
 *
 * Tests for base64-related utility functions and classes.
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#include "stdafx.h"

using namespace utility;

namespace tests { namespace functional { namespace utils_tests {
    
SUITE(strings)
{

TEST(usascii_to_utf16)
{
    std::string str_ascii("This is a test");
    utf16string str_utf16 = utility::conversions::usascii_to_utf16(str_ascii);
    
    for (size_t i = 0; i < str_ascii.size(); ++i)
    {
        VERIFY_ARE_EQUAL((utf16char)str_ascii[i], str_utf16[i]);
    }
}
    
TEST(default_to_utf16)
{
    // TODO: find some string that actually uses something unique to the default code page.
    std::string str_default("This is a test");
    utf16string str_utf16 = utility::conversions::usascii_to_utf16(str_default);
    
    for (size_t i = 0; i < str_default.size(); ++i)
    {
        VERIFY_ARE_EQUAL((utf16char)str_default[i], str_utf16[i]);
    }
}

TEST(latin1_to_utf16)
{
    // TODO: find some string that actually uses something unique to the Latin1 code page.
    std::string str_latin1("This is a test");
    utf16string str_utf16 = utility::conversions::usascii_to_utf16(str_latin1);
    
    for (size_t i = 0; i < str_latin1.size(); ++i)
    {
        VERIFY_ARE_EQUAL((utf16char)str_latin1[i], str_utf16[i]);
    }
}
}
    
}}} //namespaces
