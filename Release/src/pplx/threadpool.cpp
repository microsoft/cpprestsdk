/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
**/
#include "stdafx.h"

#if !defined(CPPREST_EXCLUDE_WEBSOCKETS) || !defined(_WIN32)
#include "pplx/threadpool.h"

#include <boost/asio/detail/thread.hpp>
#include <new>
#include <vector>
#include <type_traits>

#if defined(__ANDROID__)
#include <android/log.h>
#include <jni.h>
#endif

namespace
{
#if defined(__ANDROID__)
// This pointer will be 0-initialized by default (at load time).
std::atomic<JavaVM*> JVM;

static void abort_if_no_jvm()
{
    if (JVM == nullptr)
    {
        __android_log_print(ANDROID_LOG_ERROR, "CPPRESTSDK", "%s",
            "The CppREST SDK must be initialized before first use on android: "
            "https://github.com/Microsoft/cpprestsdk/wiki/How-to-build-for-Android");
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
#endif // __ANDROID__

struct threadpool_impl final : crossplat::threadpool
{
    threadpool_impl(size_t n)
        : crossplat::threadpool(n)
        , m_work(m_service)
    {
        for (size_t i = 0; i < n; i++)
            add_thread();
    }

    ~threadpool_impl()
    {
        m_service.stop();
        for (auto iter = m_threads.begin(); iter != m_threads.end(); ++iter)
        {
            (*iter)->join();
        }
    }

    threadpool_impl& get_shared()
    {
        return *this;
    }

private:
    void add_thread()
    {
        m_threads.push_back(std::unique_ptr<boost::asio::detail::thread>(
            new boost::asio::detail::thread([&]{ thread_start(this); })));
    }

#if defined(__ANDROID__)
    static void detach_from_java(void*)
    {
        JVM.load()->DetachCurrentThread();
    }
#endif // __ANDROID__

    static void* thread_start(void *arg) CPPREST_NOEXCEPT
    {
#if defined(__ANDROID__)
        // Calling get_jvm_env() here forces the thread to be attached.
        get_jvm_env();
        pthread_cleanup_push(detach_from_java, nullptr);
#endif // __ANDROID__
        threadpool_impl* _this = reinterpret_cast<threadpool_impl*>(arg);
        _this->m_service.run();
#if defined(__ANDROID__)
        pthread_cleanup_pop(true);
#endif // __ANDROID__
        return arg;
    }

    std::vector<std::unique_ptr<boost::asio::detail::thread>> m_threads;
    boost::asio::io_service::work m_work;
};

#if defined(_WIN32)
struct shared_threadpool
{
    std::aligned_union<0, threadpool_impl>::type shared_storage;

    threadpool_impl& get_shared()
    {
        return reinterpret_cast<threadpool_impl&>(shared_storage);
    }

    shared_threadpool(size_t n)
    {
        ::new (static_cast<void*>(&get_shared())) threadpool_impl(n);
    }
    ~shared_threadpool()
    {
        // if linked into a DLL, the threadpool shared instance will be
        // destroyed at DLL_PROCESS_DETACH, at which stage joining threads
        // causes deadlock, hence this dance
        bool terminate_threads = boost::asio::detail::thread::terminate_threads();
        boost::asio::detail::thread::set_terminate_threads(true);
        get_shared().~threadpool_impl();
        boost::asio::detail::thread::set_terminate_threads(terminate_threads);
    }
};

typedef shared_threadpool platform_shared_threadpool;
#else // ^^^ _WIN32 ^^^ // vvv !_WIN32 vvv //
typedef threadpool_impl platform_shared_threadpool;
#endif

std::pair<bool, platform_shared_threadpool*> initialize_shared_threadpool(size_t num_threads)
{
    static std::aligned_union<0, platform_shared_threadpool>::type storage;
    platform_shared_threadpool* const ptr =
        &reinterpret_cast<platform_shared_threadpool&>(storage);
    bool initialized_this_time = false;
#if defined(__ANDROID__)
    // mutex based implementation due to paranoia about (lack of) call_once support on Android
    // remove this if/when call_once is supported
    static std::mutex mtx;
    static std::atomic<bool> initialized;
    abort_if_no_jvm();
    if (!initialized.load())
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (!initialized.load())
        {
            ::new (static_cast<void*>(ptr)) platform_shared_threadpool(num_threads);
            initialized.store(true);
            initialized_this_time = true;
        }
    }   // also unlock

#else // ^^^ __ANDROID__ ^^^ // vvv !__ANDROID___ vvv //
    static std::once_flag of;

// #if defined(__ANDROID__) // if call_once can be used for android
//     abort_if_no_jvm();
// #endif // __ANDROID__
    std::call_once(of, [num_threads, ptr, &initialized_this_time] {
        ::new (static_cast<void*>(ptr)) platform_shared_threadpool(num_threads);
        initialized_this_time = true;
    });
#endif // __ANDROID__

    return {initialized_this_time, ptr};
}
}

namespace crossplat
{
threadpool& threadpool::shared_instance()
{
    return initialize_shared_threadpool(40).second->get_shared();
}


void threadpool::initialize_with_threads(size_t num_threads)
{
    const auto result = initialize_shared_threadpool(num_threads);
    if (!result.first)
    {
        throw std::runtime_error("the cpprestsdk threadpool has already been initialized");
    }
}
}

#if defined(__ANDROID__)
void cpprest_init(JavaVM* vm) {
    JVM = vm;
}
#endif

std::unique_ptr<crossplat::threadpool> crossplat::threadpool::construct(size_t num_threads)
{
    return std::unique_ptr<crossplat::threadpool>(new threadpool_impl(num_threads));
}
#endif //  !defined(CPPREST_EXCLUDE_WEBSOCKETS) || !defined(_WIN32)
