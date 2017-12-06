/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Implementation Details of the http.h layer of messaging
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

// CPPREST_EXCLUDE_COMPRESSION is set if we're on a platform that supports compression but we want to explicitly disable it.
// CPPREST_EXCLUDE_WEBSOCKETS is a flag that now essentially means "no external dependencies". TODO: Rename

#if __APPLE__
#include "TargetConditionals.h"
#if defined(TARGET_OS_MAC)
#if !defined(CPPREST_EXCLUDE_COMPRESSION)
#define CPPREST_HTTP_COMPRESSION
#endif // !defined(CPPREST_EXCLUDE_COMPRESSION)
#endif // defined(TARGET_OS_MAC)
#elif defined(_WIN32) && (!defined(WINAPI_FAMILY) || WINAPI_PARTITION_DESKTOP)
#if !defined(CPPREST_EXCLUDE_WEBSOCKETS) && !defined(CPPREST_EXCLUDE_COMPRESSION)
#define CPPREST_HTTP_COMPRESSION
#endif // !defined(CPPREST_EXCLUDE_WEBSOCKETS) && !defined(CPPREST_EXCLUDE_COMPRESSION)
#endif

#if defined(CPPREST_HTTP_COMPRESSION)
#include <zlib.h>
#endif

#include "internal_http_helpers.h"

using namespace web;
using namespace utility;
using namespace utility::conversions;

namespace web { namespace http
{
namespace details
{

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

static const std::array<bool,128> valid_chars =
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

namespace compression
{
#if defined(CPPREST_HTTP_COMPRESSION)

    class compression_base_impl
    {
    public:
        compression_base_impl(compression_algorithm alg) : m_alg(alg), m_zLibState(Z_OK)
        {
            memset(&m_zLibStream, 0, sizeof(m_zLibStream));
        }

        size_t read_output(size_t input_offset, size_t available_input, size_t total_out_before, uint8_t* temp_buffer, data_buffer& output)
        {
            input_offset += (available_input - stream().avail_in);
            auto out_length = stream().total_out - total_out_before;
            output.insert(output.end(), temp_buffer, temp_buffer + out_length);

            return input_offset;
        }

        bool is_complete() const
        {
            return state() == Z_STREAM_END;
        }

        bool has_error() const
        {
            return !is_complete() && state() != Z_OK;
        }

        int state() const
        {
            return m_zLibState;
        }

        void set_state(int state)
        {
            m_zLibState = state;
        }

        compression_algorithm algorithm() const
        {
            return m_alg;
        }

        z_stream& stream()
        {
            return m_zLibStream;
        }

        int to_zlib_alg(compression_algorithm alg)
        {
            return static_cast<int>(alg);
        }

    private:
        const compression_algorithm m_alg;

        std::atomic<int> m_zLibState{ Z_OK };
        z_stream m_zLibStream;
    };

    class stream_decompressor::stream_decompressor_impl : public compression_base_impl
    {
    public:
        stream_decompressor_impl(compression_algorithm alg) : compression_base_impl(alg)
        {
            set_state(inflateInit2(&stream(), to_zlib_alg(alg)));
        }

        ~stream_decompressor_impl()
        {
            inflateEnd(&stream());
        }

        data_buffer decompress(const uint8_t* input, size_t input_size)
        {
            if (input == nullptr || input_size == 0)
            {
                set_state(Z_BUF_ERROR);
                return data_buffer();
            }

            // Need to guard against attempting to decompress when we're already finished or encountered an error!
            if (is_complete() || has_error())
            {
                set_state(Z_STREAM_ERROR);
                return data_buffer();
            }

            const size_t BUFFER_SIZE = 1024;
            unsigned char temp_buffer[BUFFER_SIZE];

            data_buffer output;
            output.reserve(input_size * 3);

            size_t input_offset{ 0 };

            while (state() == Z_OK && input_offset < input_size)
            {
                auto total_out_before = stream().total_out;

                auto available_input = input_size - input_offset;
                stream().next_in = const_cast<uint8_t*>(&input[input_offset]);
                stream().avail_in = static_cast<int>(available_input);
                stream().next_out = temp_buffer;
                stream().avail_out = BUFFER_SIZE;

                set_state(inflate(&stream(), Z_PARTIAL_FLUSH));

                if (has_error())
                {
                    return data_buffer();
                }

                input_offset = read_output(input_offset, available_input, total_out_before, temp_buffer, output);
            }

            return output;
        }
    };

    class stream_compressor::stream_compressor_impl : public compression_base_impl
    {
    public:
        stream_compressor_impl(compression_algorithm alg) : compression_base_impl(alg)
        {
            const int level = Z_DEFAULT_COMPRESSION;
            if (alg == compression_algorithm::gzip)
            {
                set_state(deflateInit2(&stream(), level, Z_DEFLATED, to_zlib_alg(alg), MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY));
            }
            else if (alg == compression_algorithm::deflate)
            {
                set_state(deflateInit(&stream(), level));
            }
        }

        web::http::details::compression::data_buffer compress(const uint8_t* input, size_t input_size, bool finish)
        {
            if (input == nullptr || input_size == 0)
            {
                set_state(Z_BUF_ERROR);
                return data_buffer();
            }

            if (state() != Z_OK)
            {
                set_state(Z_STREAM_ERROR);
                return data_buffer();
            }

            data_buffer output;
            output.reserve(input_size);

            const size_t BUFFER_SIZE = 1024;
            uint8_t temp_buffer[BUFFER_SIZE];

            size_t input_offset{ 0 };
            auto flush = Z_NO_FLUSH;

            while (flush == Z_NO_FLUSH)
            {
                auto total_out_before = stream().total_out;
                auto available_input = input_size - input_offset;

                if (available_input == 0)
                {
                    flush = finish ? Z_FINISH : Z_PARTIAL_FLUSH;
                }
                else
                {
                    stream().avail_in = static_cast<int>(available_input);
                    stream().next_in = const_cast<uint8_t*>(&input[input_offset]);
                }

                do
                {
                    stream().next_out = temp_buffer;
                    stream().avail_out = BUFFER_SIZE;

                    set_state(deflate(&stream(), flush));

                    if (has_error())
                    {
                        return data_buffer();
                    }

                    input_offset = read_output(input_offset, available_input, total_out_before, temp_buffer, output);

                } while (stream().avail_out == 0);
            }

            return output;
        }

        ~stream_compressor_impl()
        {
            deflateEnd(&stream());
        }
    };
#else // Stub impl for when compression is not supported

    class compression_base_impl
    {
    public:
        bool has_error() const
        {
            return true;
        }
    };

    class stream_compressor::stream_compressor_impl : public compression_base_impl
    {
    public:
        stream_compressor_impl(compression_algorithm) {}
        compression::data_buffer compress(const uint8_t* data, size_t size, bool)
        {
            return data_buffer(data, data + size);
        }
    };

    class stream_decompressor::stream_decompressor_impl : public compression_base_impl
    {
    public:
        stream_decompressor_impl(compression_algorithm) {}
        compression::data_buffer decompress(const uint8_t* data, size_t size) 
        {
            return data_buffer(data, data + size);
        }
    };
#endif

    bool __cdecl stream_decompressor::is_supported()
    {
#if !defined(CPPREST_HTTP_COMPRESSION)
    return false;
#else
    return true;
#endif
    }

    stream_decompressor::stream_decompressor(compression_algorithm alg)
        : m_pimpl(std::make_shared<stream_decompressor::stream_decompressor_impl>(alg))
    {
    }

    compression::data_buffer stream_decompressor::decompress(const data_buffer& input)
    {
        if (input.empty())
        {
            return data_buffer();
        }

        return m_pimpl->decompress(&input[0], input.size());
    }

    web::http::details::compression::data_buffer stream_decompressor::decompress(const uint8_t* input, size_t input_size)
    {
        return m_pimpl->decompress(input, input_size);
    }

    bool stream_decompressor::has_error() const
    {
        return m_pimpl->has_error();
    }

    bool __cdecl stream_compressor::is_supported()
    {
#if !defined(CPPREST_HTTP_COMPRESSION)
        return false;
#else
        return true;
#endif
    }

    stream_compressor::stream_compressor(compression_algorithm alg)
        : m_pimpl(std::make_shared<stream_compressor::stream_compressor_impl>(alg))
    {

    }

    compression::data_buffer stream_compressor::compress(const data_buffer& input, bool finish)
    {
        if (input.empty())
        {
            return compression::data_buffer();
        }

        return m_pimpl->compress(&input[0], input.size(), finish);
    }

    web::http::details::compression::data_buffer stream_compressor::compress(const uint8_t* input, size_t input_size, bool finish)
    {
        return m_pimpl->compress(input, input_size, finish);
    }
    
    bool stream_compressor::has_error() const
    {
        return m_pimpl->has_error();
    }

} // namespace compression
} // namespace details
}} // namespace web::http
