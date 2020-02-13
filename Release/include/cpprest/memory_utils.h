/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Memory utilities.
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#pragma once

#include <memory>
#include <type_traits>

/// Various utilities for string conversions and date and time manipulation.
namespace utility
{

namespace details
{

/// <summary>
/// Simplistic implementation of make_unique. A better implementation would be based on variadic templates
/// and therefore not be compatible with Dev10.
/// </summary>
template<typename _Type>
std::unique_ptr<_Type> make_unique()
{
    return std::unique_ptr<_Type>(new _Type());
}

template<typename _Type, typename _Arg1>
std::unique_ptr<_Type> make_unique(_Arg1&& arg1)
{
    return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1)));
}

template<typename _Type, typename _Arg1, typename _Arg2>
std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2)
{
    return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1), std::forward<_Arg2>(arg2)));
}

template<typename _Type, typename _Arg1, typename _Arg2, typename _Arg3>
std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2, _Arg3&& arg3)
{
    return std::unique_ptr<_Type>(
        new _Type(std::forward<_Arg1>(arg1), std::forward<_Arg2>(arg2), std::forward<_Arg3>(arg3)));
}

template<typename _Type, typename _Arg1, typename _Arg2, typename _Arg3, typename _Arg4>
std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2, _Arg3&& arg3, _Arg4&& arg4)
{
    return std::unique_ptr<_Type>(new _Type(
        std::forward<_Arg1>(arg1), std::forward<_Arg2>(arg2), std::forward<_Arg3>(arg3), std::forward<_Arg4>(arg4)));
}

template<typename _Type, typename _Arg1, typename _Arg2, typename _Arg3, typename _Arg4, typename _Arg5>
std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2, _Arg3&& arg3, _Arg4&& arg4, _Arg5&& arg5)
{
    return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1),
                                            std::forward<_Arg2>(arg2),
                                            std::forward<_Arg3>(arg3),
                                            std::forward<_Arg4>(arg4),
                                            std::forward<_Arg5>(arg5)));
}

template<typename _Type, typename _Arg1, typename _Arg2, typename _Arg3, typename _Arg4, typename _Arg5, typename _Arg6>
std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2, _Arg3&& arg3, _Arg4&& arg4, _Arg5&& arg5, _Arg6&& arg6)
{
    return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1),
                                            std::forward<_Arg2>(arg2),
                                            std::forward<_Arg3>(arg3),
                                            std::forward<_Arg4>(arg4),
                                            std::forward<_Arg5>(arg5),
                                            std::forward<_Arg6>(arg6)));
}

} // namespace details

} // namespace utility
