/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Parallel Patterns Library implementation (common code across platforms)
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#include "stdafx.h"

#if !defined(_WIN32) || CPPREST_FORCE_PPLX

#include <atomic>
#include "pplx/pplx.h"

// Disable false alarm code analyze warning
#if defined(_MSC_VER)
#pragma warning(disable : 26165 26110)
#endif

namespace pplx
{
namespace details
{
/// <summary>
/// Spin lock to allow for locks to be used in global scope
/// </summary>
class _Spin_lock
{
public:
    _Spin_lock() : _M_lock(0) {}

    void lock()
    {
        if (details::atomic_compare_exchange(_M_lock, 1l, 0l) != 0l)
        {
            do
            {
                pplx::details::platform::YieldExecution();

            } while (details::atomic_compare_exchange(_M_lock, 1l, 0l) != 0l);
        }
    }

    void unlock()
    {
        // fence for release semantics
        details::atomic_exchange(_M_lock, 0l);
    }

private:
    atomic_long _M_lock;
};

typedef ::pplx::scoped_lock<_Spin_lock> _Scoped_spin_lock;
} // namespace details

static struct _pplx_g_sched_t
{
    typedef std::shared_ptr<pplx::scheduler_interface> sched_ptr;

    sched_ptr get_scheduler()
    {        
        sched_ptr sptr = std::atomic_load_explicit(&m_scheduler, std::memory_order_consume);
        if (!sptr)
        {
            ::pplx::details::_Scoped_spin_lock lock(m_spinlock);
            sptr = std::atomic_load_explicit(&m_scheduler, std::memory_order_relaxed);
            if (!sptr)
            {
                sptr = std::make_shared<::pplx::default_scheduler_t>();
                std::atomic_store_explicit(&m_scheduler, sptr, std::memory_order_release);
            }
        }

        return m_scheduler;
    }

    void set_scheduler(sched_ptr scheduler)
    {
        if (std::atomic_load_explicit(&m_scheduler, std::memory_order_consume) != nullptr)
        {
            throw invalid_operation("Scheduler is already initialized");
        }

        ::pplx::details::_Scoped_spin_lock lock(m_spinlock);
        std::atomic_store_explicit(&m_scheduler, scheduler, std::memory_order_release);
    }

private:
    pplx::details::_Spin_lock m_spinlock;
    sched_ptr m_scheduler;
} _pplx_g_sched;

_PPLXIMP std::shared_ptr<pplx::scheduler_interface> _pplx_cdecl get_ambient_scheduler()
{
    return _pplx_g_sched.get_scheduler();
}

_PPLXIMP void _pplx_cdecl set_ambient_scheduler(std::shared_ptr<pplx::scheduler_interface> _Scheduler)
{
    _pplx_g_sched.set_scheduler(std::move(_Scheduler));
}

} // namespace pplx

#endif
