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
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "pplx.h"

// Disable false alarm code analyze warning 
#pragma warning (disable : 26165 26110)
namespace pplx 
{

#if defined(__cplusplus_winrt)
namespace details {

volatile long s_asyncId = 0;

_PPLXIMP unsigned int __cdecl _GetNextAsyncId()
{
    return static_cast<unsigned int>(_InterlockedIncrement(&s_asyncId));
}
} // namespace details
#endif

static std::shared_ptr<pplx::scheduler> _M_Scheduler;
static pplx::details::_Spin_lock _M_SpinLock;

_PPLXIMP std::shared_ptr<pplx::scheduler> __cdecl get_ambient_scheduler()
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

_PPLXIMP void __cdecl set_ambient_scheduler(std::shared_ptr<pplx::scheduler> _Scheduler)
{
    ::pplx::details::_Scoped_spin_lock _Lock(_M_SpinLock);

    if (_M_Scheduler != nullptr)
    {
        throw invalid_operation("Scheduler is already initialized");
    }

    _M_Scheduler = _Scheduler;
}

} // namespace pplx
