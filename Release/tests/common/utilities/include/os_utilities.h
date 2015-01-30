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
#include "cpprest/details/cpprest_compat.h"

namespace tests { namespace common { namespace utilities {

class os_utilities
{
public:

    static TEST_UTILITY_API void __cdecl sleep(unsigned long ms);

    // Could use std::atomics but VS 2010 doesn't support it yet.
    static TEST_UTILITY_API unsigned long __cdecl interlocked_increment(volatile unsigned long *addend);
    static TEST_UTILITY_API long __cdecl interlocked_exchange(volatile long *target, long value);

private:
    os_utilities();
    os_utilities(const os_utilities &);
    os_utilities & operator=(const os_utilities &);
};

}}}

