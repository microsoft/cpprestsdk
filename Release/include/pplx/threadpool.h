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
* threadpool.h
*
* Simple Linux implementation of a static thread pool.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
***/
#pragma once

#include <pthread.h>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunreachable-code"
#include "boost/asio.hpp"
#pragma clang diagnostic pop

namespace crossplat {

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

    static threadpool& shared_instance()
    {
        return s_shared;
    }

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

    static threadpool s_shared;

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

    static void* thread_start(void *arg)
    {
        threadpool* _this = reinterpret_cast<threadpool*>(arg);
        try
        {
            _this->m_service.run();
        }
        catch (const _cancel_thread&)
        {
            // thread was cancelled
        }
        return arg;
    }

    std::vector<pthread_t> m_threads;
    boost::asio::io_service m_service;
    boost::asio::io_service::work m_work;
};

}
