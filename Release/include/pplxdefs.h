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
* pplxdefs.h
*
* Parallel Patterns Library
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _PPLXDEFS_H
#define _PPLXDEFS_H

#ifndef _MS_WINDOWS
#if defined(WIN32) || defined(__cplusplus_winrt)
#define _MS_WINDOWS
#endif
#endif // _MS_WINDOWS

#if defined(_DEBUG)
#define _PPLX_ASSERT(x)   do {_ASSERTE(x); __assume(x);} while(false)
#else
#define _PPLX_ASSERT(x)   __assume(x)
#endif

#pragma warning(disable :4127)

#ifdef _PPLX_EXPORT
#define _PPLXIMP __declspec(dllexport)
#else
#define _PPLXIMP __declspec(dllimport)
#endif

#endif // _PPLXDEFS_H
