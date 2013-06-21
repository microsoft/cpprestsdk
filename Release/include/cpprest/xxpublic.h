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
* xxpublic.h
*
* Standard macros and definitions.
* This header has minimal dependency on windows headers and is safe for use in the public API
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _MS_WINDOWS
#if defined(WIN32) || defined(__cplusplus_winrt)
#define _MS_WINDOWS
#endif
#endif // _MS_WINDOWS

#ifdef _ASYNCRT_EXPORT
#define _ASYNCRTIMP __declspec(dllexport)
#else
#define _ASYNCRTIMP __declspec(dllimport)
#endif

// forward declarations of windows types

// for UINT_PTR types used in winsock APIs
#ifdef _MS_WINDOWS
#include <BaseTsd.h>
typedef UINT_PTR SOCKET;
#endif

// for guids, used in comm.h
#ifdef _MS_WINDOWS
#include <Guiddef.h>
#include <compat/windows_compat.h>
#else
#include "boost/uuid/uuid.hpp"
#endif

// for winhttp.h
typedef void * HINTERNET;

// for file io (windows.h)
typedef void * HANDLE;

#ifdef _MS_WINDOWS
typedef void* FILE_HANDLE;
#else
typedef FILE* FILE_HANDLE;
#endif

// for http client and others that use overlapped IO delegates (windef.h)
#define CALLBACK __stdcall
