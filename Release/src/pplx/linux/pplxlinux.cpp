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
* pplx.cpp
*
* Parallel Patterns Library - Linux version
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "pplx/pplx.h"
#include "pplx/threadpool.h"
#include "sys/syscall.h"

#ifdef _MS_WINDOWS
#error "ERROR: This file should only be included in non-windows Build"
#endif

namespace pplx 
{ 

namespace details {

    namespace platform
    {
        _PPLXIMP long GetCurrentThreadId()
        {
            return (long)syscall(SYS_gettid) * 4;
        }

        _PPLXIMP void YieldExecution()
        {
            boost::this_thread::yield();
        }
    }

    _PPLXIMP void linux_scheduler::schedule( TaskProc_t proc, void* param)
    {
        crossplat::threadpool::shared_instance().schedule(boost::bind(proc, param));
    }

    class linux_timer
    {
        struct callback_finisher
        {
            linux_timer * m_timer;
            callback_finisher(linux_timer *timer) : m_timer(timer)
            {
                m_timer->_in_callback = true;
            }
            ~callback_finisher()
            {
                m_timer->_in_callback = false;
                if( m_timer->_orphan )
                {
                    delete m_timer;
                }
            }
        };
 
    private:
        unsigned int _ms;
        bool _repeat;
        boost::asio::deadline_timer _timer;
        TaskProc_t _proc;
        void * _ctx;
        std::atomic_bool _in_callback;
        std::atomic_bool _orphan;
        pplx::extensibility::event_t _completed;

        void callback(const boost::system::error_code& ec)
        {
            callback_finisher _(this);
            if (ec == boost::asio::error::operation_aborted)
            {
                _completed.set();
                return;
            }
 
            _proc(_ctx);

            if (_repeat)
            {
                _timer.expires_at(_timer.expires_at() + boost::posix_time::milliseconds(_ms));
                _timer.async_wait(boost::bind(&linux_timer::callback, this, boost::asio::placeholders::error));
            }
            else
            {
                _completed.set();
            }
        }

    public:
        linux_timer(unsigned int ms, bool repeating, TaskProc_t proc, void * context)
            : _ms(ms)
            , _repeat(repeating)
            , _timer(crossplat::threadpool::shared_instance().service(), boost::posix_time::milliseconds(ms))
            , _proc(proc)
            , _ctx(context)
            ,_in_callback(false)
            ,_orphan(false)
        {
            _timer.async_wait(boost::bind(&linux_timer::callback, this, boost::asio::placeholders::error));
        }

        void stop(bool wait)
        {
            _timer.cancel();
            if(_in_callback)
            {
                // If we're called re-entrantly, or a callback is in progress on another thread,
                // abandon the timer and let it delete itself at the end of the callback
                _orphan = true;
            }
            else
            {
                if (wait || !_repeat)
                {
                    _completed.wait();
                }
                delete this;
            }
        }
    };

    _PPLXIMP void timer_impl::start(unsigned int ms, bool repeating, TaskProc_t proc, void * context)
    {
        _ASSERTE(m_timerImpl == nullptr);
        m_timerImpl = new linux_timer(ms, repeating, proc, context); 
    }

    _PPLXIMP void timer_impl::stop(bool waitForCallbacks)
    {
        if( m_timerImpl != nullptr )
        {
            m_timerImpl->stop(waitForCallbacks);
            m_timerImpl = nullptr;
        }
    }
    
    timer_impl::~timer_impl()
    {
    }

} // namespace details

} // namespace pplx
