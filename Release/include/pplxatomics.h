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
* pplxatomics.h
*
* Replaces std::atomics as it is not supported in dev 10
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _PPLXATOMICS_H
#define _PPLXATOMICS_H

#include "pplxdefs.h"

#ifdef _MS_WINDOWS
#if _MSC_VER >= 1700
#define _USE_REAL_ATOMICS
#endif
#else // GCC compiler
#define _USE_REAL_ATOMICS
#endif

#ifdef _USE_REAL_ATOMICS
#include <atomic>
#else
#include <intrin.h>
#endif

#pragma pack(push,_CRT_PACKING)

/// <summary>
///     The <c>pplx</c> namespace provides classes and functions that give you access to the Concurrency Runtime,
///     a concurrent programming framework for C++. For more information, see <see cref="Concurrency Runtime"/>.
/// </summary>
/**/
namespace pplx
{

#ifdef _USE_REAL_ATOMICS
typedef std::atomic<long> atomic_long;
typedef std::atomic<size_t> atomic_size_t;

template<typename _T>
_T atomic_compare_exchange(std::atomic<_T>& _Target, _T _Exchange, _T _Comparand)
{
    _T _Result = _Comparand;
    _Target.compare_exchange_strong(_Result, _Exchange);
    return _Result;
}

template<typename _T>
_T atomic_exchange(std::atomic<_T>& _Target, _T _Value)
{
    return _Target.exchange(_Value);
}

template<typename _T>
_T atomic_increment(std::atomic<_T>& _Target)
{
    return _Target.fetch_add(1) + 1;
}

template<typename _T>
_T atomic_decrement(std::atomic<_T>& _Target)
{
    return _Target.fetch_sub(1) - 1;
}

template<typename _T>
_T atomic_add(std::atomic<_T>& _Target, _T value)
{
    return _Target.fetch_add(value) + value;
}

#else // not _USE_REAL_ATOMICS - windows code

typedef long volatile atomic_long;
typedef size_t volatile atomic_size_t;

template<class T>
inline T atomic_exchange(T volatile& _Target, T _Value)
{
    return _InterlockedExchange(&_Target, _Value);
}

inline long atomic_increment(long volatile & _Target)
{
    return _InterlockedIncrement(&_Target);
}

inline long atomic_add(long volatile & _Target, long value)
{
    return _InterlockedExchangeAdd(&_Target, value) + value;
}

inline size_t atomic_increment(size_t volatile & _Target)
{
#if (defined(_M_IX86) || defined(_M_ARM))
    return static_cast<size_t>(_InterlockedIncrement(reinterpret_cast<long volatile *>(&_Target)));
#else
    return static_cast<size_t>(_InterlockedIncrement64(reinterpret_cast<__int64 volatile *>(&_Target)));
#endif
}

inline long atomic_decrement(long volatile & _Target)
{
    return _InterlockedDecrement(&_Target);
}


inline size_t atomic_decrement(size_t volatile & _Target)
{
#if (defined(_M_IX86) || defined(_M_ARM))
    return static_cast<size_t>(_InterlockedDecrement(reinterpret_cast<long volatile *>(&_Target)));
#else
    return static_cast<size_t>(_InterlockedDecrement64(reinterpret_cast<__int64 volatile *>(&_Target)));
#endif
}

inline long atomic_compare_exchange(long volatile & _Target, long _Exchange, long _Comparand)
{
    return _InterlockedCompareExchange(&_Target, _Exchange, _Comparand);
}

inline size_t atomic_compare_exchange(size_t volatile & _Target, size_t _Exchange, size_t _Comparand)
{
#if (defined(_M_IX86) || defined(_M_ARM))
    return static_cast<size_t>(_InterlockedCompareExchange(reinterpret_cast<long volatile *>(_Target), static_cast<long>(_Exchange), static_cast<long>(_Comparand)));
#else
    return static_cast<size_t>(_InterlockedCompareExchange64(reinterpret_cast<__int64 volatile *>(_Target), static_cast<__int64>(_Exchange), static_cast<__int64>(_Comparand)));
#endif
}
#endif // _USE_REAL_ATOMICS

} // namespace pplx

#pragma pack(pop)

#endif // _PPLXATOMICS_H
