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

} // namespace details

} // pplx
