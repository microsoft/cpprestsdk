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
* http_helpers.cpp - Implementation Details of the http.h layer of messaging
*
* Functions and types for interoperating with http.h from modern C++
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/http_helpers.h"

using namespace web; using namespace utility;
using namespace utility::conversions;

namespace web { namespace http
{
namespace details
{
    bool is_content_type_one_of(const utility::string_t *first, const utility::string_t *last, const utility::string_t &value)
    {
        while (first != last) {
            if(
#ifdef _MS_WINDOWS
                _wcsicmp(first->c_str(), value.c_str()) == 0
#else
                boost::iequals(*first, value.c_str())
#endif
                )
                return true;
            ++first;
        }
        return false;
    }

    bool is_content_type_textual(const utility::string_t &content_type)
    {
        static const utility::string_t textual_types[] = {
            mime_types::message_http,
            mime_types::application_json,
            mime_types::application_xml,
            mime_types::application_atom_xml,
            mime_types::application_http,
            mime_types::application_x_www_form_urlencoded
        };

        if((content_type.size() >= 4 &&
#ifdef _MS_WINDOWS
            _wcsicmp(content_type.substr(0,4).c_str(), _XPLATSTR("text")) == 0)
#else
            boost::iequals(content_type.substr(0,4), _XPLATSTR("text")))
#endif
            )
            return true;

        return (is_content_type_one_of(std::begin(textual_types), std::end(textual_types), content_type));
    }

    bool is_content_type_json(const utility::string_t &content_type)
    {
        static const utility::string_t json_types[] = {
            mime_types::application_json,
            mime_types::text_json,
            mime_types::text_xjson,
            mime_types::text_javascript,
            mime_types::text_xjavascript,
            mime_types::application_javascript,
            mime_types::application_xjavascript
        };

        return (is_content_type_one_of(std::begin(json_types), std::end(json_types), content_type));
    }

    void parse_content_type_and_charset(const utility::string_t &content_type, utility::string_t &content, utility::string_t &charset)
    {
        const size_t semi_colon_index = content_type.find_first_of(_XPLATSTR(";"));
        
        // No charset specified.
        if(semi_colon_index == utility::string_t::npos)
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
        if(equals_index == utility::string_t::npos)
        {
            charset = get_default_charset(content);
            return;
        }

        // Split and make sure 'charset'
        utility::string_t charset_key = possible_charset.substr(0, equals_index);
        trim_whitespace(charset_key);
#ifdef _MS_WINDOWS
        if(_wcsicmp(charset_key.c_str(), _XPLATSTR("charset")) != 0)
#else
        if(!boost::iequals(charset_key, _XPLATSTR("charset")))
#endif
        {
            charset = get_default_charset(content);
            return;
        }
        charset = possible_charset.substr(equals_index + 1);
        trim_whitespace(charset);
    }

    utility::string_t get_default_charset(const utility::string_t &content_type)
    {
        // We are defaulting everything to Latin1 except JSON which is utf-8.
        if(is_content_type_json(content_type))
        {
            return charset_types::utf8;
        }
        else
        {
            return charset_types::latin1;
        }
    }

    std::string convert_latin1_to_utf8(const unsigned char *src, size_t src_size)
    {
        return utf16_to_utf8(latin1_to_utf16(convert_bytes_to_string(src, src_size)));
    }

    std::string convert_latin1_to_utf8(const std::vector<unsigned char> &src)
    {
        return utf16_to_utf8(latin1_to_utf16(convert_bytes_to_string(src)));
    }

    utf16string convert_latin1_to_utf16(const unsigned char *src, size_t src_size)
    {
        return latin1_to_utf16(convert_bytes_to_string(src, src_size));
    }

    utf16string convert_latin1_to_utf16(const std::vector<unsigned char> &src)
    {
        return latin1_to_utf16(convert_bytes_to_string(src));
    }

    // Helper function to determine byte order mark.
    enum endian_ness
    {
        little_endian,
        big_endian,
        unknown
    };
    static endian_ness check_type_order_mark(const unsigned char *src, size_t size)
    {
        if(size < 2)
        {
            return unknown;
        }

        // little endian
        if(src[0] == 0xFF && src[1] == 0xFE)
        {
            return little_endian;
        }

        // big endian
        else if(src[0] == 0xFE && src[1] == 0xFF)
        {
            return big_endian;
        }

        return unknown;
    }
    static endian_ness check_byte_order_mark(const std::vector<unsigned char> &src)
    {
        if(src.empty())
        {
            return unknown;
        }
        return check_type_order_mark(&src[0], src.size());
    }
    static endian_ness check_byte_order_mark(const utf16string &src)
    {
        if(src.empty())
        {
            return unknown;
        }
        return check_type_order_mark((const unsigned char *)&src[0], src.size() * 2);
    }

    std::string convert_utf16_to_utf8(const utf16string &src)
    {
        const endian_ness endian = check_byte_order_mark(src);
        switch(endian)
        {
        case little_endian:
            return convert_utf16le_to_utf8(src, true);
        case big_endian:
            return convert_utf16be_to_utf8(src, true);
        case unknown:
            // unknown defaults to big endian.
            return convert_utf16be_to_utf8(src, false);
        }
        
        // Compiler complains about not returning in all cases.
        throw std::runtime_error("Better never hit here.");
    }

    utf16string convert_utf8_to_utf16(const unsigned char *src, size_t src_size)
    {
        return utf8_to_utf16(convert_bytes_to_string(src, src_size));
    }

    std::string convert_utf16_to_utf8(const std::vector<unsigned char> &src)
    {
        const endian_ness endian = check_byte_order_mark(src);
        switch(endian)
        {
        case little_endian:
            return convert_utf16le_to_utf8(convert_bytes_to_wstring(src), true);
        case big_endian:
            return convert_utf16be_to_utf8(convert_bytes_to_wstring(src), true);
        case unknown:
            // unknown defaults to big endian.
            return convert_utf16be_to_utf8(convert_bytes_to_wstring(src), false);
        }

        // Compiler complains about not returning in all cases.
        throw std::runtime_error("Better never hit here.");
    }

    utf16string convert_utf16_to_utf16(const unsigned char *src, size_t src_size)
    {
        return convert_utf16_to_utf16(convert_bytes_to_wstring(src, src_size));
    }

    utf16string convert_utf16_to_utf16(utf16string &&src)
    {
        const endian_ness endian = check_byte_order_mark(src);
        utf16string temp = src;
        switch(endian)
        {
        case little_endian:
            temp = src;
            temp.erase(0, 1);
            return temp;
        case big_endian:
            return convert_utf16be_to_utf16le(src, true);
        case unknown:
            // unknown defaults to big endian.
            return convert_utf16be_to_utf16le(src, false);
        }

        // Compiler complains about not returning in all cases.
        throw std::runtime_error("Better never hit here.");
    }

    utf16string convert_utf16_to_utf16(const std::vector<unsigned char> &src)
    {
        if(src.empty())
        {
            return utf16string();
        }

        // Just need to check for matching byte order and remove mark if present.
        const endian_ness endian = check_byte_order_mark(src);
        switch(endian)
        {
        case little_endian:
            return convert_bytes_to_wstring(&src[2], src.size() - 2);
        case big_endian:
            return convert_utf16be_to_utf16le(src, true);
        case unknown:
            // unknown defaults to big endian.
            return convert_utf16be_to_utf16le(src, false);
        }

        // Compiler complains about not returning in all cases.
        throw std::runtime_error("Better never hit here.");
    }

    utf16string convert_utf8_to_utf16(const std::vector<unsigned char> &src)
    {
        return utf8_to_utf16(convert_bytes_to_string(src));
    }

    std::string convert_utf16le_to_utf8(const utf16string &src, bool erase_bom)
    {
        // Same endian ness as Windows.
        utf16string temp(src);
        if(erase_bom && !src.empty())
        {
            temp.erase(0, 1);
        }
        return utf16_to_utf8(temp);
    }

    std::string convert_utf16le_to_utf8(const std::vector<unsigned char> &src, bool erase_bom)
    {
        // Same endian ness as Windows.
        utf16string wresult = convert_bytes_to_wstring(src);
        if(erase_bom && !wresult.empty())
        {
            wresult.erase(0, 1);
        }
        return utf16_to_utf8(wresult);
    }

    // Helper function to change endian ness from big endian to little endian
    static utf16string big_endian_to_little_endian(const utf16string &src, bool erase_bom)
    {
        utf16string dest;

        // erase byte order mark.
        size_t bom_index = 0;
#ifdef _MS_WINDOWS
        msl::utilities::SafeInt<size_t> sz(src.size());
#else
        size_t sz(src.size());
#endif
        if(erase_bom && !src.empty())
        {
            dest.resize(sz - 1);
            bom_index = 1;
        }
        else
        {
            dest.resize(src.size());
        }

        // Make sure even number of bytes.
        if(dest.size() % 2 != 0)
        {
            throw http_exception(_XPLATSTR("Error when changing endian ness there were an uneven number of bytes."));
        }

        // Need to convert to little endian for Windows.
        const size_t dest_size = dest.size();
        for(size_t i = 0; i < dest_size; ++i)
        {
            dest[i] = static_cast<utf16char>(src[i + bom_index] << 8);
            dest[i] = static_cast<utf16char>(dest[i] | src[i + bom_index] >> 8);
        }
        return dest;
    }
    static std::vector<unsigned char> big_endian_to_little_endian(const std::vector<unsigned char> &src, bool erase_bom)
    {
        std::vector<unsigned char> dest;

        // erase byte order mark.
        size_t bom_index = 0;
#ifdef _MS_WINDOWS
        msl::utilities::SafeInt<size_t> sz(src.size());
#else
        size_t sz(src.size());
#endif
        if(erase_bom && sz >= 2)
        {
            dest.resize(sz - 2);
            bom_index = 2;
        }
        else
        {
            dest.resize(sz);
        }

        // Make sure even number of bytes.
#ifdef _MS_WINDOWS
        msl::utilities::SafeInt<size_t> dsz(dest.size());
#else
        size_t dsz(dest.size());
#endif
        if(dsz % 2 != 0)
        {
            throw http_exception(_XPLATSTR("Error when changing endian ness there were an uneven number of bytes."));
        }

        // Need to convert to little endian for Windows.
        const size_t dest_size = dsz;
        for(size_t i = 0; i < dest_size; i += 2)
        {
            dest[i] = src[i + 1 + bom_index];
            dest[i + 1] = src[i + bom_index];
        }

        return dest;
    }

    std::string convert_utf16be_to_utf8(const utf16string &src, bool erase_bom)
    {
        return convert_utf16le_to_utf8(big_endian_to_little_endian(src, erase_bom), false);
    }

    std::string convert_utf16be_to_utf8(const std::vector<unsigned char> &src, bool erase_bom)
    {
        return convert_utf16le_to_utf8(big_endian_to_little_endian(src, erase_bom), false);
    }

    utf16string convert_utf16be_to_utf16le(const unsigned char *src, size_t src_size, bool erase_bom)
    {
        return convert_utf16be_to_utf16le(convert_bytes_to_wstring(src, src_size), erase_bom);
    }

    utf16string convert_utf16be_to_utf16le(const utf16string &src, bool erase_bom)
    {
        return big_endian_to_little_endian(src, erase_bom);
    }

    utf16string convert_utf16be_to_utf16le(const std::vector<unsigned char> &src, bool erase_bom)
    {
        return convert_bytes_to_wstring(big_endian_to_little_endian(src, erase_bom));
    }

    std::string convert_bytes_to_string(const unsigned char *src, size_t src_size)
    {
        std::string result;
        if(src_size != 0)
        {
            result.resize(src_size);
            memcpy(&result[0], src, src_size);
        }
        return result;
    }
    
    utf16string convert_bytes_to_wstring(const unsigned char *src, size_t src_size)
    {
        // If there was some sort of way we could do a move or create a string
        // from a sequence of bytes we would save a lot of copies, but there isn't.
        utf16string result;
#ifdef _MS_WINDOWS
        msl::utilities::SafeInt<size_t> sz(src_size);
#else
        size_t sz(src_size);
#endif
        if(sz != 0)
        {
            result.resize(sz / 2);
            memcpy(&result[0], src, sz);
        }
        return result;
    }

    std::string convert_bytes_to_string(const std::vector<unsigned char> &src)
    {
        if(src.empty())
        {
            return std::string();
        }
        else
        {
            return convert_bytes_to_string(&src[0], src.size());
        }
    }

    utf16string convert_bytes_to_wstring(const std::vector<unsigned char> &src)
    {
        if(src.empty())
        {
            return utf16string();
        }
        else
        {
            return convert_bytes_to_wstring(&src[0], src.size());
        }
    }

    void ltrim_whitespace(utility::string_t &str)
    {
        size_t index;
        for(index = 0; index < str.size() && isspace(str[index]); ++index);
        str.erase(0, index);
    }
    void rtrim_whitespace(utility::string_t &str)
    {
        size_t index;
        for(index = str.size(); index > 0 && isspace(str[index - 1]); --index);
        str.erase(index);
    }
    void trim_whitespace(utility::string_t &str)
    {
        ltrim_whitespace(str);
        rtrim_whitespace(str);
    }

size_t chunked_encoding::add_chunked_delimiters(_Out_writes_ (buffer_size) uint8_t *data, _In_ size_t buffer_size, size_t bytes_read)
{
    size_t offset = 0;

    if(buffer_size < bytes_read + http::details::chunked_encoding::additional_encoding_space)
    {
        throw http_exception(_XPLATSTR("Insufficient buffer size."));
    }

    if ( bytes_read == 0 )
    {
        offset = 5;
        data[5] = '0';
        data[6] = '\r';  data[7] = '\n'; // The end of the size.
        data[8] = '\r';  data[9] = '\n'; // The end of the (empty) trailer.
        data[10] = '\r'; data[11] = '\n'; // The end of the message.
    }
    else
    {
        char buffer[9];
#ifdef _MS_WINDOWS
        sprintf_s(buffer, 9, "%8IX", bytes_read);
#else
# if __x86_64__
        sprintf(buffer, "%8lX", bytes_read);
# else
        sprintf(buffer, "%8X", bytes_read);
# endif
#endif
        memcpy(&data[0], buffer, 8);
        while (data[offset] == ' ') ++offset;
        data[8] = '\r'; data[9] = '\n'; // The end of the size.
        data[10+bytes_read] = '\r'; data[11+bytes_read] = '\n'; // The end of the chunk.
    }

    return offset;
}


} // namespace details
}} // namespace web::http
