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
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Pre-compiled headers
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#if defined(_WIN32)
// Include first to avoid any issues with Windows.h.
#define NOMINMAX
#include <winsock2.h>
#endif

#if defined(_WIN32)
// Trick Boost.Asio into thinking CE, otherwise _beginthreadex will be used which is banned
// for the Windows Runtime pre VS2015. Then CreateThread will be used instead.
#if _MSC_VER < 1900
#if defined(__cplusplus_winrt)
#define UNDER_CE 1
#endif
#endif
#endif

#include "cpprest/uri.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/streams.h"
#include "cpprest/containerstream.h"

#include "unittestpp.h"

