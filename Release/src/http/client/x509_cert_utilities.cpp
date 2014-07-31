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
* x509_cert_utilities.cpp
*
* Contains utility functions for helping to verify server certificates in OS X/iOS.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "cpprest/x509_cert_utilities.h"

#include <vector>
#include <string>

#if defined(__APPLE__)
#include <CoreFoundation/CFData.h>
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#endif

namespace web { namespace http { namespace client { namespace details {

#if defined(__APPLE__)

// Simple RAII pattern wrapper to perform CFRelease on objects.
template <typename T>
class CFRef
{
public:
    CFRef(T v) : value(v) {}
    CFRef() : value(nullptr) {}

    ~CFRef()
    {
        if(value != nullptr)
        {
            CFRelease(value);
        }
    }

    T & Get()
    {
        return value;
    }
private:
    T value;
};
    
template <typename T>
class CFVectorRef
{
public:
    CFVectorRef() {}
    ~CFVectorRef()
    {
        for(auto & item : container)
        {
            CFRelease(item);
        }
    }
    
    void push_back(T item)
    {
        container.push_back(item);
    }
    std::vector<T> & items()
    {
        return container;
    }
    
private:
    std::vector<T> container;
};

bool verify_X509_cert_chain(const std::vector<std::string> &certChain, const std::string &hostName)
{
    // Build up CFArrayRef with all the certificates.
    // All this code is basically just to get into the correct structures for the Apple APIs.
    // Copies are avoided whenever possible.
    CFVectorRef<SecCertificateRef> certs;
    for(const auto & certBuf : certChain)
    {
        CFRef<CFDataRef> certDataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                                   reinterpret_cast<const unsigned char*>(certBuf.c_str()),
                                                                   certBuf.size(),
                                                                   kCFAllocatorNull);
        if(certDataRef.Get() == nullptr)
        {
            return false;
        }
        
        SecCertificateRef certObj = SecCertificateCreateWithData(nullptr, certDataRef.Get());
        if(certObj == nullptr)
        {
            return false;
        }
        certs.push_back(certObj);
    }
    CFRef<CFArrayRef> certsArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&certs.items()[0], certs.items().size(), nullptr);
    if(certsArray.Get() == nullptr)
    {
        return false;
    }
    
    // Create trust management object with certificates and SSL policy.
    // Note: SecTrustCreateWithCertificates expects the certificate to be
    // verified is the first element.
    CFRef<CFStringRef> cfHostName = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
                                                                    &hostName[0],
                                                                    kCFStringEncodingASCII,
                                                                    kCFAllocatorNull);
    if(cfHostName.Get() == nullptr)
    {
        return false;
    }
    CFRef<SecPolicyRef> policy = SecPolicyCreateSSL(true /* client side */, cfHostName.Get());
    CFRef<SecTrustRef> trust;
    OSStatus status = SecTrustCreateWithCertificates(certsArray.Get(), policy.Get(), &trust.Get());
    if(status == noErr)
    {
        // Perform actual certificate verification.
        SecTrustResultType trustResult;
        status = SecTrustEvaluate(trust.Get(), &trustResult);
        if(status == noErr && (trustResult == kSecTrustResultUnspecified || trustResult == kSecTrustResultProceed))
        {
            return true;
        }
    }

    return false;
}
#endif

}}}}
