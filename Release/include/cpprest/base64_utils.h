/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Base64 utilities.
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#pragma once

#include "cpprest/details/basic_types.h"
#include <string>
#include <vector>

/// Various utilities for string conversions and date and time manipulation.
namespace utility
{
/// Functions for Unicode string conversions.
namespace conversions
{
/// <summary>
/// Encode the given byte array into a base64 string
/// </summary>
_ASYNCRTIMP utility::string_t __cdecl to_base64(const std::vector<unsigned char>& data);

/// <summary>
/// Encode the given 8-byte integer into a base64 string
/// </summary>
_ASYNCRTIMP utility::string_t __cdecl to_base64(uint64_t data);

/// <summary>
/// Decode the given base64 string to a byte array
/// </summary>
_ASYNCRTIMP std::vector<unsigned char> __cdecl from_base64(utility::string_view_t str);

} // namespace conversions

} // namespace utility
