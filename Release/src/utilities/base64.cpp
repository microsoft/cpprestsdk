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
* asyncrt_utils.cpp - Utilities
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace utility;

#define _USE_INTERNAL_BASE64_
std::vector<unsigned char> _from_base64(const utility::string_t& str);
utility::string_t _to_base64(const unsigned char *ptr, size_t size);

std::vector<unsigned char> __cdecl conversions::from_base64(const utility::string_t& str)
{
    return _from_base64(str);
}

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


#if defined(_USE_INTERNAL_BASE64_)
static const char* _base64_enctbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
unsigned char _base64_dectbl [] = 
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
       52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255, 254, 255, 255,
      255,  0,    1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
       15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
      255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
       41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255 };

struct _triple_byte
{
    unsigned char _1_1 : 2;
    unsigned char _0   : 6;
    unsigned char _2_1 : 4;
    unsigned char _1_2 : 4;
    unsigned char _3   : 6;
    unsigned char _2_2 : 2;
};

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

    if ( input.empty() ) 
        return result;

    size_t padding = 0;

    // Validation
    {
        auto size = input.size();

        if ( (size % 4) != 0 )
        {
            throw std::runtime_error("length of base64 string is not an even multiple of 4");
        }

        for (auto iter = input.begin(); iter != input.end(); ++iter,--size)
        {
            auto ch = *iter;
            if ( ch < 0 || _base64_dectbl[ch] == 255 )
            {
                throw std::runtime_error("invalid character found in base64 string");
            }
            if ( _base64_dectbl[ch] == 254 )
            {
                padding++;
                // padding only at the end
                if ( size > 2 || (size == 2 && _base64_dectbl[*(iter+1)] != 254) )
                {
                    throw std::runtime_error("invalid padding character found in base64 string");
                }
            }
        }
    }


    auto size = input.size();
    const char_t* ptr = &input[0];

    auto outsz = (size / 4)*3;
    outsz -= padding;

    result.resize(outsz);

    size_t idx = 0;
    for (; size > 4; ++idx )
    {
        unsigned char target[3];
        memset(target, 0, sizeof(target));
        _triple_byte* record = reinterpret_cast<_triple_byte*>(target);

        unsigned char val0 = _base64_dectbl[ptr[0]];
        unsigned char val1 = _base64_dectbl[ptr[1]];
        unsigned char val2 = _base64_dectbl[ptr[2]];
        unsigned char val3 = _base64_dectbl[ptr[3]];

        record->_0   = val0;
        record->_1_1 = val1 >> 4;
        result[idx] = target[0];

        record->_1_2 = val1 & 0xF;
        record->_2_1 = val2 >> 2;
        result[++idx] = target[1];

        record->_2_2 = val2 & 0x3;
        record->_3   = val3 & 0x3F;
        result[++idx] = target[2];

        ptr += 4;
        size -= 4;
    }

    // Handle the last four bytes separately, to avoid having the conditional statements
    // in all the iterations (a performance issue).

    { 
        unsigned char target[3];
        memset(target, 0, sizeof(target));
        _triple_byte* record = reinterpret_cast<_triple_byte*>(target);

        unsigned char val0 = _base64_dectbl[ptr[0]];
        unsigned char val1 = _base64_dectbl[ptr[1]];
        unsigned char val2 = _base64_dectbl[ptr[2]];
        unsigned char val3 = _base64_dectbl[ptr[3]];

        record->_0   = val0;
        record->_1_1 = val1 >> 4;
        result[idx] = target[0];

        record->_1_2 = val1 & 0xF;
        if ( val2 != 254 )
        {
            record->_2_1 = val2 >> 2;
            result[++idx] = target[1];
        }
        else
        {
            // There shouldn't be any information (ones) in the unused bits,
            if ( record->_1_2 != 0 )
            {
                throw std::runtime_error("Invalid end of base64 string");
            }
                return result;
        }

        record->_2_2 = val2 & 0x3;
        if ( val3 != 254 )
        {
            record->_3   = val3 & 0x3F;
            result[++idx] = target[2];
        }
        else
        {
            // There shouldn't be any information (ones) in the unused bits.
            if ( record->_2_2 != 0 )
            {
                throw std::runtime_error("Invalid end of base64 string");
            }
                return result;
        }
    }

    return result;
}

utility::string_t _to_base64(const unsigned char *ptr, size_t size)
{
    utility::string_t result;
    
    for (; size >= 3; )
    {
        const _triple_byte* record = reinterpret_cast<const _triple_byte*>(ptr);
        unsigned char idx0 = record->_0;
        unsigned char idx1 = (record->_1_1 << 4) | record->_1_2;
        unsigned char idx2 = (record->_2_1 << 2) | record->_2_2;
        unsigned char idx3 = record->_3;
        result.push_back(char_t(_base64_enctbl[idx0]));
        result.push_back(char_t(_base64_enctbl[idx1]));
        result.push_back(char_t(_base64_enctbl[idx2]));
        result.push_back(char_t(_base64_enctbl[idx3]));
        size -= 3;
        ptr += 3;
    }
    switch(size)
    {
        case 1:
        {
            const _triple_byte* record = reinterpret_cast<const _triple_byte*>(ptr);
            unsigned char idx0 = record->_0;
            unsigned char idx1 = (record->_1_1 << 4);
            result.push_back(char_t(_base64_enctbl[idx0]));
            result.push_back(char_t(_base64_enctbl[idx1]));
            result.push_back('=');
            result.push_back('=');
            break;
        }
        case 2:
        {
            const _triple_byte* record = reinterpret_cast<const _triple_byte*>(ptr);
            unsigned char idx0 = record->_0;
            unsigned char idx1 = (record->_1_1 << 4) | record->_1_2;
            unsigned char idx2 = (record->_2_1 << 2);
            result.push_back(char_t(_base64_enctbl[idx0]));
            result.push_back(char_t(_base64_enctbl[idx1]));
            result.push_back(char_t(_base64_enctbl[idx2]));
            result.push_back('=');
            break;
        }
    }
    return result;
}
#endif
