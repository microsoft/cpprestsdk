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
* Functions and types for interoperating with http.h from modern C++
*   This file includes windows definitions and should not be included in a public header
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "cpprest/http_msg.h"

namespace web { namespace http
{
namespace details
{

    /// <summary>
    /// Constants for MIME types.
    /// </summary>
    class mime_types
    {
    public:
    #define _MIME_TYPES
    #define DAT(a,b) _ASYNCRTIMP const static utility::string_t a;
    #include "cpprest/details/http_constants.dat"
    #undef _MIME_TYPES
    #undef DAT
    };

    /// <summary>
    /// Constants for charset types.
    /// </summary>
    class charset_types
    {
    public:
    #define _CHARSET_TYPES
    #define DAT(a,b) _ASYNCRTIMP const static utility::string_t a;
    #include "cpprest/details/http_constants.dat"
    #undef _CHARSET_TYPES
    #undef DAT
    };

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
    /// Helper function to get the default HTTP reason phrase for a status code.
    /// </summary>
    utility::string_t get_default_reason_phrase(status_code code);

    /// <summary>
    /// Helper functions to convert a series of bytes from a charset to utf-8 or utf-16.
    /// These APIs deal with checking for and handling byte order marker (BOM).
    /// </summary>
    utility::string_t convert_utf16_to_string_t(utf16string src);
    utf16string convert_utf16_to_utf16(utf16string src);
    std::string convert_utf16_to_utf8(utf16string src);
    utility::string_t convert_utf16le_to_string_t(utf16string src, bool erase_bom);
    std::string convert_utf16le_to_utf8(utf16string src, bool erase_bom);
    utility::string_t convert_utf16be_to_string_t(utf16string src, bool erase_bom);
    std::string convert_utf16be_to_utf8(utf16string src, bool erase_bom);
    utf16string convert_utf16be_to_utf16le(utf16string src, bool erase_bom);

    // simple helper functions to trim whitespace.
    _ASYNCRTIMP void __cdecl ltrim_whitespace(utility::string_t &str);
    _ASYNCRTIMP void __cdecl rtrim_whitespace(utility::string_t &str);
    _ASYNCRTIMP void __cdecl trim_whitespace(utility::string_t &str);

    bool validate_method(const utility::string_t& method);


    namespace chunked_encoding
    {
        // Transfer-Encoding: chunked support
        static const size_t additional_encoding_space = 12;
        static const size_t data_offset               = additional_encoding_space-2;

        // Add the data necessary for properly sending data with transfer-encoding: chunked.
        //
        // There are up to 12 additional bytes needed for each chunk:
        //
        // The last chunk requires 5 bytes, and is fixed.
        // All other chunks require up to 8 bytes for the length, and four for the two CRLF
        // delimiters.
        //
        _ASYNCRTIMP size_t __cdecl add_chunked_delimiters(_Out_writes_(buffer_size) uint8_t *data, _In_ size_t buffer_size, size_t bytes_read);
    }

}}}
