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
* ioscheduler.cpp
*
* Implemenation of I/O Scheduler
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "pplx/ioscheduler.h"
#include "cpprest/globals.h"

namespace Concurrency { namespace streams { namespace details {

#if _WIN32_WINNT >= _WIN32_WINNT_VISTA 
io_scheduler::~io_scheduler()
{
    if (g_isProcessTerminating != 1)
    {
        // Cancel pending callbacks and waits for the ones currently executing
        CloseThreadpoolCleanupGroupMembers(m_cleanupGroup, TRUE, NULL);

        // Release resources
        CloseThreadpoolCleanupGroup(m_cleanupGroup);
        DestroyThreadpoolEnvironment(&m_environ);
    }
}
#endif // _WIN32_WINNT >= _WIN32_WINNT_VISTA 

/// <summary>
/// We keep a single instance of the I/O scheduler. In order to create it on first
/// demand, it's referenced through a shared_ptr<T> and retrieved through function call.
/// </summary>
pplx::extensibility::critical_section_t _g_lock;
std::shared_ptr<io_scheduler> _g_scheduler;

/// <summary>
/// Get the I/O scheduler instance.
/// </summary>
std::shared_ptr<io_scheduler> __cdecl io_scheduler::get_scheduler()
{
    pplx::extensibility::scoped_critical_section_t lck(_g_lock);
    if ( !_g_scheduler )
    {
        _g_scheduler = std::shared_ptr<io_scheduler>(new io_scheduler());
    }

    return _g_scheduler;
}

}}} // namespace