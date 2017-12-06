/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
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

void TestDateTimeRoundtrip(utility::string_t str, utility::string_t strExpected)
{
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    VERIFY_ARE_EQUAL(str2, strExpected);
}

void TestDateTimeRoundtrip(utility::string_t str)
{
    TestDateTimeRoundtrip(str, str);
}

TEST(parsing_time_roundtrip_datetime1)
{
    // Preserve all 7 digits after the comma:
    TestDateTimeRoundtrip(_XPLATSTR("2013-11-19T14:30:59.1234567Z"));
}

TEST(parsing_time_roundtrip_datetime2)
{
    // lose the last '999' without rounding up
    TestDateTimeRoundtrip(_XPLATSTR("2013-11-19T14:30:59.1234567999Z"), _XPLATSTR("2013-11-19T14:30:59.1234567Z"));
}

TEST(parsing_time_roundtrip_datetime3)
{
    // leading 0-s after the comma, tricky to parse correctly
    TestDateTimeRoundtrip(_XPLATSTR("2013-11-19T14:30:59.00123Z"));
}

TEST(parsing_time_roundtrip_datetime4)
{
    // another leading 0 test
    TestDateTimeRoundtrip(_XPLATSTR("2013-11-19T14:30:59.0000001Z"));
}

TEST(parsing_time_roundtrip_datetime5)
{
    // this is going to be truncated
    TestDateTimeRoundtrip(_XPLATSTR("2013-11-19T14:30:59.00000001Z"), _XPLATSTR("2013-11-19T14:30:59Z"));
}

TEST(parsing_time_roundtrip_datetime6)
{
    // Only one digit after the dot
    TestDateTimeRoundtrip(_XPLATSTR("2013-11-19T14:30:59.5Z"));
}

TEST(parsing_time_roundtrip_datetime_invalid1, "Ignore:Linux", "Codeplex issue #115", "Ignore:Apple", "Codeplex issue #115")
{
    // No digits after the dot, or non-digits. This is not a valid input, but we should not choke on it,
    // Simply ignore the bad fraction
    const utility::string_t bad_strings[] = { _XPLATSTR("2013-11-19T14:30:59.Z"),
                                              _XPLATSTR("2013-11-19T14:30:59.1a2Z")
                                            };
    utility::string_t str_corrected = _XPLATSTR("2013-11-19T14:30:59Z");

    for (const auto& str : bad_strings)
    {
        auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
        utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
        VERIFY_ARE_EQUAL(str2, str_corrected);
    }
}

TEST(parsing_time_roundtrip_datetime_invalid2)
{
    // Variouls unsupported cases. In all cases, we have produce an empty date time
    const utility::string_t bad_strings[] = { _XPLATSTR(""),     // empty
                                              _XPLATSTR(".Z"),   // too short
                                              _XPLATSTR(".Zx"),  // no trailing Z
                                              _XPLATSTR("3.14Z") // not a valid date
                                            };

    for (const auto& str : bad_strings)
    {
        auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
        VERIFY_ARE_EQUAL(dt.to_interval(), 0);
    }
}

TEST(parsing_time_roundtrip_time)
{
    // time only without date
    utility::string_t str = _XPLATSTR("14:30:59.1234567Z");
    auto dt = utility::datetime::from_string(str, utility::datetime::ISO_8601);
    utility::string_t str2 = dt.to_string(utility::datetime::ISO_8601);
    // Must look for a substring now, since the date part is filled with today's date
    VERIFY_IS_TRUE(str2.find(str) != std::string::npos);
}

} // SUITE(datetime)

}}}
