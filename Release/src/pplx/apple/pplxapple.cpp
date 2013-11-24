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
 * pplxapple.cpp
 *
 * Apple-specific pplx implementations
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/


#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <libkern/OSAtomic.h>
#include "stdafx.h"
#include "pplx/pplx.h"

// DEVNOTE: 
// The use of mutexes is suboptimal for synchronization of task execution.
// Given that scheduler implementations should use GCD queues, there are potentially better mechanisms available to coordinate tasks (such as dispatch groups).

namespace pplx
{

namespace details {
    
    namespace platform
    {
        _PPLXIMP long GetCurrentThreadId()
        {
            pthread_t threadId = pthread_self();
            return (long)threadId;
        }
        
        void YieldExecution()
        {
            sleep(0);
        }
        
    } // namespace platform
    
    void apple_scheduler::schedule( TaskProc_t proc, void* param)
    {
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_async_f(queue, param, proc);
    }
    
    apple_scheduler::~apple_scheduler()
    {
    }

    class apple_timer
    {
        struct callback_finisher
        {
            apple_timer * m_timer;
            callback_finisher(apple_timer *timer) : m_timer(timer)
            {
                m_timer->_in_callback = true;
            }
            ~callback_finisher()
            {
                m_timer->_in_callback = false;
            }
        };
    
    private:
        bool _repeat;
        dispatch_time_t _nextPopTime;
        TaskProc_t _proc;
        void * _context;
        std::atomic_int _callbacks_pending;
        std::atomic_bool _orphan;
        pplx::extensibility::event_t _completed;
        std::atomic_bool _stop_requested;
        std::atomic_bool _in_callback; // support reentrancy

        static void timer_callback(void * context)
        {
            apple_timer * ptimer = (apple_timer*)context;
            callback_finisher _(ptimer);
            ptimer->_callbacks_pending--;
            if(ptimer->_repeat)
            {
                if(ptimer->_stop_requested && ptimer->_callbacks_pending == 0) {
                    ptimer->finalize_callback();
                }
                else{
                    ptimer->_proc(ptimer->_context);
                    ptimer->dispatch_repeated_worker();
                }
            }
            else
            {
                ptimer->_proc(ptimer->_context);
                ptimer->finalize_callback();
            }
        }
    
        void finalize_callback()
        {
            if(_orphan)
            {
                delete this;
            }
            else
            {            
                _completed.set();
            }
        }
        
        void dispatch_repeated_worker()
        {
            _callbacks_pending++;
            dispatch_after_f(_nextPopTime, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), this, timer_callback);
        }
 
        void dispatch_repeated(int64_t intervalInNanoSeconds)
        {
            dispatch_time_t firstPopTime = dispatch_time(DISPATCH_TIME_NOW, intervalInNanoSeconds);
        
            _nextPopTime = dispatch_time(firstPopTime, intervalInNanoSeconds);
        
            dispatch_repeated_worker();
        }

    public:
        apple_timer(unsigned int ms, bool repeating, TaskProc_t proc, void * context) :
            _repeat(repeating)
            , _proc(proc)
            , _context(context)
            ,_callbacks_pending(0)
            ,_orphan(false)
            ,_stop_requested(false)
            ,_in_callback(false)
        {
            int64_t deltaNanoseconds = ms * NSEC_PER_MSEC;
            _completed.reset();
    
            if (repeating)
            {
                dispatch_repeated(deltaNanoseconds);
            }
            else
            {
                dispatch_time_t dispatchTime = dispatch_time(DISPATCH_TIME_NOW, deltaNanoseconds);
                dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
                _callbacks_pending++;
                dispatch_after_f(dispatchTime, queue, this, timer_callback);
            }
        }

        void stop(bool wait)
        {
            _stop_requested = true;
            if(_in_callback)
            {
                // this timer will clean up itself when it's done executing the callback
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
        m_timerImpl = new apple_timer(ms, repeating, proc, context); 
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
    
} // pplx