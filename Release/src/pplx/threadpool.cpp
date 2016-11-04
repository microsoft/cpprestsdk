/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
**/
#include "stdafx.h"

#if defined(__ANDROID__)
#include <android/log.h>
#include <jni.h>
#endif

namespace crossplat
{
#if (defined(ANDROID) || defined(__ANDROID__))
// This pointer will be 0-initialized by default (at load time).
std::atomic<JavaVM*> JVM;

static void abort_if_no_jvm()
{
    if (JVM == nullptr)
    {
        __android_log_print(ANDROID_LOG_ERROR, "CPPRESTSDK", "%s", "The CppREST SDK must be initialized before first use on android: https://github.com/Microsoft/cpprestsdk/wiki/How-to-build-for-Android");
        std::abort();
    }
}

JNIEnv* get_jvm_env()
{
    abort_if_no_jvm();
    JNIEnv* env = nullptr;
    auto result = JVM.load()->AttachCurrentThread(&env, nullptr);
    if (result != JNI_OK)
    {
        throw std::runtime_error("Could not attach to JVM");
    }

    return env;
}

threadpool& threadpool::shared_instance()
{
    abort_if_no_jvm();
    static threadpool s_shared(40);
    return s_shared;
}

#else

// initialize the static shared threadpool
threadpool& threadpool::shared_instance()
{
    static threadpool s_shared(40);
    return s_shared;
}

#endif

}

#if defined(__ANDROID__)
void cpprest_init(JavaVM* vm) {
    crossplat::JVM = vm;
}
#endif

