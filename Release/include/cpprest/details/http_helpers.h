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

#include "cpprest/details/basic_types.h"

#if __APPLE__
     #include "TargetConditionals.h"
#endif

// CPPREST_EXCLUDE_COMPRESSION is set if we're on a platform that supports compression but we want to explicitly disable it.
// CPPREST_EXCLUDE_WEBSOCKETS is a flag that now essentially means "no external dependencies". TODO: Rename
#if (defined(_WIN32) || defined(TARGET_OS_MAC)) && !defined(CPPREST_EXCLUDE_WEBSOCKETS) && !defined(CPPREST_EXCLUDE_COMPRESSION)
#define CPPREST_HTTP_COMPRESSION
#endif

#include "cpprest/http_msg.h"

namespace web { namespace http
{
namespace details
{

    /// <summary>
    /// Helper function to get the default HTTP reason phrase for a status code.
    /// </summary>
    utility::string_t get_default_reason_phrase(status_code code);

    // simple helper functions to trim whitespace.
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

    namespace compression
    {
        enum class compression_algorithm : int 
        { 
            deflate = 15,
            gzip = 31,
            invalid = 9999
        };

        using data_buffer = std::vector<uint8_t>;

        class stream_decompressor
        {
        public:

            static compression_algorithm to_compression_algorithm(const utility::string_t& alg)
            {
                if (U("gzip") == alg)
                {
                    return compression_algorithm::gzip;
                }
                else if (U("deflate") == alg)
                {
                    return compression_algorithm::deflate;
                }

                return compression_algorithm::invalid;
            }

            static bool is_supported()
            {
#if !defined(CPPREST_HTTP_COMPRESSION) 
                return false;
#else
                return true;
#endif
            }

            _ASYNCRTIMP stream_decompressor(compression_algorithm alg);

            _ASYNCRTIMP data_buffer decompress(const data_buffer& input);

            _ASYNCRTIMP data_buffer decompress(const uint8_t* input, size_t input_size);

            _ASYNCRTIMP bool has_error() const;

        private:
            class stream_decompressor_impl;
            std::shared_ptr<stream_decompressor_impl> m_pimpl;
        };

        class stream_compressor
        {
        public:

            static bool is_supported()
            {
#if !defined(CPPREST_HTTP_COMPRESSION) 
                return false;
#else
                return true;
#endif
            }

            _ASYNCRTIMP stream_compressor(compression_algorithm alg);

            _ASYNCRTIMP data_buffer compress(const data_buffer& input, bool finish);

            _ASYNCRTIMP data_buffer compress(const uint8_t* input, size_t input_size, bool finish);

            _ASYNCRTIMP bool has_error() const;

        private:
            class stream_compressor_impl;
            std::shared_ptr<stream_compressor_impl> m_pimpl;
        };

    }
}}}
