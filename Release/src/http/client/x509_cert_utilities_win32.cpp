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
* Contains utility functions for helping to verify server certificates on Windows desktop.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#include "cpprest/details/x509_cert_utilities.h"

#include <type_traits>
#include <wincrypt.h>

namespace web { namespace http { namespace client { namespace details {

// Helper RAII unique_ptrs to free Windows structures.
struct cert_free_certificate_context
{
    void operator()(const CERT_CONTEXT *ctx) const
    {
        CertFreeCertificateContext(ctx);
    }
};
typedef std::unique_ptr<const CERT_CONTEXT, cert_free_certificate_context> cert_context;
struct cert_free_certificate_chain
{
    void operator()(const CERT_CHAIN_CONTEXT *chain) const
    {
        CertFreeCertificateChain(chain);
    }
};
typedef std::unique_ptr<const CERT_CHAIN_CONTEXT, cert_free_certificate_chain> chain_context;

bool verify_X509_cert_chain(const std::vector<std::string> &certChain, const std::string &)
{
    // Create certificate context from server certificate.
    cert_context cert(CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        reinterpret_cast<const unsigned char *>(certChain[0].c_str()),
        static_cast<DWORD>(certChain[0].size())));
    if (cert == nullptr)
    {
        return false;
    }

    // Let the OS build a certificate chain from the server certificate.
    CERT_CHAIN_PARA params;
    ZeroMemory(&params, sizeof(params));
    params.cbSize = sizeof(CERT_CHAIN_PARA);
    params.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
    LPSTR usages [] =
    {
        szOID_PKIX_KP_SERVER_AUTH,
        
        // For older servers and to match IE.
        szOID_SERVER_GATED_CRYPTO,
        szOID_SGC_NETSCAPE
    };
    params.RequestedUsage.Usage.cUsageIdentifier = std::extent<decltype(usages)>::value;
    params.RequestedUsage.Usage.rgpszUsageIdentifier = usages;
    PCCERT_CHAIN_CONTEXT chainContext;
    chain_context chain;
    if (!CertGetCertificateChain(
        nullptr,
        cert.get(),
        nullptr,
        nullptr,
        &params,
        CERT_CHAIN_REVOCATION_CHECK_CHAIN,
        nullptr,
        &chainContext))
    {
        return false;
    }
    chain.reset(chainContext);

    // Check to see if the certificate chain is actually trusted.
    if (chain->TrustStatus.dwErrorStatus != CERT_TRUST_NO_ERROR)
    {
        return false;
    }

    return true;
}

}}}}