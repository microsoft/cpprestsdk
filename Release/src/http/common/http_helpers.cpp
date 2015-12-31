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
* Implementation Details of the http.h layer of messaging
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace utility;
using namespace utility::conversions;

namespace web { namespace http
{
namespace details
{

bool is_content_type_one_of(const utility::string_t *first, const utility::string_t *last, const utility::string_t &value)
{
    while (first != last)
    {
        if (utility::details::str_icmp(*first, value))
        {
            return true;
        }
        ++first;
    }
    return false;
}

// Remove once VS 2013 is no longer supported.
#if defined(_WIN32) && _MSC_VER < 1900
// Not referring to mime_types to avoid static initialization order fiasco.
static const utility::string_t textual_types [] = {
    U("message/http"),
    U("application/json"),
    U("application/xml"),
    U("application/atom+xml"),
    U("application/http"),
    U("application/x-www-form-urlencoded")
};
#endif
bool is_content_type_textual(const utility::string_t &content_type)
{
#if !defined(_WIN32) || _MSC_VER >= 1900
    static const utility::string_t textual_types [] = {
        mime_types::message_http,
        mime_types::application_json,
        mime_types::application_xml,
        mime_types::application_atom_xml,
        mime_types::application_http,
        mime_types::application_x_www_form_urlencoded
    };
#endif

    if (content_type.size() >= 4 && utility::details::str_icmp(content_type.substr(0, 4), _XPLATSTR("text")))
    {
        return true;
    }
    return (is_content_type_one_of(std::begin(textual_types), std::end(textual_types), content_type));
}

// Remove once VS 2013 is no longer supported.
#if defined(_WIN32) && _MSC_VER < 1900
// Not referring to mime_types to avoid static initialization order fiasco.
static const utility::string_t json_types [] = {
    U("application/json"),
    U("application/x-json"),
    U("text/json"),
    U("text/x-json"),
    U("text/javascript"),
    U("text/x-javascript"),
    U("application/javascript"),
    U("application/x-javascript")
};
#endif
bool is_content_type_json(const utility::string_t &content_type)
{
#if !defined(_WIN32) || _MSC_VER >= 1900
    static const utility::string_t json_types [] = {
        mime_types::application_json,
        mime_types::application_xjson,
        mime_types::text_json,
        mime_types::text_xjson,
        mime_types::text_javascript,
        mime_types::text_xjavascript,
        mime_types::application_javascript,
        mime_types::application_xjavascript
    };
#endif

    return (is_content_type_one_of(std::begin(json_types), std::end(json_types), content_type));
}

void parse_content_type_and_charset(const utility::string_t &content_type, utility::string_t &content, utility::string_t &charset)
{
    const size_t semi_colon_index = content_type.find_first_of(_XPLATSTR(";"));

    // No charset specified.
    if (semi_colon_index == utility::string_t::npos)
    {
        content = content_type;
        trim_whitespace(content);
        charset = get_default_charset(content);
        return;
    }

    // Split into content type and second part which could be charset.
    content = content_type.substr(0, semi_colon_index);
    trim_whitespace(content);
    utility::string_t possible_charset = content_type.substr(semi_colon_index + 1);
    trim_whitespace(possible_charset);
    const size_t equals_index = possible_charset.find_first_of(_XPLATSTR("="));

    // No charset specified.
    if (equals_index == utility::string_t::npos)
    {
        charset = get_default_charset(content);
        return;
    }

    // Split and make sure 'charset'
    utility::string_t charset_key = possible_charset.substr(0, equals_index);
    trim_whitespace(charset_key);
    if (!utility::details::str_icmp(charset_key, _XPLATSTR("charset")))
    {
        charset = get_default_charset(content);
        return;
    }
    charset = possible_charset.substr(equals_index + 1);
    // Remove the redundant ';' at the end of charset.
    while (charset.back() == ';')
    {
        charset.pop_back();
    }
    trim_whitespace(charset);
    if (charset.front() == _XPLATSTR('"') && charset.back() == _XPLATSTR('"'))
    {
        charset = charset.substr(1, charset.size() - 2);
        trim_whitespace(charset);
    }
}

utility::string_t get_default_charset(const utility::string_t &content_type)
{
    // We are defaulting everything to Latin1 except JSON which is utf-8.
    if (is_content_type_json(content_type))
    {
        return charset_types::utf8;
    }
    else
    {
        return charset_types::latin1;
    }
}

// Remove once VS 2013 is no longer supported.
#if defined(_WIN32) && _MSC_VER < 1900
static const http_status_to_phrase idToPhraseMap [] = {
#define _PHRASES
#define DAT(a,b,c) {status_codes::a, c},
#include "cpprest/details/http_constants.dat"
#undef _PHRASES
#undef DAT
};
#endif
utility::string_t get_default_reason_phrase(status_code code)
{
#if !defined(_WIN32) || _MSC_VER >= 1900
    // Future improvement: why is this stored as an array of structs instead of a map
    // indexed on the status code for faster lookup?
    // Not a big deal because it is uncommon to not include a reason phrase.
    static const http_status_to_phrase idToPhraseMap [] = {
#define _PHRASES
#define DAT(a,b,c) {status_codes::a, c},
#include "cpprest/details/http_constants.dat"
#undef _PHRASES
#undef DAT
    };
#endif

    utility::string_t phrase;
    for (const auto &elm : idToPhraseMap)
    {
        if (elm.id == code)
        {
            phrase = elm.phrase;
            break;
        }
    }
    return phrase;
}

// Helper function to determine byte order mark.
enum endian_ness
{
    little_endian,
    big_endian,
    unknown
};
static endian_ness check_byte_order_mark(const utf16string &str)
{
    if (str.empty())
    {
        return unknown;
    }
    const unsigned char *src = (const unsigned char *) &str[0];

    // little endian
    if (src[0] == 0xFF && src[1] == 0xFE)
    {
        return little_endian;
    }

    // big endian
    else if (src[0] == 0xFE && src[1] == 0xFF)
    {
        return big_endian;
    }

    return unknown;
}

utility::string_t convert_utf16_to_string_t(utf16string src)
{
#ifdef _UTF16_STRINGS
    return convert_utf16_to_utf16(std::move(src));
#else
    return convert_utf16_to_utf8(std::move(src));
#endif
}

std::string convert_utf16_to_utf8(utf16string src)
{
    const endian_ness endian = check_byte_order_mark(src);
    switch (endian)
    {
    case little_endian:
        return convert_utf16le_to_utf8(std::move(src), true);
    case big_endian:
        return convert_utf16be_to_utf8(std::move(src), true);
    case unknown:
        // unknown defaults to big endian.
        return convert_utf16be_to_utf8(std::move(src), false);
    }
    __assume(0);
}

utf16string convert_utf16_to_utf16(utf16string src)
{
    const endian_ness endian = check_byte_order_mark(src);
    switch (endian)
    {
    case little_endian:
        src.erase(0, 1);
        return std::move(src);
    case big_endian:
        return convert_utf16be_to_utf16le(std::move(src), true);
    case unknown:
        // unknown defaults to big endian.
        return convert_utf16be_to_utf16le(std::move(src), false);
    }
    __assume(0);
}

std::string convert_utf16le_to_utf8(utf16string src, bool erase_bom)
{
    if (erase_bom && !src.empty())
    {
        src.erase(0, 1);
    }
    return utf16_to_utf8(std::move(src));
}

utility::string_t convert_utf16le_to_string_t(utf16string src, bool erase_bom)
{
    if (erase_bom && !src.empty())
    {
        src.erase(0, 1);
    }
#ifdef _UTF16_STRINGS
    return std::move(src);
#else
    return utf16_to_utf8(std::move(src));
#endif
}

// Helper function to change endian ness from big endian to little endian
static utf16string big_endian_to_little_endian(utf16string src, bool erase_bom)
{
    if (erase_bom && !src.empty())
    {
        src.erase(0, 1);
    }
    if (src.empty())
    {
        return std::move(src);
    }

    const size_t size = src.size();
    for (size_t i = 0; i < size; ++i)
    {
        utf16char ch = src[i];
        src[i] = static_cast<utf16char>(ch << 8);
        src[i] = static_cast<utf16char>(src[i] | ch >> 8);
    }

    return std::move(src);
}

utility::string_t convert_utf16be_to_string_t(utf16string src, bool erase_bom)
{
#ifdef _UTF16_STRINGS
    return convert_utf16be_to_utf16le(std::move(src), erase_bom);
#else
    return convert_utf16be_to_utf8(std::move(src), erase_bom);
#endif
}

std::string convert_utf16be_to_utf8(utf16string src, bool erase_bom)
{
    return utf16_to_utf8(big_endian_to_little_endian(std::move(src), erase_bom));
}

utf16string convert_utf16be_to_utf16le(utf16string src, bool erase_bom)
{
    return big_endian_to_little_endian(std::move(src), erase_bom);
}

void ltrim_whitespace(utility::string_t &str)
{
    size_t index;
    for (index = 0; index < str.size() && isspace(str[index]); ++index);
    str.erase(0, index);
}
void rtrim_whitespace(utility::string_t &str)
{
    size_t index;
    for (index = str.size(); index > 0 && isspace(str[index - 1]); --index);
    str.erase(index);
}
void trim_whitespace(utility::string_t &str)
{
    ltrim_whitespace(str);
    rtrim_whitespace(str);
}

size_t chunked_encoding::add_chunked_delimiters(_Out_writes_(buffer_size) uint8_t *data, _In_ size_t buffer_size, size_t bytes_read)
{
    size_t offset = 0;

    if (buffer_size < bytes_read + http::details::chunked_encoding::additional_encoding_space)
    {
        throw http_exception(_XPLATSTR("Insufficient buffer size."));
    }

    if (bytes_read == 0)
    {
        offset = 7;
        data[7] = '0';
        data[8] = '\r';  data[9] = '\n'; // The end of the size.
        data[10] = '\r'; data[11] = '\n'; // The end of the message.
    }
    else
    {
        char buffer[9];
#ifdef _WIN32
        sprintf_s(buffer, sizeof(buffer), "%8IX", bytes_read);
#else
        snprintf(buffer, sizeof(buffer), "%8zX", bytes_read);
#endif
        memcpy(&data[0], buffer, 8);
        while (data[offset] == ' ') ++offset;
        data[8] = '\r'; data[9] = '\n'; // The end of the size.
        data[10 + bytes_read] = '\r'; data[11 + bytes_read] = '\n'; // The end of the chunk.
    }

    return offset;
}

#if (!defined(_WIN32) || defined(__cplusplus_winrt))
const std::array<bool,128> valid_chars =
{{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //16-31
    0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, //32-47
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, //48-63
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //64-79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, //80-95
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //96-111
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0 //112-127
    }};

// Checks if the method contains any invalid characters
bool validate_method(const utility::string_t& method)
{
    for (const auto &ch : method)
    {
        size_t ch_sz = static_cast<size_t>(ch);
        if (ch_sz >= 128)
            return false;

        if (!valid_chars[ch_sz])
            return false;
    }

    return true;
}
#endif

} // namespace details
}} // namespace web::http
