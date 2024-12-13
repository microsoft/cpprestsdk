/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Certificate info
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#pragma once

#include <string>
#include <vector>

namespace web
{
namespace http
{
namespace client
{
using CertificateChain = std::vector<std::vector<unsigned char>>;

struct certificate_info
{
    std::string host_name;
    CertificateChain certificate_chain;
    long certificate_error {0};
    bool verified {false};

    certificate_info(const std::string host) : host_name(host) { }
    certificate_info(const std::string host, CertificateChain chain, long error = 0)
        : host_name(host), certificate_chain(chain), certificate_error(error)
    {
    }
};

using CertificateChainFunction = std::function<bool(const std::shared_ptr<certificate_info> certificate_Info)>;

} // namespace client
} // namespace http
} // namespace web
