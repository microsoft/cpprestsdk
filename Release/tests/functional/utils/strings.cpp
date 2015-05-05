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

#if !defined(__GLIBCXX__)
#include <codecvt>
#endif

#include <locale_guard.h>

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

TEST(utf8_to_utf16)
{
#if !defined(__GLIBCXX__)
    std::wstring_convert<std::codecvt_utf8_utf16<utf16char>, utf16char> conversion;
#endif

    // single byte character
    VERIFY_ARE_EQUAL(_XPLATSTR("ABC123"), utility::conversions::utf8_to_utf16("ABC123"));

    // 2 byte character
    std::string input;
    input.push_back(unsigned char(207)); // 11001111
    input.push_back(unsigned char(129)); // 10000001
    input.push_back(unsigned char(198)); // 11000110
    input.push_back(unsigned char(141)); // 10001101
    auto result = utility::conversions::utf8_to_utf16(input);
#if defined(__GLIBCXX__)
    VERIFY_ARE_EQUAL(961, result[0]);
    VERIFY_ARE_EQUAL(397, result[1]);
#else
    VERIFY_ARE_EQUAL(conversion.from_bytes(input), result);
#endif

    // 3 byte character
    input.clear();
    input.push_back(unsigned char(230)); // 11100110
    input.push_back(unsigned char(141)); // 10001101
    input.push_back(unsigned char(157)); // 10011101
    input.push_back(unsigned char(231)); // 11100111
    input.push_back(unsigned char(143)); // 10001111
    input.push_back(unsigned char(156)); // 10011100
    result = utility::conversions::utf8_to_utf16(input);
#if defined(__GLIBCXX__)
    VERIFY_ARE_EQUAL(25437, result[0]);
    VERIFY_ARE_EQUAL(29660, result[1]);
#else
    VERIFY_ARE_EQUAL(conversion.from_bytes(input), result);
#endif

    // 4 byte character
    input.clear();
    input.push_back(unsigned char(240)); // 11110000
    input.push_back(unsigned char(173)); // 10101101
    input.push_back(unsigned char(157)); // 10011101
    input.push_back(unsigned char(143)); // 10001111
    input.push_back(unsigned char(240)); // 11111000
    input.push_back(unsigned char(161)); // 10100001
    input.push_back(unsigned char(191)); // 10111111
    input.push_back(unsigned char(191)); // 10111111
    result = utility::conversions::utf8_to_utf16(input);
#if defined(__GLIBCXX__)
    VERIFY_ARE_EQUAL(55413, result[0]);
    VERIFY_ARE_EQUAL(57167, result[1]);
    VERIFY_ARE_EQUAL(55296, result[2]);
    VERIFY_ARE_EQUAL(57160, result[3]);
#else
    VERIFY_ARE_EQUAL(conversion.from_bytes(input), result);
#endif
}

TEST(utf8_to_utf16_errors)
{
    // missing second continuation byte
    std::string input;
    input.push_back(unsigned char(207)); // 11001111
    VERIFY_THROWS(utility::conversions::utf8_to_utf16(input), std::invalid_argument);

    // missing third continuation byte
    input.clear();
    input.push_back(unsigned char(230)); // 11100110
    input.push_back(unsigned char(141)); // 10001101
    VERIFY_THROWS(utility::conversions::utf8_to_utf16(input), std::invalid_argument);

    // missing fourth continuation byte
    input.clear();
    input.push_back(unsigned char(240)); // 11110000
    input.push_back(unsigned char(173)); // 10101101
    input.push_back(unsigned char(157)); // 10011101
    VERIFY_THROWS(utility::conversions::utf8_to_utf16(input), std::invalid_argument);
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

TEST(print_string_locale, "Ignore:Android", "Locale unsupported on Android")
{
    std::locale changedLocale;
    try
    {
#ifdef _WIN32
        changedLocale = std::locale("fr-FR");
#else
        changedLocale = std::locale("fr_FR.UTF-8");
#endif
    }
    catch (const std::exception &)
    {
        // Silently pass if locale isn't installed on machine.
        return;
    }

    tests::common::utilities::locale_guard loc(changedLocale);

    utility::ostringstream_t oss;
    oss << 1000;
    VERIFY_ARE_EQUAL(oss.str(), utility::conversions::print_string(1000));
    VERIFY_ARE_EQUAL(_XPLATSTR("1000"), utility::conversions::print_string(1000, std::locale::classic()));
}

TEST(scan_string_locale, "Ignore:Android", "Locale unsupported on Android")
{
    std::locale changedLocale;
    try
    {
#ifdef _WIN32
        changedLocale = std::locale("fr-FR");
#else
        changedLocale = std::locale("fr_FR.UTF-8");
#endif
    }
    catch (const std::exception &)
    {
        // Silently pass if locale isn't installed on machine.
        return;
    }

    VERIFY_ARE_EQUAL(_XPLATSTR("1000"), utility::conversions::scan_string<utility::string_t>(utility::string_t(_XPLATSTR("1000"))));
    VERIFY_ARE_EQUAL(_XPLATSTR("1,000"), utility::conversions::scan_string<utility::string_t>(utility::string_t(_XPLATSTR("1,000"))));

    VERIFY_ARE_EQUAL(_XPLATSTR("1000"), utility::conversions::scan_string<utility::string_t>(utility::string_t(_XPLATSTR("1000")), changedLocale));
    VERIFY_ARE_EQUAL(_XPLATSTR("1,000"), utility::conversions::scan_string<utility::string_t>(utility::string_t(_XPLATSTR("1,000")), changedLocale));

    {
        tests::common::utilities::locale_guard loc(changedLocale);
        VERIFY_ARE_EQUAL(_XPLATSTR("1000"), utility::conversions::scan_string<utility::string_t>(utility::string_t(_XPLATSTR("1000")), std::locale::classic()));
        VERIFY_ARE_EQUAL(_XPLATSTR("1,000"), utility::conversions::scan_string<utility::string_t>(utility::string_t(_XPLATSTR("1,000")), std::locale::classic()));
    }
}

}
    
}}} //namespaces
