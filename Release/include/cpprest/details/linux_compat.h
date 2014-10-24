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
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
***/

#pragma once
#include <cstdint>
#include <sstream>
#include <iostream>
#define __cdecl __attribute__ ((cdecl))

#include "cpprest/details/nosal.h"

#define CPPREST_NOEXCEPT noexcept

#define novtable /* no novtable equivalent */
#define __declspec(x) __attribute__ ((x))

// ignore these:
#define dllimport 
#ifdef __LP64__ // ignore cdecl on 64-bit
#define cdecl
#endif

#include <stdint.h>
#include <assert.h>

#define __assume(x) do { if (!(x)) __builtin_unreachable(); } while (false)

#define CASABLANCA_UNREFERENCED_PARAMETER(x) (void)x
#define _ASSERTE(x) assert(x)

#ifdef CASABLANCA_DEPRECATION_NO_WARNINGS
#define CASABLANCA_DEPRECATED(x)
#else
#define CASABLANCA_DEPRECATED(x) __attribute__((deprecated(x)))
#endif

