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
* Parallel Patterns Library implementation (common code across platforms)
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#error This file must not be compiled for Visual Studio 12 or later
#endif

#include "pplx/pplx.h"

// Disable false alarm code analyze warning 
#pragma warning (disable : 26165 26110)
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

        _Spin_lock()
            : _M_lock(0)
        {
        }

        void lock()
        {
            if ( details::atomic_compare_exchange(_M_lock, 1l, 0l) != 0l )
            {
                do 
                {
                    pplx::details::platform::YieldExecution();

                } while ( details::atomic_compare_exchange(_M_lock, 1l, 0l) != 0l );
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


static std::shared_ptr<pplx::scheduler_interface> _M_Scheduler;
static pplx::details::_Spin_lock _M_SpinLock;

_PPLXIMP std::shared_ptr<pplx::scheduler_interface> __cdecl get_ambient_scheduler()
{
    if ( !_M_Scheduler)
    {
        ::pplx::details::_Scoped_spin_lock _Lock(_M_SpinLock);
        if (!_M_Scheduler)
        {
            _M_Scheduler = std::make_shared< ::pplx::default_scheduler_t>();
        }
    }

    return _M_Scheduler;
}

_PPLXIMP void __cdecl set_ambient_scheduler(std::shared_ptr<pplx::scheduler_interface> _Scheduler)
{
    ::pplx::details::_Scoped_spin_lock _Lock(_M_SpinLock);

    if (_M_Scheduler != nullptr)
    {
        throw invalid_operation("Scheduler is already initialized");
    }

    _M_Scheduler = _Scheduler;
}

} // namespace pplx
