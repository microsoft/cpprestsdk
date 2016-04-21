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
* HTTP Library: JSON parser and writer
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <stdio.h>

#ifndef _WIN32
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#endif

using namespace web;
using namespace web::json;
using namespace utility;
using namespace utility::conversions;

//
// JSON Serialization
//

static void append_escape_string(std::string& str, const std::string& escaped)
{
    for (const auto &ch : escaped)
    {
        switch (ch)
        {
            case '\"':
                str += '\\';
                str += '\"';
                break;
            case '\\':
                str += '\\';
                str += '\\';
                break;
            case '\b':
                str += '\\';
                str += 'b';
                break;
            case '\f':
                str += '\\';
                str += 'f';
                break;
            case '\r':
                str += '\\';
                str += 'r';
                break;
            case '\n':
                str += '\\';
                str += 'n';
                break;
            case '\t':
                str += '\\';
                str += 't';
                break;
            default:

                // If a control character then must unicode escaped.
                if (ch >= 0 && ch <= 0x1F)
                {
                    static const std::array<char, 16> intToHex = { { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' } };
                    str += '\\';
                    str += 'u';
                    str += '0';
                    str += '0';
                    str += intToHex[(ch & 0xF0) >> 4];
                    str += intToHex[ch & 0x0F];
                }
                else
                {
                    str += ch;
                }
        }
    }
}

void web::json::details::format_string(const std::string& key, std::string& str)
{
    str.push_back('"');
    append_escape_string(str, key);
    str.push_back('"');
}

void web::json::details::_String::serialize_impl(std::string& str) const
{
    str.push_back('"');

    if(m_has_escape_char)
    {
        append_escape_string(str, m_string);
    }
    else
    {
        str.append(m_string);
    }

    str.push_back('"');
}

void web::json::details::_Number::serialize_impl(std::string& stream) const
{
    if(m_number.m_type != number::type::double_type)
    {
        // #digits + 1 to avoid loss + 1 for the sign + 1 for null terminator.
        const size_t tempSize = std::numeric_limits<uint64_t>::digits10 + 3;
        char tempBuffer[tempSize];

#ifdef _WIN32
        // This can be improved performance-wise if we implement our own routine
        if (m_number.m_type == number::type::signed_type)
            _i64toa_s(m_number.m_intval, tempBuffer, tempSize, 10);
        else
            _ui64toa_s(m_number.m_uintval, tempBuffer, tempSize, 10);

        const auto numChars = strnlen_s(tempBuffer, tempSize);
#else
        int numChars;
        if (m_number.m_type == number::type::signed_type)
            numChars = snprintf(tempBuffer, tempSize, "%" PRId64, m_number.m_intval);
        else
            numChars = snprintf(tempBuffer, tempSize, "%" PRIu64, m_number.m_uintval);
#endif
        stream.append(tempBuffer, numChars);
    }
    else
    {
        // #digits + 2 to avoid loss + 1 for the sign + 1 for decimal point + 5 for exponent (e+xxx) + 1 for null terminator
        const size_t tempSize = std::numeric_limits<double>::digits10 + 10;
        char tempBuffer[tempSize];
#ifdef _WIN32
        const auto numChars = _sprintf_s_l(
            tempBuffer,
            tempSize,
            "%.*g",
            utility::details::scoped_c_thread_locale::c_locale(),
            std::numeric_limits<double>::digits10 + 2,
            m_number.m_value);
#else
        const auto numChars = snprintf(tempBuffer, tempSize, "%.*g", std::numeric_limits<double>::digits10 + 2, m_number.m_value);
#endif
        stream.append(tempBuffer, numChars);
    }
}

const std::string& web::json::value::as_string() const
{
    return m_value->as_string();
}

std::string json::value::serialize() const
{
#ifndef _WIN32
    utility::details::scoped_c_thread_locale locale;
#endif

    std::string ret;
    ret.reserve(m_value->serialize_size());
    m_value->serialize_impl(ret);
    return ret;
}
