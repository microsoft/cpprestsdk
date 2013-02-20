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
