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
* os_utilities.h - defines an abstraction for common OS functions like Sleep, hiding the underlying platform. 
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "common_utilities_public.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;

namespace tests { namespace common { namespace utilities {

#if defined(_MSC_VER) && (_MSC_VER >= 1800)

    class timer_impl
    {
    public:
        timer_impl()
            : m_timerImpl(nullptr)
        {
        }

        TEST_UTILITY_API void start(unsigned int ms, bool repeat, ::Concurrency::TaskProc_t userFunc, _In_ void * context);
        TEST_UTILITY_API void stop(bool waitForCallbacks);

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

    typedef timer_impl timer_t;
#endif
}}}

namespace Concurrency
{
    namespace details
    {
        using tests::common::utilities::timer_t;
    }
}

#else 
#include "pplx/pplxtasks.h"
#endif

#if !defined(_MS_WINDOWS) || (_MSC_VER >= 1700)
#include <chrono>
#endif

namespace tests { namespace common { namespace utilities {

class os_utilities
{
public:

    static TEST_UTILITY_API void sleep(unsigned long ms);

    // Could use std::atomics but VS 2010 doesn't support it yet.
    static TEST_UTILITY_API unsigned long interlocked_increment(volatile unsigned long *addend);
    static TEST_UTILITY_API long interlocked_exchange(volatile long *target, long value);

private:
    os_utilities();
    os_utilities(const os_utilities &);
    os_utilities & operator=(const os_utilities &);
};

namespace details
{
    /// <summary>
    /// A convenient extension to PPLX: loop until a condition is no longer met
    /// </summary>
    /// <param name="func">
    ///   A function representing the body of the loop. It will be invoked at least once and 
    ///   then repetitively as long as it returns true.
    /// </param>
    inline pplx::task<bool> do_while(std::function<pplx::task<bool>(void)> func)
    {
        pplx::task<bool> first = func();
        return first.then([=](bool guard) -> pplx::task<bool> {
            if (guard)
                return do_while(func);
            else
                return first;
            });
    }

    // Helper class for delayed tasks
    class _Delayed_task_helper
    {
    public:

        ~_Delayed_task_helper()
        {
            _M_timer.stop(false);
        }

        static pplx::task<void> _Create(unsigned int _Delay)
        {
            auto _Helper = new _Delayed_task_helper;

            // Create the task before starting the timer as the callback will delete the helper
            auto _Task = pplx::create_task(_Helper->_M_tce);
            _Helper->_M_timer.start(_Delay, false, _Timer_callback, (void *)(_Helper));
            return _Task;
        }
    
    private:
        _Delayed_task_helper() {}

        static inline void __cdecl _Timer_callback(void * _Context)
        {
            auto _Helper = static_cast<_Delayed_task_helper *>(_Context);
            _Helper->_M_tce.set();
            delete _Helper;
        }

        pplx::task_completion_event<void> _M_tce;     
        pplx::details::timer_t _M_timer;
    };
} // namespace details

/// <summary> 
///     Creates a task from given user Functor, which will be scheduled after <paramref name="delay"/> million senconds. 
/// </summary> 
/// <typeparam name="Func"> 
///     The type of the user task Functor. 
/// </typeparam> 
/// <param name="delay"> 
///     The delay before starting the task. 
/// </param> 
/// <param name="func"> 
///     The user task Functor. 
/// </param> 
/// <param name="token"> 
///     The optional cancellation token, which will be passed to the return task. 
/// </param> 
/// <returns> 
///     It will return a task constructed with user Functor <paramref name="func"/>, and cancellation token <paramref name="token"/>. 
///     The returning task will be scheduled after <paramref name="delay"/> million seconds. 
/// </returns> 
/// <seealso cref="Task Parallelism (Concurrency Runtime)"/> 
/**/
#if  !defined(_MS_WINDOWS) || (_MSC_VER >= 1700)

// Post VS10 and Linux
template <typename Func> 
auto create_delayed_task(std::chrono::milliseconds delay, Func func, ::pplx::cancellation_token token = ::pplx::cancellation_token::none()) -> decltype(::pplx::create_task(func)) 
{ 
    return details::_Delayed_task_helper::_Create(static_cast<unsigned int>(delay.count())).then(func, token);
}

#endif // !defined(_MS_WINDOWS) || (_MSC_VER >= 1700)

// VS 2010
template <typename Func> 
auto create_delayed_task(unsigned int delay, Func func, ::pplx::cancellation_token token = ::pplx::cancellation_token::none()) -> decltype(::pplx::create_task(func)) 
{ 
    return details::_Delayed_task_helper::_Create(delay).then(func, token);
}

}}}

