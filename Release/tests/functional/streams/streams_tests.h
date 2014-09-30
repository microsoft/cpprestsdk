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
* streams_tests.h
*
* Common routines for streams tests.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <system_error>

namespace tests { namespace functional { namespace streams {

template<typename CharType>
void test_stream_length(concurrency::streams::basic_istream<CharType> istr, size_t length)
{
    using namespace concurrency::streams;
    
    auto curr = istr.tell();
    auto t1 = (curr != static_cast<typename basic_istream<CharType>::pos_type>(basic_istream<CharType>::traits::eof()));
    VERIFY_IS_TRUE(t1);

    auto end = istr.seek(0, std::ios_base::end);
    VERIFY_IS_TRUE(end != static_cast<typename basic_istream<CharType>::pos_type>(basic_istream<CharType>::traits::eof()));

    auto len = end - curr;

    VERIFY_ARE_EQUAL(len, length);

    {
        auto curr2 = istr.tell();
        VERIFY_IS_TRUE(curr != static_cast<typename basic_istream<CharType>::pos_type>(basic_istream<CharType>::traits::eof()));

        auto end2 = istr.seek(0, std::ios_base::end);
        VERIFY_IS_TRUE(end != static_cast<typename basic_istream<CharType>::pos_type>(basic_istream<CharType>::traits::eof()));

        auto len2 = end2 - curr2;

        VERIFY_ARE_EQUAL(len2, 0);
    }

    auto newpos = istr.seek(curr);
    VERIFY_IS_TRUE(newpos != static_cast<typename basic_istream<CharType>::pos_type>(basic_istream<CharType>::traits::eof()));

    VERIFY_ARE_EQUAL(curr, newpos);
}

// Helper function to verify std::system_error is thrown with correct error code
#define VERIFY_THROWS_SYSTEM_ERROR(__expression, __code)                                \
    UNITTEST_MULTILINE_MACRO_BEGIN                                                      \
        try                                                                             \
        {                                                                               \
            __expression;                                                               \
            VERIFY_IS_TRUE(false, "Expected std::system_error not thrown");             \
        }                                                                               \
        catch (const std::system_error &_exc)                                           \
        {                                                                               \
            VERIFY_IS_TRUE(std::string(_exc.what()).size() > 0);                        \
            /* The reason we can't directly compare with the given std::errc code is because*/      \
            /* on Windows the STL implementation of error categories are NOT unique across*/        \
            /* dll boundaries.*/                                                        \
            const std::error_condition _condFound = _exc.code().default_error_condition();          \
            VERIFY_ARE_EQUAL(static_cast<int>(__code), _condFound.value());             \
        }                                                                               \
        catch(...)                                                                      \
        {                                                                               \
            VERIFY_IS_TRUE(false, "Exception other than std::system_error thrown");     \
        }                                                                               \
    UNITTEST_MULTILINE_MACRO_END

}}}
