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
* pplxwin.h
*
* Linux specific pplx implementations
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _PPLXLINUX_H
#define _PPLXLINUX_H

#include "pplxdefs.h"

#ifndef _MS_WINDOWS

#include "linux_compat.h"
#include "pplxatomics.h"
#include "pplxinterface.h"

#include <signal.h>
#include <mutex>
#include <condition_variable>

#include "pthread.h"
#include "boost/thread/mutex.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/bind/bind.hpp"
namespace pplx
{

namespace platform
{
    /// <summary>
    /// Returns a unique identifier for the execution thread where this routine in invoked
    /// </summary>
    _PPLXIMP long __cdecl GetCurrentThreadId();

    /// <summary>
    /// Yields the execution of the current execution thread - typically when spin-waiting
    /// </summary>
    _PPLXIMP void __cdecl YieldExecution();
}

namespace details
{
    /// <summary>
    /// Manual reset event
    /// </summary>
    class event_impl
     {
    private:
        std::mutex _lock;
        std::condition_variable _condition;
        bool _signaled;
    public:

        static const unsigned int timeout_infinite = 0xFFFFFFFF;

        event_impl()
            : _signaled(false) 
        {
        }

        void set()
        {
            {
                std::lock_guard<std::mutex> lock(_lock);
                _signaled = true;
            }
            _condition.notify_all();
        }

        void reset()
        {
            std::lock_guard<std::mutex> lock(_lock);
            _signaled = false;
        }

        unsigned int wait(unsigned int timeout)
        {
            std::unique_lock<std::mutex> lock(_lock);
            if (timeout == event_impl::timeout_infinite)
            {
                _condition.wait(lock, [this]() -> bool { return _signaled; });
                return 0;
            }
            else
            {
                std::chrono::milliseconds period(timeout);
                auto status = _condition.wait_for(lock, period, [this]() -> bool { return _signaled; });
                _PPLX_ASSERT(status == _signaled);
                // Return 0 if the wait completed as a result of signaling the event. Otherwise, return timeout_infinite
                // Note: this must be consistent with the behavior of the Windows version, which is based on WaitForSingleObjectEx
                return status ? 0: event_impl::timeout_infinite;
            }
        }

        unsigned int wait()
        {
            return wait(event_impl::timeout_infinite);
        }
    };

    /// <summary>
    /// Reader writer lock
    /// </summary>
    class reader_writer_lock_impl
    {
    private:

        pthread_rwlock_t _M_reader_writer_lock;

    public:

        class scoped_lock_read
        {
        public:
            explicit scoped_lock_read(reader_writer_lock_impl &_Reader_writer_lock) : _M_reader_writer_lock(_Reader_writer_lock)
            {
                _M_reader_writer_lock.lock_read();
            }

            ~scoped_lock_read()
            {
                _M_reader_writer_lock.unlock();
            }

        private:
            reader_writer_lock_impl& _M_reader_writer_lock;
            scoped_lock_read(const scoped_lock_read&);                    // no copy constructor
            scoped_lock_read const & operator=(const scoped_lock_read&);  // no assignment operator
        };

        reader_writer_lock_impl()
        {
            pthread_rwlock_init(&_M_reader_writer_lock, nullptr);
        }

        ~reader_writer_lock_impl()
        {
            pthread_rwlock_destroy(&_M_reader_writer_lock);
        }

        void lock()
        {
            pthread_rwlock_wrlock(&_M_reader_writer_lock);
        }

        void lock_read()
        {
            pthread_rwlock_rdlock(&_M_reader_writer_lock);
        }

        void unlock()
        {
            pthread_rwlock_unlock(&_M_reader_writer_lock);
        }
    };

    /// <summary>
    /// Recursive mutex
    /// </summary>
    class recursive_lock_impl
    {
    public:

        recursive_lock_impl()
            : _M_owner(-1), _M_recursionCount(0)
        {
        }

        ~recursive_lock_impl()
        {
            _PPLX_ASSERT(_M_owner == -1);
            _PPLX_ASSERT(_M_recursionCount == 0);
        }

        void lock()
        {
            auto id = ::pplx::platform::GetCurrentThreadId();

            if ( _M_owner == id )
            {
                _M_recursionCount++;
            }
            else
            {
                _M_cs.lock();
                _M_owner = id;
                _M_recursionCount = 1;
            }            
        }

        void unlock()
        {
            _PPLX_ASSERT(_M_owner == ::pplx::platform::GetCurrentThreadId());
            _PPLX_ASSERT(_M_recursionCount >= 1);

            _M_recursionCount--;

            if ( _M_recursionCount == 0 )
            {
                _M_owner = -1;
                _M_cs.unlock();
            }           
        }

    private:
        boost::mutex _M_cs;
        long _M_recursionCount;
        volatile long _M_owner;
    };

    class linux_timer;

    class timer_impl
    {
    public:
        timer_impl() : m_timerImpl(nullptr)
        {
        }

        ~timer_impl();

        _PPLXIMP void start(unsigned int ms, bool repeat, TaskProc userFunc, _In_ void * context);
        _PPLXIMP void stop(bool waitForCallbacks);

    private:
        linux_timer * m_timerImpl;
    };

    class linux_scheduler : public pplx::scheduler
    {
    public:
        _PPLXIMP virtual void schedule( TaskProc proc, _In_ void* param);
    };

} // namespace details

/// <summary>
/// Timer
/// </summary>
typedef details::timer_impl timer_t;

/// <summary>
/// Events
/// </summary>
typedef details::event_impl notification_event;

/// <summary>
/// Reader writer lock
/// </summary>
typedef details::reader_writer_lock_impl reader_writer_lock;

/// <summary>
/// std::mutex
/// </summary>
typedef boost::mutex critical_section;

/// <summary>
/// std::recursive_mutex
/// </summary>
typedef details::recursive_lock_impl recursive_lock;

/// <summary>
///  A generic RAII wrapper for locks that implement the critical_section interface
///  std::lock_guard
/// </summary>
template<class _Lock>
class scoped_lock
{
public:
    explicit scoped_lock(_Lock& _Critical_section) : _M_critical_section(_Critical_section)
    {
        _M_critical_section.lock();
    }

    ~scoped_lock()
    {
        _M_critical_section.unlock();
    }

private:
    _Lock& _M_critical_section;

    scoped_lock(const scoped_lock&);                    // no copy constructor
    scoped_lock const & operator=(const scoped_lock&);  // no assignment operator
};

typedef scoped_lock<pplx::critical_section> scoped_critical_section;
typedef scoped_lock<pplx::recursive_lock> scoped_recursive_lock;
typedef scoped_lock<pplx::reader_writer_lock> scoped_rw_lock;
typedef pplx::reader_writer_lock::scoped_lock_read scoped_read_lock;

/// <summary>
/// Default scheduler type
/// </summary>
typedef details::linux_scheduler default_scheduler_t;

/// <summary>
/// Terminate the process due to unhandled exception
/// </summary>
inline void report_unobserved_exception()
{
    raise(SIGTRAP);
    std::terminate();
}

} // namespace pplx

#endif // !_MS_WINDOWS
#endif // _PPLXLINUX_H