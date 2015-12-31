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
* Parallel Patterns Library - Linux version
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "pplx/pplx.h"
#include "pplx/threadpool.h"
#include "sys/syscall.h"

#ifdef _WIN32
#error "ERROR: This file should only be included in non-windows Build"
#endif

namespace pplx
{

namespace details {

    namespace platform
    {
        _PPLXIMP long GetCurrentThreadId()
        {
            return reinterpret_cast<long>(reinterpret_cast<void*>(pthread_self()));
        }

        _PPLXIMP void YieldExecution()
        {
            boost::this_thread::yield();
        }
    }

    _PPLXIMP void linux_scheduler::schedule(TaskProc_t proc, void* param)
    {
        crossplat::threadpool::shared_instance().schedule(boost::bind(proc, param));
    }

} // namespace details

} // namespace pplx
