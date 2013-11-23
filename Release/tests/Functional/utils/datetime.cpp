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

TEST(parsing_time_roundtrip_datetime01)
{
    // Preserve all 7 digits after the comma:
    utility::string_t str = _XPLATSTR("2013-11-19T14:30:59.1234567Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    VERIFY_ARE_EQUAL(str2, str);
}

TEST(parsing_time_roundtrip_datetime02)
{
    // lose the last '999' without rounding up
    utility::string_t str = _XPLATSTR("2013-11-19T14:30:59.1234567999Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    VERIFY_ARE_EQUAL(str2, _XPLATSTR("2013-11-19T14:30:59.1234567Z"));
}

TEST(parsing_time_roundtrip_datetime03)
{
    // leading 0-s after the comma, tricky to parse correctly
    utility::string_t str = _XPLATSTR("2013-11-19T14:30:59.00123Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    VERIFY_ARE_EQUAL(str2, str);
}

TEST(parsing_time_roundtrip_datetime4)
{
    // another leading 0 test
    utility::string_t str = _XPLATSTR("2013-11-19T14:30:59.0000001Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    VERIFY_ARE_EQUAL(str2, str);
}

TEST(parsing_time_roundtrip_datetime05)
{
    // this is going to be truncated
    utility::string_t str = _XPLATSTR("2013-11-19T14:30:59.00000001Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    VERIFY_ARE_EQUAL(str2, _XPLATSTR("2013-11-19T14:30:59Z"));
}

TEST(parsing_time_roundtrip_time)
{
    // time only without date
    utility::string_t str = _XPLATSTR("14:30:59.1234567Z");
    //utility::string_t str = _XPLATSTR("14:30:01Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    // Must look for a substring now, since the date part is filled with today's date
    VERIFY_IS_TRUE(str2.find(str) != std::string::npos);
}

} // SUITE(datetime)

}}}
