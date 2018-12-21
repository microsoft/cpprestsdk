/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 ****/

#pragma once

#include "cpprest/details/basic_types.h"
#include <string>

namespace web
{
namespace http
{
namespace details
{
/// <summary>
/// Helper function to get the default HTTP reason phrase for a status code.
/// </summary>
utility::string_t get_default_reason_phrase(status_code code);

// simple helper functions to trim whitespace.
template<class Char>
void trim_whitespace(std::basic_string<Char>& str)
{
    size_t index;
    // trim left whitespace
    for (index = 0; index < str.size() && isspace(str[index]); ++index)
        ;
    str.erase(0, index);
    // trim right whitespace
    for (index = str.size(); index > 0 && isspace(str[index - 1]); --index)
        ;
    str.erase(index);
}

bool validate_method(const utility::string_t& method);

} // namespace details
} // namespace http
} // namespace web

namespace web
{
namespace http
{
namespace compression
{
class decompress_factory;

namespace details
{
namespace builtin
{
/// <summary>
/// Helper function to get the set of built-in decompress factories
/// </summary>

const std::vector<std::shared_ptr<web::http::compression::decompress_factory>> get_decompress_factories();

} // namespace builtin
} // namespace details

} // namespace compression
} // namespace http
} // namespace web
