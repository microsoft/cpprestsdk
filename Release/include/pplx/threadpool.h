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
*
* Simple Linux implementation of a static thread pool.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
***/
#pragma once

#include <pthread.h>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Winfinite-recursion"
#endif
#include "boost/asio.hpp"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if (defined(ANDROID) || defined(__ANDROID__))
#include <atomic>
#include <jni.h>
#include "pplx/pplx.h"
#endif

namespace crossplat {

#if (defined(ANDROID) || defined(__ANDROID__))
// IDEA: Break this section into a separate android/jni header
extern std::atomic<JavaVM*> JVM;
JNIEnv* get_jvm_env();

struct java_local_ref_deleter
{
    void operator()(jobject lref) const
    {
        crossplat::get_jvm_env()->DeleteLocalRef(lref);
    }
};

template<class T>
using java_local_ref = std::unique_ptr<typename std::remove_pointer<T>::type, java_local_ref_deleter>;
#endif

class threadpool
{
public:

    threadpool(size_t n)
      : m_service(n),
        m_work(m_service)
    {
        for (size_t i = 0; i < n; i++)
            add_thread();
    }

    static threadpool& shared_instance();

    ~threadpool()
    {
        m_service.stop();
        for (auto iter = m_threads.begin(); iter != m_threads.end(); ++iter)
        {
            pthread_t t = *iter;
            void* res;
            pthread_join(t, &res);
        }
    }

    template<typename T>
    void schedule(T task)
    {
        m_service.post(task);
    }

    boost::asio::io_service& service()
    {
        return m_service;
    }

private:
    struct _cancel_thread { };

    void add_thread()
    {
        pthread_t t;
        auto result = pthread_create(&t, nullptr, &thread_start, this);
        if (result == 0)
            m_threads.push_back(t);
    }

    void remove_thread()
    {
        schedule([]() -> void { throw _cancel_thread(); });
    }

#if (defined(ANDROID) || defined(__ANDROID__))
    static void detach_from_java(void*)
    {
        JVM.load()->DetachCurrentThread();
    }
#endif

    static void* thread_start(void *arg)
    {
#if (defined(ANDROID) || defined(__ANDROID__))
        // Calling get_jvm_env() here forces the thread to be attached.
        get_jvm_env();
        pthread_cleanup_push(detach_from_java, nullptr);
#endif
        threadpool* _this = reinterpret_cast<threadpool*>(arg);
        try
        {
            _this->m_service.run();
        }
        catch (const _cancel_thread&)
        {
            // thread was cancelled
        }
        catch (...)
        {
            // Something bad happened
#if (defined(ANDROID) || defined(__ANDROID__))
            // Reach into the depths of the 'droid!
            // NOTE: Uses internals of the bionic library
            // Written against android ndk r9d, 7/26/2014
            __pthread_cleanup_pop(&__cleanup, true);
            throw;
#endif
        }
#if (defined(ANDROID) || defined(__ANDROID__))
        pthread_cleanup_pop(true);
#endif
        return arg;
    }

    std::vector<pthread_t> m_threads;
    boost::asio::io_service m_service;
    boost::asio::io_service::work m_work;
};

}
