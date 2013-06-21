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
* datetime.cpp
*
* Tests for datetime-related utility functions and classes.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace utility;

namespace tests { namespace functional { namespace utils_tests {

SUITE(datetime)
{

// This is by no means a comprehensive test suite for the datetime functionality.
// It's a response to a particular bug and should be amended over time.

TEST(parsing_dateandtime_basic)
{
    // ISO 8601
    // RFC 1123

    auto dt1 = utility::datetime::from_string(_XPLATSTR("20130517T00:00:00Z"), utility::datetime::ISO_8601);
    VERIFY_ARE_NOT_EQUAL(0u, dt1.to_interval());

    auto dt2 = utility::datetime::from_string(_XPLATSTR("Fri, 17 May 2013 00:00:00 GMT"), utility::datetime::RFC_1123);
    VERIFY_ARE_NOT_EQUAL(0u, dt2.to_interval());

    VERIFY_ARE_EQUAL(dt1.to_interval(), dt2.to_interval());
}

TEST(parsing_dateandtime_extended)
{
    // ISO 8601
    // RFC 1123

    auto dt1 = utility::datetime::from_string(_XPLATSTR("2013-05-17T00:00:00Z"), utility::datetime::ISO_8601);
    VERIFY_ARE_NOT_EQUAL(0u, dt1.to_interval());

    auto dt2 = utility::datetime::from_string(_XPLATSTR("Fri, 17 May 2013 00:00:00 GMT"), utility::datetime::RFC_1123);
    VERIFY_ARE_NOT_EQUAL(0u, dt2.to_interval());

    VERIFY_ARE_EQUAL(dt1.to_interval(), dt2.to_interval());
}

TEST(parsing_date_basic)
{
    // ISO 8601
    {
        auto dt = utility::datetime::from_string(_XPLATSTR("20130517"), utility::datetime::ISO_8601);

        VERIFY_ARE_NOT_EQUAL(0u, dt.to_interval());
    }
}

TEST(parsing_date_extended)
{
    // ISO 8601
    {
        auto dt = utility::datetime::from_string(_XPLATSTR("2013-05-17"), utility::datetime::ISO_8601);

        VERIFY_ARE_NOT_EQUAL(0u, dt.to_interval());
    }
}

TEST(parsing_time_extended)
{
    // ISO 8601
    {
        auto dt = utility::datetime::from_string(_XPLATSTR("14:30:01Z"), utility::datetime::ISO_8601);

        VERIFY_ARE_NOT_EQUAL(0u, dt.to_interval());
    }
}

} // SUITE(datetime)

}}}
