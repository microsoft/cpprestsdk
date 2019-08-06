/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Internal headers
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#if !defined(_WIN32)
#error windows_config.h use only expected for Windows builds. (_WIN32 is not defined)
#endif

// use the debug version of the CRT if _DEBUG is defined
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif // _DEBUG

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#if CPPREST_TARGET_XP && _WIN32_WINNT != 0x0501
#error CPPREST_TARGET_XP implies _WIN32_WINNT == 0x0501
#endif // CPPREST_TARGET_XP && _WIN32_WINNT != 0x0501

#include <objbase.h>

#include <windows.h>

// Windows Header Files:
#ifndef __cplusplus_winrt
#include <winhttp.h>
#endif !__cplusplus_winrt
