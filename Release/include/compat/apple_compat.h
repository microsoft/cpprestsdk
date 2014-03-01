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
 * apple_compat.h
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/


#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#define __cdecl

#include "nosal.h"

// MSVC doesn't support this yet
#define _noexcept noexcept

#define novtable /* no novtable equivalent */
#define __declspec(x) __attribute__ ((x))
#define __export __attribute__ ((dllexport))
#define __stdcall __attribute__ ((stdcall))
#define STDMETHODCALLTYPE __export __stdcall

// ignore these:
#define dllimport 

// OpenProt defines
#define _SH_DENYRW 0x20

#include <stdint.h>

#define __assume(x) do { if (!(x)) __builtin_unreachable(); } while (false)

typedef uint32_t HRESULT;

#define SOCKET int
#define SOCKET_ERROR -1

#define S_OK 0
#define S_FALSE 1
#define STG_E_CANTSAVE 0x80030103
#define STG_E_INVALIDPOINTER 0x80030009
#define E_NOTIMPL 0x80004001
#define E_NOINTERFACE 0x80004002

typedef unsigned long ULONG;
typedef unsigned short WORD;
typedef unsigned long DWORD;

typedef struct _SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME;

#define ULARGE_INTEGER uint64_t
#define LARGE_INTEGER int64_t

#define WINAPI __stdcall

#define YieldProcessor() __asm__ __volatile__ ("pause")

#define UNREFERENCED_PARAMETER(x) (void)x
#define _ASSERTE(x) assert(x)

#ifdef CASABLANCA_DEPRECATION_NO_WARNINGS
#define CASABLANCA_DEPRECATED(x)
#else
#define CASABLANCA_DEPRECATED(x) __attribute__((deprecated(x)))
#endif

#include <string>

typedef char16_t utf16char;
typedef std::u16string utf16string;
typedef std::basic_stringstream<char16_t> utf16stringstream;
typedef std::basic_ostringstream<char16_t> utf16ostringstream;
typedef std::basic_ostream<char16_t> utf16ostream;
typedef std::basic_istream<char16_t> utf16istream;
typedef std::basic_istringstream<char16_t> utf16istringstream;

#include "compat/SafeInt3.hpp"
typedef SafeInt<size_t> SafeSize;
