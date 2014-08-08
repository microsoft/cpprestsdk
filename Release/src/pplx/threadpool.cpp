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
**/
#include "stdafx.h"

namespace crossplat
{
// initialize the static shared threadpool 
threadpool threadpool::s_shared(40);

#if defined(ANDROID)
// This pointer will be 0-initialized by default (at load time).
std::atomic<JavaVM*> JVM;
static thread_local JNIEnv* JVM_ENV = nullptr;

JNIEnv* get_jvm_env()
{
    if (JVM == nullptr)
    {
        throw std::runtime_error("Could not contact JVM");
    }
    if (JVM_ENV == nullptr)
    {
        auto result = JVM.load()->AttachCurrentThread(&crossplat::JVM_ENV, nullptr);
        if (result != JNI_OK)
        {
            throw std::runtime_error("Could not attach to JVM");
        }
    }

    return JVM_ENV;
}

#endif

}

#if defined(ANDROID)
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
   JNIEnv* env;
   if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
   {
      return -1;
   }

   crossplat::JVM = vm;
   return JNI_VERSION_1_6;
}
#endif
