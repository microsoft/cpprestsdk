/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/
#include "stdafx.h"

using namespace web;
using namespace utility;

std::vector<unsigned char> _from_base64(const utility::string_t& str);
utility::string_t _to_base64(const unsigned char* ptr, size_t size);

std::vector<unsigned char> __cdecl conversions::from_base64(const utility::string_t& str) { return _from_base64(str); }

utility::string_t __cdecl conversions::to_base64(const std::vector<unsigned char>& input)
{
    if (input.size() == 0)
    {
        // return empty string
        return utility::string_t();
    }

    return _to_base64(&input[0], input.size());
}

utility::string_t __cdecl conversions::to_base64(uint64_t input)
{
    return _to_base64(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}

static const char* _base64_enctbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const std::array<unsigned char, 128> _base64_dectbl = {
    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62,
     255, 255, 255, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255, 255, 254, 255, 255, 255, 0,
     1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
     23,  24,  25,  255, 255, 255, 255, 255, 255, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
     39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  255, 255, 255, 255, 255}};

//
// A note on the implementation of BASE64 encoding and decoding:
//
// This is a fairly basic and naive implementation; there is probably a lot of room for
// performance improvement, as well as for adding options such as support for URI-safe base64,
// ignoring CRLF, relaxed validation rules, etc. The decoder is currently pretty strict.
//

#ifdef __GNUC__
// gcc is concerned about the bitfield uses in the code, something we simply need to ignore.
#pragma GCC diagnostic ignored "-Wconversion"
#endif
std::vector<unsigned char> _from_base64(const utility::string_t& input)
{
    std::vector<unsigned char> result;

    if (input.empty()) return result;

    size_t padding = 0;

    // Validation
    {
        auto size = input.size();

        if ((size % 4) != 0)
        {
            throw std::runtime_error("length of base64 string is not an even multiple of 4");
        }

        for (auto iter = input.begin(); iter != input.end(); ++iter, --size)
        {
            const size_t ch_sz = static_cast<size_t>(*iter);
            if (ch_sz >= _base64_dectbl.size() || _base64_dectbl[ch_sz] == 255)
            {
                throw std::runtime_error("invalid character found in base64 string");
            }
            if (_base64_dectbl[ch_sz] == 254)
            {
                padding++;
                // padding only at the end
                if (size > 2)
                {
                    throw std::runtime_error("invalid padding character found in base64 string");
                }
                if (size == 2)
                {
                    const size_t ch2_sz = static_cast<size_t>(*(iter + 1));
                    if (ch2_sz >= _base64_dectbl.size() || _base64_dectbl[ch2_sz] != 254)
                    {
                        throw std::runtime_error("invalid padding character found in base64 string");
                    }
                }
            }
        }
    }

    auto size = input.size();
    const char_t* ptr = &input[0];

    auto outsz = (size / 4) * 3;
    outsz -= padding;

    result.resize(outsz);

    size_t idx = 0;
    for (; size > 4; ++idx)
    {
        unsigned char val0 = _base64_dectbl[ptr[0]];
        unsigned char val1 = _base64_dectbl[ptr[1]];
        unsigned char val2 = _base64_dectbl[ptr[2]];
        unsigned char val3 = _base64_dectbl[ptr[3]];

        result[idx] = (val0 << 2) | (val1 >> 4);
        result[++idx] = ((val1 & 0xF) << 4) | (val2 >> 2);
        result[++idx] = ((val2 & 0x3) << 6) | (val3 & 0x3F);

        ptr += 4;
        size -= 4;
    }

    // Handle the last four bytes separately, to avoid having the conditional statements
    // in all the iterations (a performance issue).

    {
        unsigned char val0 = _base64_dectbl[ptr[0]];
        unsigned char val1 = _base64_dectbl[ptr[1]];
        unsigned char val2 = _base64_dectbl[ptr[2]];
        unsigned char val3 = _base64_dectbl[ptr[3]];

        result[idx] = (val0 << 2) | (val1 >> 4);

        if (val2 != 254)
        {
            result[++idx] = ((val1 & 0xF) << 4) | (val2 >> 2);
        }
        else
        {
            // There shouldn't be any information (ones) in the unused bits,
            if ((val1 & 0xF) != 0)
            {
                throw std::runtime_error("Invalid end of base64 string");
            }
            return result;
        }

        if (val3 != 254)
        {
            result[++idx] = ((val2 & 0x3) << 6) | (val3 & 0x3F);
        }
        else
        {
            // There shouldn't be any information (ones) in the unused bits.
            if ((val2 & 0x3) != 0)
            {
                throw std::runtime_error("Invalid end of base64 string");
            }
            return result;
        }
    }

    return result;
}

utility::string_t _to_base64(const unsigned char* ptr, size_t size)
{
    utility::string_t result;

    for (; size >= 3;)
    {
        unsigned char idx0 = ptr[0] >> 2;
        unsigned char idx1 = ((ptr[0] & 0x3) << 4) | (ptr[1] >> 4);
        unsigned char idx2 = ((ptr[1] & 0xF) << 2) | (ptr[2] >> 6);
        unsigned char idx3 = ptr[2] & 0x3F;
        result.push_back(char_t(_base64_enctbl[idx0]));
        result.push_back(char_t(_base64_enctbl[idx1]));
        result.push_back(char_t(_base64_enctbl[idx2]));
        result.push_back(char_t(_base64_enctbl[idx3]));
        size -= 3;
        ptr += 3;
    }
    switch (size)
    {
        case 1:
        {
            unsigned char idx0 = ptr[0] >> 2;
            unsigned char idx1 = ((ptr[0] & 0x3) << 4);
            result.push_back(char_t(_base64_enctbl[idx0]));
            result.push_back(char_t(_base64_enctbl[idx1]));
            result.push_back('=');
            result.push_back('=');
            break;
        }
        case 2:
        {
            unsigned char idx0 = ptr[0] >> 2;
            unsigned char idx1 = ((ptr[0] & 0x3) << 4) | (ptr[1] >> 4);
            unsigned char idx2 = ((ptr[1] & 0xF) << 2);
            result.push_back(char_t(_base64_enctbl[idx0]));
            result.push_back(char_t(_base64_enctbl[idx1]));
            result.push_back(char_t(_base64_enctbl[idx2]));
            result.push_back('=');
            break;
        }
    }
    return result;
}
