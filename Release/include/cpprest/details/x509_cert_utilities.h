/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Contains utility functions for helping to verify server certificates in OS X/iOS and Android.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <string>

#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__)) || (defined(_WIN32)  && !defined(__cplusplus_winrt) && !defined(_M_ARM) && !defined(CPPREST_EXCLUDE_WEBSOCKETS))

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4005)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#include <boost/asio/ssl.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace web { namespace http { namespace client { namespace details {

/// <summary>
/// Using platform specific APIs verifies server certificate.
/// Currently implemented to work on iOS, Android, and OS X.
/// </summary>
/// <param name="verifyCtx">Boost.ASIO context to get certificate chain from.</param>
/// <param name="hostName">Host name from the URI.</param>
/// <returns>True if verification passed and server can be trusted, false otherwise.</returns>
bool verify_cert_chain_platform_specific(boost::asio::ssl::verify_context &verifyCtx, const std::string &hostName);

}}}}

#endif