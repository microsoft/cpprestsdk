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
* http_helpers.h - Implementation Details of the http.h layer of messaging
*
* Functions and types for interoperating with http.h from modern C++
*   This file includes windows definitions and should not be included in a public header
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once


// this is a windows header
#ifdef _MS_WINDOWS
#include "cpprest/targetver.h"
#endif

#include "cpprest/http_msg.h"
#include "cpprest/xxpublic.h"

namespace web { namespace http
{
namespace details
{
    // Constants for MIME types.
    const utility::string_t content_type_application_json = _XPLATSTR("application/json");

    const utility::string_t content_type_application_octetstream = _XPLATSTR("application/octet-stream");
    const utility::string_t content_type_text_plain = _XPLATSTR("text/plain");
    const utility::string_t content_type_text_plain_utf8 = _XPLATSTR("text/plain; charset=utf-8");
    const utility::string_t content_type_text_plain_utf16 = _XPLATSTR("text/plain; charset=utf-16");
    const utility::string_t content_type_text_plain_utf16le = _XPLATSTR("text/plain; charset=utf-16le");

    // Unofficial JSON MIME types...
    const utility::string_t content_type_text_json = _XPLATSTR("text/json");
    const utility::string_t content_type_text_xjson = _XPLATSTR("text/x-json");
    const utility::string_t content_type_text_javascript = _XPLATSTR("text/javascript");
    const utility::string_t content_type_text_xjavascript = _XPLATSTR("text/x-javascript");
    const utility::string_t content_type_application_javascript = _XPLATSTR("application/javascript");
    const utility::string_t content_type_application_xjavascript = _XPLATSTR("application/x-javascript");

    // Constants for differnet charsets.
    const utility::string_t charset_usascii = _XPLATSTR("us-ascii");
    const utility::string_t charset_latin1 = _XPLATSTR("iso-8859-1");
    const utility::string_t charset_utf8 = _XPLATSTR("utf-8");
    const utility::string_t charset_utf16 = _XPLATSTR("utf-16");
    const utility::string_t charset_utf16le = _XPLATSTR("utf-16le");
    const utility::string_t charset_utf16be = _XPLATSTR("utf-16be");

    /// <summary>
    /// Determines whether or not the given content type is 'textual' according the feature specifications.
    /// </summary>
    bool is_content_type_textual(const utility::string_t &content_type);

    /// <summary>
    /// Determines whether or not the given content type is JSON according the feature specifications.
    /// </summary>
    bool is_content_type_json(const utility::string_t &content_type);

    /// <summary>
    /// Parses the given Content-Type header value to get out actual content type and charset.
    /// If the charset isn't specified the default charset for the content type will be set.
    /// </summary>
    void parse_content_type_and_charset(const utility::string_t &content_type, utility::string_t &content, utility::string_t &charset); 

    /// <summary>
    /// Gets the default charset for given content type. If the MIME type is not textual or recognized Latin1 will be returned.
    /// </summary>
    utility::string_t get_default_charset(const utility::string_t &content_type);

    /// <summary>
    /// Helper functions to convert a series of bytes from a charset to utf-8 or utf-16.
    /// </summary>
    std::string convert_latin1_to_utf8(const std::vector<unsigned char> &src);
    std::string convert_latin1_to_utf8(const unsigned char *src, size_t src_size);
    utf16string convert_latin1_to_utf16(const std::vector<unsigned char> &src);
    utf16string convert_latin1_to_utf16(const unsigned char *src, size_t src_size);
    std::string convert_utf16_to_utf8(const utf16string &src);
    utf16string convert_utf8_to_utf16(const unsigned char *src, size_t src_size);
    std::string convert_utf16_to_utf8(const std::vector<unsigned char> &src);
    utf16string convert_utf16_to_utf16(utf16string &&src);
    utf16string convert_utf16_to_utf16(const unsigned char *src, size_t src_size);
    utf16string convert_utf16_to_utf16(const std::vector<unsigned char> &src);
    utf16string convert_utf8_to_utf16(const std::vector<unsigned char> &src);
    std::string convert_utf16le_to_utf8(const utf16string &src, bool erase_bom);
    std::string convert_utf16le_to_utf8(const std::vector<unsigned char> &src, bool erase_bom);
    std::string convert_utf16be_to_utf8(const utf16string &src, bool erase_bom);
    std::string convert_utf16be_to_utf8(const std::vector<unsigned char> &src, bool erase_bom);
    utf16string convert_utf16be_to_utf16le(const utf16string &src, bool erase_bom);
    utf16string convert_utf16be_to_utf16le(const unsigned char *src, size_t src_size, bool erase_bom);
    utf16string convert_utf16be_to_utf16le(const std::vector<unsigned char> &src, bool erase_bom);

    // Helper functions to extract a string from a series of bytes.
    std::string convert_bytes_to_string(const std::vector<unsigned char> &src);
    utf16string convert_bytes_to_wstring(const std::vector<unsigned char> &src);
    std::string convert_bytes_to_string(const unsigned char *src, size_t src_size);
    utf16string convert_bytes_to_wstring(const unsigned char *src, size_t src_size);

    // simple helper functions to trim whitespace.
    void ltrim_whitespace(utility::string_t &str);
    void rtrim_whitespace(utility::string_t &str);
    void trim_whitespace(utility::string_t &str);

    namespace chunked_encoding
    {
        // Transfer-Encoding: chunked support
        static const size_t additional_encoding_space = 12;
        static const size_t data_offset               = additional_encoding_space-2;

        // Add the data necessary for properly sending data with transfer-encoding: chunked.
        //
        // There are up to 12 additional bytes needed for each chunk:
        //
        // The last chunk requires 7 bytes, and is fixed.
        // All other chunks require up to 8 bytes for the length, and four for the two CRLF
        // delimiters.
        //
        _ASYNCRTIMP size_t add_chunked_delimiters(_Out_writes_ (buffer_size) uint8_t *data, _In_ size_t buffer_size, size_t bytes_read);
    }

} // namespace details
}} // namespace web::http
