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
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
***/
#pragma once

#include "boost/asio.hpp"
#include "boost/thread.hpp"
#include "boost/bind.hpp"

namespace crossplat {

class threadpool
{
private:
    boost::thread_group _tg;
    boost::asio::io_service _service;
    boost::asio::io_service::work _work;

    static threadpool _shared;

struct goodbye_thread{};

void thread_start()
{
    try
    {
        _service.run();
    }
    catch (const goodbye_thread&)
    {
        // thread was cancelled
    }
}

public:
threadpool(size_t n)
    : _tg()
    , _service(n)
    , _work(_service)
{
    for (size_t i = 0; i < n; i++)
        add_thread();
}

static threadpool& shared_instance()
{
    return _shared;
}

~threadpool()
{
    _service.stop();
    _tg.join_all();
}

template<typename T>
void schedule(T task)
{
    _service.post(task);
}

void add_thread()
{
    _tg.create_thread(boost::bind(&threadpool::thread_start, this));
}

void remove_thread()
{
    schedule([]() -> void { throw goodbye_thread(); });
}

boost::asio::io_service& service()
{
    return _service;
}
};

}

namespace Concurrency
{
    struct Context
    {
        static void Oversubscribe(bool on)
        {
            if (on)
            {
                crossplat::threadpool::shared_instance().add_thread();
            }
            else
            {
                crossplat::threadpool::shared_instance().remove_thread();
            }
        }
    };
}
