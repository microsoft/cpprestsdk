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
* stdafx.h
*
* Pre-compiled headers
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#ifndef __STDAFX_H

#include "cpprest/xxpublic.h"
#include "cpprest/basic_types.h"

#ifdef _MS_WINDOWS
#include "cpprest/targetver.h"
#include "compat/windows_compat.h"
// use the debug version of the CRT if _DEBUG is defined
#ifdef _DEBUG
    #define _CRTDBG_MAP_ALLOC
    #include <stdlib.h>
    #include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#define NOMINMAX
#include <windows.h>
#include <objbase.h>

// Windows Header Files:
#if !defined(__cplusplus_winrt)
#include <winhttp.h>

#endif // #if !defined(__cplusplus_winrt)
#else // LINUX
#include <compat/linux_compat.h>
#define FAILED(x) ((x) != 0)
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <signal.h>
#include "pthread.h"
#include "boost/thread/mutex.hpp"
#include "boost/locale.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/bind/bind.hpp"
#include <pplx/threadpool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#endif // _MS_WINDOWS

#include <iostream>
#include <fstream>
#include <algorithm>
#include <exception>
#include <assert.h>
#include <streambuf>

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else 
#include "pplx/pplxtasks.h"
#endif

// Stream
#include "cpprest/streams.h"
#include "cpprest/astreambuf.h"

#include "cpprest/asyncrt_utils.h"

// json
#include "cpprest/json.h"

// uri
#include "cpprest/base_uri.h"
#include "cpprest/uri_parser.h"

// http
#include "cpprest/http_msg.h"
#include "cpprest/http_client.h"
#include "cpprest/http_helpers.h"

#if !defined(__cplusplus_winrt)
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA 
#include "cpprest/http_server.h"
#include "cpprest/http_listener.h"
#include "cpprest/http_server_api.h"  
#endif // _WIN32_WINNT >= _WIN32_WINNT_VISTA 


#ifdef _MS_WINDOWS
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA 
#include "cpprest/http_windows_server.h"  
#endif // _WIN32_WINNT >= _WIN32_WINNT_VISTA 

#endif

#include "cpprest/log.h"
#include "cpprest/filelog.h"
#endif

#if defined(max)
#error: max macro defined -- make sure to #define NOMINMAX before including windows.h
#endif
#if defined(min)
#error: min macro defined -- make sure to #define NOMINMAX before including windows.h
#endif

#endif // #if defined(__STDAFX_H)
