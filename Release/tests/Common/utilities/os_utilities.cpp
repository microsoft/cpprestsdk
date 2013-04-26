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
* os_utilities.cpp - defines an abstraction for common OS functions like Sleep, hiding the underlying platform. 
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#include "os_utilities.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace tests { namespace common { namespace utilities {

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
    //
    // Timer implementation
    //
    class windows_timer : public timer_impl::_Timer_interface
    {
    public:
        windows_timer(::Concurrency::TaskProc_t userFunc, _In_ void * context)
            : m_userFunc(userFunc), m_userContext(context)
        {
        }

        virtual ~windows_timer()
        {
        }

        virtual void start(unsigned int ms, bool repeat)
        {
    #if defined(__cplusplus_winrt)
            auto timerHandler = ref new Windows::System::Threading::TimerElapsedHandler([this](Windows::System::Threading::ThreadPoolTimer ^)
            {
                this->m_userFunc(this->m_userContext);
            });

            Windows::Foundation::TimeSpan span;
            span.Duration = ms * 10000;
            if (repeat)
            {
                m_hTimer = Windows::System::Threading::ThreadPoolTimer::CreatePeriodicTimer(timerHandler, span);
            }
            else
            {
                m_hTimer = Windows::System::Threading::ThreadPoolTimer::CreateTimer(timerHandler, span);
            }
    #else
            if (!CreateTimerQueueTimer(&m_hTimer, NULL, _TimerCallback, this, ms, repeat ? ms : 0, WT_EXECUTEDEFAULT))
            {
                throw std::bad_alloc();
            }
    #endif
        }

        virtual void stop(bool waitForCallbacks)
        {
    #if defined(__cplusplus_winrt)
            if (m_hTimer != nullptr)
            {
                m_hTimer->Cancel();
                m_hTimer = nullptr;
            }
			waitForCallbacks;
    #else
            while (!DeleteTimerQueueTimer(NULL, m_hTimer, waitForCallbacks ? INVALID_HANDLE_VALUE : NULL))
            {
                if (GetLastError() == ERROR_IO_PENDING) 
                    break;
            }
    #endif

            delete this;
        }

    private:

        static void CALLBACK _TimerCallback(PVOID context, BOOLEAN)
        {
            auto timer = static_cast<windows_timer *>(context);
            timer->m_userFunc(timer->m_userContext);
        }

    #if defined(__cplusplus_winrt)
        Windows::System::Threading::ThreadPoolTimer ^ m_hTimer;
    #else
        HANDLE m_hTimer;
    #endif
        ::Concurrency::TaskProc_t m_userFunc;
        void * m_userContext;
    };

    void timer_impl::start(unsigned int ms, bool repeat, ::Concurrency::TaskProc_t userFunc, _In_ void * context)
    {
        _ASSERTE(m_timerImpl == nullptr);
        m_timerImpl = new windows_timer(userFunc, context);
        m_timerImpl->start(ms, repeat);
    }

    void timer_impl::stop(bool waitForCallbacks)
    {
        if (m_timerImpl != nullptr)
        {
            m_timerImpl->stop(waitForCallbacks);
            m_timerImpl = nullptr;
        }
    }
#endif

void os_utilities::sleep(unsigned long ms)
{
#ifdef WIN32
    Sleep(ms);
#else
    usleep(ms*1000);
#endif
}

unsigned long os_utilities::interlocked_increment(volatile unsigned long *addend)
{
#ifdef WIN32
    return InterlockedIncrement(addend);
#elif defined(__GNUC__)
    return __sync_add_and_fetch(addend, 1);
#else
#error Need to implement interlocked_increment
#endif
}

long os_utilities::interlocked_exchange(volatile long *target, long value)
{
#ifdef WIN32
    return InterlockedExchange(target, value);
#elif defined(__GNUC__)
    return __sync_lock_test_and_set(target, value);
#else
#error Need to implement interlocked_exchange
#endif
}

}}}
