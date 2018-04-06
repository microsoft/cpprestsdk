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
#include "cpprest/certificate_info.h"

#if defined(__linux__) || defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__)) || (defined(_WIN32)  && !defined(__cplusplus_winrt) && !defined(_M_ARM) && !defined(CPPREST_EXCLUDE_WEBSOCKETS))

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

using namespace utility;

bool is_end_certificate_in_chain(boost::asio::ssl::verify_context &verifyCtx);

#if !defined(__linux__)
/// <summary>
/// Using platform specific APIs verifies server certificate.
/// Currently implemented to work on iOS, Android, and OS X.
/// </summary>
/// <param name="verifyCtx">Boost.ASIO context to get certificate chain from.</param>
/// <param name="hostName">Host name from the URI.</param>
/// <returns>True if verification passed and server can be trusted, false otherwise.</returns>
bool verify_cert_chain_platform_specific(boost::asio::ssl::verify_context &verifyCtx, const std::string &hostName, const CertificateChainFunction& func = nullptr);
bool verify_X509_cert_chain(const std::vector<std::string> &certChain, const std::string &hostName, const CertificateChainFunction& func = nullptr);

#endif


std::vector<std::vector<unsigned char>> get_X509_cert_chain_encoded_data(boost::asio::ssl::verify_context &verifyCtx);

}}}}

#endif
