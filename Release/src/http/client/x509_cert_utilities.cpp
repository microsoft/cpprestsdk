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

#include "stdafx.h"
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

#if defined(ANDROID)
#include <jni.h>
#endif

using namespace crossplat;

namespace web { namespace http { namespace client { namespace details {

#if defined(__APPLE__)

// Simple RAII pattern wrapper to perform CFRelease on objects.
template <typename T>
class cf_ref
{
public:
    cf_ref(T v) : value(v)
    {
        static_assert(sizeof(cf_ref<T>) == sizeof(T), "Code assumes just a wrapper, see usage in CFArrayCreate below.");
    }
    cf_ref() : value(nullptr) {}
    cf_ref(cf_ref &&other) : value(other.value) { other.value = nullptr; }
    
    ~cf_ref()
    {
        if(value != nullptr)
        {
            CFRelease(value);
        }
    }

    T & get()
    {
        return value;
    }
private:
    cf_ref(const cf_ref &);
    cf_ref & operator=(const cf_ref &);
    T value;
};

bool verify_X509_cert_chain(const std::vector<std::string> &certChain, const std::string &hostName)
{
    // Build up CFArrayRef with all the certificates.
    // All this code is basically just to get into the correct structures for the Apple APIs.
    // Copies are avoided whenever possible.
    std::vector<cf_ref<SecCertificateRef>> certs;
    for(const auto & certBuf : certChain)
    {
        cf_ref<CFDataRef> certDataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                                   reinterpret_cast<const unsigned char*>(certBuf.c_str()),
                                                                   certBuf.size(),
                                                                   kCFAllocatorNull);
        if(certDataRef.get() == nullptr)
        {
            return false;
        }
        
        cf_ref<SecCertificateRef> certObj = SecCertificateCreateWithData(nullptr, certDataRef.get());
        if(certObj.get() == nullptr)
        {
            return false;
        }
        certs.push_back(std::move(certObj));
    }
    cf_ref<CFArrayRef> certsArray = CFArrayCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<void **>(&certs[0])), certs.size(), nullptr);
    if(certsArray.get() == nullptr)
    {
        return false;
    }
    
    // Create trust management object with certificates and SSL policy.
    // Note: SecTrustCreateWithCertificates expects the certificate to be
    // verified is the first element.
    cf_ref<CFStringRef> cfHostName = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
                                                                    hostName.c_str(),
                                                                    kCFStringEncodingASCII,
                                                                    kCFAllocatorNull);
    if(cfHostName.get() == nullptr)
    {
        return false;
    }
    cf_ref<SecPolicyRef> policy = SecPolicyCreateSSL(true /* client side */, cfHostName.get());
    cf_ref<SecTrustRef> trust;
    OSStatus status = SecTrustCreateWithCertificates(certsArray.get(), policy.get(), &trust.get());
    if(status == noErr)
    {
        // Perform actual certificate verification.
        SecTrustResultType trustResult;
        status = SecTrustEvaluate(trust.get(), &trustResult);
        if(status == noErr && (trustResult == kSecTrustResultUnspecified || trustResult == kSecTrustResultProceed))
        {
            return true;
        }
    }

    return false;
}
#endif

#if defined(ANDROID)

/// <summary>
/// Helper function to check return value and see if any exceptions
/// occurred when calling a JNI function.
/// <summary>
/// <returns><c>true</c> if JNI call <c>failed</c>, false othewise.</returns>
bool jni_failed(JNIEnv *env)
{
    if(env->ExceptionOccurred())
    {
        // Clear exception otherwise no other JNI functions can be called.
        // In the future if we improve error reporting the exception message
        // can be retrieved from here.
        env->ExceptionClear();
        return true;
    }
    return false;
}
template <typename T>
bool jni_failed(JNIEnv *env, const T &result)
{
    if(jni_failed(env))
    {
        return true;
    }
    else if(result == nullptr)
    {
        return true;
    }
    return false;
}

bool verify_X509_cert_chain(const std::vector<std::string> &certChain, const std::string &hostName)
{
    JNIEnv* env = get_jvm_env();
    
    // Possible performance improvement:
    // In the future we could gain performance by turning all the jclass local
    // references into global references. Then we could lazy initialize and
    // save them globally. If this is done I'm not exactly sure where the release
    // should be. 

    // ByteArrayInputStream
    java_local_ref<jclass> byteArrayInputStreamClass(env->FindClass("java/io/ByteArrayInputStream"));
    if(jni_failed(env, byteArrayInputStreamClass))
    {
        return false;
    }
    jmethodID byteArrayInputStreamConstructorMethod = env->GetMethodID(
        byteArrayInputStreamClass.get(), 
        "<init>", 
        "([B)V");
    if(jni_failed(env, byteArrayInputStreamConstructorMethod))
    {
        return false;
    }

    // CertificateFactory
    java_local_ref<jclass> certificateFactoryClass(env->FindClass("java/security/cert/CertificateFactory"));
    if(jni_failed(env, certificateFactoryClass))
    {
        return false;
    }
    jmethodID certificateFactoryGetInstanceMethod = env->GetStaticMethodID(
        certificateFactoryClass.get(), 
        "getInstance", 
        "(Ljava/lang/String;)Ljava/security/cert/CertificateFactory;");
    if(jni_failed(env, certificateFactoryGetInstanceMethod))
    {
        return false;
    }
    jmethodID generateCertificateMethod = env->GetMethodID(
        certificateFactoryClass.get(), 
        "generateCertificate", 
        "(Ljava/io/InputStream;)Ljava/security/cert/Certificate;");
    if(jni_failed(env, generateCertificateMethod))
    {
        return false;
    }

    // X509Certificate
    java_local_ref<jclass> X509CertificateClass(env->FindClass("java/security/cert/X509Certificate"));
    if(jni_failed(env, X509CertificateClass))
    {
        return false;
    }

    // TrustManagerFactory
    java_local_ref<jclass> trustManagerFactoryClass(env->FindClass("javax/net/ssl/TrustManagerFactory"));
    if(jni_failed(env, trustManagerFactoryClass))
    {
        return false;
    }
    jmethodID trustManagerFactoryGetInstanceMethod = env->GetStaticMethodID(
        trustManagerFactoryClass.get(), 
        "getInstance", 
        "(Ljava/lang/String;)Ljavax/net/ssl/TrustManagerFactory;");
    if(jni_failed(env, trustManagerFactoryGetInstanceMethod))
    {
        return false;
    }
    jmethodID trustManagerFactoryInitMethod = env->GetMethodID(
        trustManagerFactoryClass.get(), 
        "init", 
        "(Ljava/security/KeyStore;)V");
    if(jni_failed(env, trustManagerFactoryInitMethod))
    {
        return false;
    }
    jmethodID trustManagerFactoryGetTrustManagersMethod = env->GetMethodID(
        trustManagerFactoryClass.get(), 
        "getTrustManagers", 
        "()[Ljavax/net/ssl/TrustManager;");
    if(jni_failed(env, trustManagerFactoryGetTrustManagersMethod))
    {
        return false;
    }

    // X509TrustManager
    java_local_ref<jclass> X509TrustManagerClass(env->FindClass("javax/net/ssl/X509TrustManager"));
    if(jni_failed(env, X509TrustManagerClass))
    {
        return false;
    }
    jmethodID X509TrustManagerCheckServerTrustedMethod = env->GetMethodID(
        X509TrustManagerClass.get(), 
        "checkServerTrusted", 
        "([Ljava/security/cert/X509Certificate;Ljava/lang/String;)V");
    if(jni_failed(env, X509TrustManagerCheckServerTrustedMethod))
    {
        return false;
    }

    // StrictHostnameVerifier
    java_local_ref<jclass> strictHostnameVerifierClass(env->FindClass("org/apache/http/conn/ssl/StrictHostnameVerifier"));
    if(jni_failed(env, strictHostnameVerifierClass))
    {
        return false;
    }
    jmethodID strictHostnameVerifierConstructorMethod = env->GetMethodID(strictHostnameVerifierClass.get(), "<init>", "()V");
    if(jni_failed(env, strictHostnameVerifierConstructorMethod))
    {
        return false;
    }
    jmethodID strictHostnameVerifierVerifyMethod = env->GetMethodID(
        strictHostnameVerifierClass.get(), 
        "verify", 
        "(Ljava/lang/String;Ljava/security/cert/X509Certificate;)V");
    if(jni_failed(env, strictHostnameVerifierVerifyMethod))
    {
        return false;
    }

    // Create CertificateFactory
    java_local_ref<jstring> XDot509String(env->NewStringUTF("X.509"));
    if(jni_failed(env, XDot509String))
    {
        return false;
    }
    java_local_ref<jobject> certificateFactory(env->CallStaticObjectMethod(
         certificateFactoryClass.get(), 
         certificateFactoryGetInstanceMethod, 
         XDot509String.get()));
    if(jni_failed(env, certificateFactory))
    {
        return false;
    }
    
    // Create Java array to store all the certs in.
    java_local_ref<jobjectArray> certsArray(env->NewObjectArray(certChain.size(), X509CertificateClass.get(), nullptr));
    if(jni_failed(env, certsArray))
    {
        return false;
    }

    // For each certificate perform the following steps:
    //   1. Create ByteArrayInputStream backed by DER certificate bytes 
    //   2. Create Certificate using CertificateFactory.generateCertificate
    //   3. Add Certificate to array
    int i = 0;
    for(const auto &certData : certChain)
    {
        java_local_ref<jbyteArray> byteArray(env->NewByteArray(certData.size()));
        if(jni_failed(env, byteArray))
        {
            return false;
        }
        env->SetByteArrayRegion(byteArray.get(), 0, certData.size(), reinterpret_cast<const jbyte *>(certData.c_str()));
        if(jni_failed(env))
        {
            return false;
        }
        java_local_ref<jobject> byteArrayInputStream(env->NewObject(
            byteArrayInputStreamClass.get(), 
            byteArrayInputStreamConstructorMethod, 
            byteArray.get()));
        if(jni_failed(env, byteArrayInputStream))
        {
            return false;
        }

        java_local_ref<jobject> cert(env->CallObjectMethod(
            certificateFactory.get(), 
            generateCertificateMethod, 
            byteArrayInputStream.get()));
        if(jni_failed(env, cert))
        {
            return false;
        }

        env->SetObjectArrayElement(certsArray.get(), i, cert.get());
        if(jni_failed(env))
        {
            return false;
        }
        ++i;
    }
    
    // Create TrustManagerFactory, init with Android system certs
    java_local_ref<jstring> X509String(env->NewStringUTF("X509"));
    if(jni_failed(env, X509String))
    {
        return false;
    }
    java_local_ref<jobject> trustFactoryManager(env->CallStaticObjectMethod(
        trustManagerFactoryClass.get(), 
        trustManagerFactoryGetInstanceMethod, 
        X509String.get()));
    if(jni_failed(env, trustFactoryManager))
    {
        return false;
    }
    env->CallVoidMethod(trustFactoryManager.get(), trustManagerFactoryInitMethod, nullptr);
    if(jni_failed(env)) 
    {
       return false;
    }

    // Get TrustManager
    java_local_ref<jobjectArray> trustManagerArray(static_cast<jobjectArray>(
        env->CallObjectMethod(trustFactoryManager.get(), trustManagerFactoryGetTrustManagersMethod)));
    if(jni_failed(env, trustManagerArray))
    {
        return false;
    }
    java_local_ref<jobject> trustManager(env->GetObjectArrayElement(trustManagerArray.get(), 0));
    if(jni_failed(env, trustManager))
    {
        return false;
    }
    
    // Validate certificate chain.
    java_local_ref<jstring> RSAString(env->NewStringUTF("RSA"));
    if(jni_failed(env, RSAString))
    {
        return false;
    }
    env->CallVoidMethod(
        trustManager.get(), 
        X509TrustManagerCheckServerTrustedMethod, 
        certsArray.get(), 
        RSAString.get());
    if(jni_failed(env))
    {
        return false;
    }

    // Verify hostname on certificate according to RFC 2818.
    java_local_ref<jobject> hostnameVerifier(env->NewObject(
        strictHostnameVerifierClass.get(), strictHostnameVerifierConstructorMethod));
    if(jni_failed(env, hostnameVerifier))
    {
        return false;
    }
    java_local_ref<jstring> hostNameString(env->NewStringUTF(hostName.c_str()));
    if(jni_failed(env, hostNameString))
    {
        return false;
    }
    java_local_ref<jobject> cert(env->GetObjectArrayElement(certsArray.get(), 0));
    if(jni_failed(env, cert))
    {
        return false;
    }
    env->CallVoidMethod(
        hostnameVerifier.get(), 
        strictHostnameVerifierVerifyMethod, 
        hostNameString.get(), 
        cert.get());
    if(jni_failed(env))
    {
        return false;
    }

    return true;
}
#endif

}}}}
