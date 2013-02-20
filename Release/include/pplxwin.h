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
* Windows specific pplx implementations
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _PPLXWIN_H
#define _PPLXWIN_H

#include "pplxdefs.h"

#ifdef _MS_WINDOWS

#include "windows_compat.h"
#include "pplxatomics.h"
#include "pplxinterface.h"

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
};

namespace details
{
    /// <summary>
    /// Manual reset event
    /// </summary>
    class event_impl
    {
    public:

        static const unsigned int timeout_infinite = 0xFFFFFFFF;

        _PPLXIMP event_impl();

        _PPLXIMP ~event_impl();

        _PPLXIMP void set();

        _PPLXIMP void reset();

        _PPLXIMP unsigned int wait(unsigned int timeout);

        unsigned int wait()
        {
            return wait(event_impl::timeout_infinite);
        }

    private:
        // Windows events
        void * _M_impl;

        event_impl(const event_impl&);                  // no copy constructor
        event_impl const & operator=(const event_impl&); // no assignment operator
    };

    /// <summary>
    /// Mutex - lock for mutual exclusion
    /// </summary>
    class critical_section_impl
    {
    public:

        _PPLXIMP critical_section_impl();

        _PPLXIMP ~critical_section_impl();

        _PPLXIMP void lock();

        _PPLXIMP void unlock();

    private:

        typedef void * _PPLX_BUFFER;

        // Windows critical section
        _PPLX_BUFFER _M_impl[8];

        critical_section_impl(const critical_section_impl&);                  // no copy constructor
        critical_section_impl const & operator=(const critical_section_impl&); // no assignment operator
    };

    /// <summary>
    /// Reader writer lock
    /// </summary>
    class reader_writer_lock_impl
    {
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

        _PPLXIMP reader_writer_lock_impl();

        _PPLXIMP void lock();

        _PPLXIMP void lock_read();

        _PPLXIMP void unlock();

    private:

        // Windows slim reader writer lock
        void * _M_impl;

        // Slim reader writer lock doesn't have a general 'unlock' method.
        // We need to track how it was acquired and release accordingly.
        // true - lock exclusive
        // false - lock shared
        bool m_locked_exclusive;
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

        void recursive_lock_impl::lock()
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

        void recursive_lock_impl::unlock()
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
        pplx::details::critical_section_impl _M_cs;
        long _M_recursionCount;
        volatile long _M_owner;
    };

    class timer_impl
    {
    public:
        timer_impl()
            : m_timerImpl(nullptr)
        {
        }

        _PPLXIMP void start(unsigned int ms, bool repeat, TaskProc userFunc, _In_ void * context);
        _PPLXIMP void stop(bool waitForCallbacks);

        class _Timer_interface
        {
        public:
            virtual ~_Timer_interface()
            {
            }

            virtual void start(unsigned int ms, bool repeat) = 0;
            virtual void stop(bool waitForCallbacks) = 0;
        };

    private:
        _Timer_interface * m_timerImpl;
    };

    class windows_scheduler : public pplx::scheduler
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
typedef details::critical_section_impl critical_section;

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
typedef details::windows_scheduler default_scheduler_t;

/// <summary>
/// Terminate the process due to unhandled exception
/// </summary>
inline void report_unobserved_exception()
{
    __debugbreak();
    std::terminate();
}

} // namespace pplx

#endif // _MS_WINDOWS
#endif // _PPLXWIN_H