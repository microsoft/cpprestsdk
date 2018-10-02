/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Platform-dependent type definitions
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <deque>
#include <queue>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include "cpprest/details/cpprest_compat.h"

#ifndef _WIN32
# define __STDC_LIMIT_MACROS
# include <stdint.h>
#else
#include <cstdint>
#endif

#include "cpprest/details/SafeInt3.hpp"

namespace utility
{

#ifdef _WIN32
#define _UTF16_STRINGS
#endif

// We should be using a 64-bit size type for most situations that do
// not involve specifying the size of a memory allocation or buffer.
typedef uint64_t size64_t;

#ifndef _WIN32
typedef uint32_t HRESULT; // Needed for PPLX
#endif

#if defined _WIN32 && !defined _DLL
// TEMPLATE CLASS allocator
template<class _Ty>
class allocator : public std::allocator<_Ty> {
public:
    typedef allocator<_Ty> _Alloc;
    typedef std::allocator<_Ty> _Mybase;

    template<class _Other>
    struct rebind {
        // convert this type to allocator<_Other>
        typedef allocator<_Other> other;
    };

    allocator() CPPREST_NOEXCEPT : _Mybase() {
        // construct default allocator (do nothing)
    }

    allocator(const _Alloc& _Right) CPPREST_NOEXCEPT : _Mybase(_Right) {
        // construct by copying (do nothing)
    }

    allocator(_Alloc&& _Right) CPPREST_NOEXCEPT : _Mybase(std::move(_Right)) {
        // construct by moving (do nothing)
    }

    template<class _Other>
    allocator(const allocator<_Other>& _Right) CPPREST_NOEXCEPT : _Mybase(_Right) {
        // construct from a related allocator (do nothing)
    }

    template<class _Other>
    allocator(allocator<_Other>&& _Right) CPPREST_NOEXCEPT : _Mybase(std::forward<_Other>(_Right)) {
        // construct from a related allocator (do nothing)
    }

    _Alloc& operator=(const _Alloc& _Right) {
        // assign by copying
        _Mybase::operator=(_Right);
        return (*this);
    }

    _Alloc& operator=(_Alloc&& _Right) {
        // assign by moving
        _Mybase::operator=(std::move(_Right));
        return (*this);
    }

    template<class _Other>
    _Alloc& operator=(const allocator<_Other>&) {
        // assign from a related allocator
        _Mybase::operator=(std::forward<_Other>(_Right));
        return (*this);
    }

    _DECLSPEC_ALLOCATOR pointer allocate(size_type _Count) {
        // allocate array of _Count elements
        return static_cast<pointer>(::HeapAlloc(::GetProcessHeap(), NULL, _Count * sizeof(_Ty)));
    }

    _DECLSPEC_ALLOCATOR pointer allocate(size_type _Count, const void *) {
        // allocate array of _Count elements, ignore hint
        return allocate(_Count);
    }

    void deallocate(pointer _Ptr, size_type _Count) {
        // deallocate object at _Ptr
        UNREFERENCED_PARAMETER(_Count);
        ::HeapFree(::GetProcessHeap(), NULL, _Ptr);
    }
};

template <class _Ty1, class _Ty2>
inline bool operator == (const allocator<_Ty1>&, const allocator<_Ty2>&) {
    return true;
}

template <class _Ty1, class _Ty2>
inline bool operator != (const allocator<_Ty1>& _Left, const allocator<_Ty2>& _Right) {
    return !(_Left == _Right);
}

// TEMPLATE CLASS secure_allocator
template<class _Ty>
class secure_allocator : public utility::allocator<_Ty> {
public:
    typedef secure_allocator<_Ty> _Alloc;
    typedef utility::allocator<_Ty> _Mybase;

    template<class _Other>
    struct rebind {
        // convert this type to secure_allocator<_Other>
        typedef secure_allocator<_Other> other;
    };

    secure_allocator() CPPREST_NOEXCEPT : _Mybase() {
        // construct default secure_allocator (do nothing)
    }

    secure_allocator(const _Alloc& _Right) CPPREST_NOEXCEPT : _Mybase(_Right) {
        // construct by copying (do nothing)
    }

    secure_allocator(_Alloc&& _Right) CPPREST_NOEXCEPT : _Mybase(std::move(_Right)) {
        // construct by moving (do nothing)
    }

    template<class _Other>
    secure_allocator(const secure_allocator<_Other>& _Right) CPPREST_NOEXCEPT : _Mybase(_Right) {
        // construct from a related secure_allocator (do nothing)
    }

    template<class _Other>
    secure_allocator(secure_allocator<_Other>&& _Right) CPPREST_NOEXCEPT : _Mybase(std::forward<_Other>(_Right)) {
        // construct from a related secure_allocator (do nothing)
    }

    _Alloc& operator=(const _Alloc& _Right) {
        // assign by copying
        _Mybase::operator=(_Right);
        return (*this);
    }

    _Alloc& operator=(_Alloc&& _Right) {
        // assign by moving
        _Mybase::operator=(std::move(_Right));
        return (*this);
    }

    template<class _Other>
    _Alloc& operator=(const secure_allocator<_Other>&) {
        // assign from a related secure_allocator
        _Mybase::operator=(std::forward<_Other>(_Right));
        return (*this);
    }

    _DECLSPEC_ALLOCATOR pointer allocate(size_type _Count) {
        // allocate array of _Count elements
        return static_cast<pointer>(::HeapAlloc(::GetProcessHeap(), NULL, _Count * sizeof(_Ty)));
    }

    _DECLSPEC_ALLOCATOR pointer allocate(size_type _Count, const void *) {
        // allocate array of _Count elements, ignore hint
        return allocate(_Count);
    }

    void deallocate(pointer _Ptr, size_type _Count) {
        // deallocate object at _Ptr
        ::SecureZeroMemory(_Ptr, _Count * sizeof(_Ty));
        ::HeapFree(::GetProcessHeap(), NULL, _Ptr);
    }
};

template <class _Ty1, class _Ty2>
inline bool operator == (const secure_allocator<_Ty1>&, const secure_allocator<_Ty2>&) {
    return true;
}

template <class _Ty1, class _Ty2>
inline bool operator != (const secure_allocator<_Ty1>& _Left, const secure_allocator<_Ty2>& _Right) {
    return !(_Left == _Right);
}
#else
template<class _Ty>
using allocator = std::allocator<_Ty>;
#endif

#if defined _WIN32 && !defined _DLL
// TEMPLATE CLASS unique_ptr_deleter
template<class _Ty>
struct unique_ptr_deleter {
    // deleter for unique_ptr
    constexpr unique_ptr_deleter() CPPREST_NOEXCEPT = default;

    template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
    unique_ptr_deleter(const unique_ptr_deleter<_Ty2>&) CPPREST_NOEXCEPT {
        // construct from another unique_ptr_deleter
    }

    void operator()(_Ty *_Ptr) const CPPREST_NOEXCEPT {
        // delete a pointer
        static_assert(0 < sizeof(_Ty), "can't delete an incomplete type");
        // destroy object at _Ptr
        _Ptr->~_Ty();
        ::HeapFree(::GetProcessHeap(), NULL, _Ptr);
    }
};

template<class _Ty>
struct unique_ptr_deleter<_Ty[]> {
    // deleter for unique_ptr to array of unknown size
    constexpr unique_ptr_deleter() CPPREST_NOEXCEPT = default;

    template<class _Uty, class = typename std::enable_if<std::is_convertible<_Uty(*)[], _Ty(*)[]>::value, void>::type>
    unique_ptr_deleter(const unique_ptr_deleter<_Uty[]>&) CPPREST_NOEXCEPT {
        // construct from another unique_ptr_deleter
    }

    template<class _Uty, class = typename std::enable_if<std::is_convertible<_Uty(*)[], _Ty(*)[]>::value, void>::type>
    void operator()(_Uty *_Array) const CPPREST_NOEXCEPT {
        // delete a pointer
        static_assert(0 < sizeof(_Uty), "can't delete an incomplete type");
        // destroy object at _Ptr
        size_t *_Ptr = reinterpret_cast<size_t*>(_Array);

        if (!std::is_trivially_destructible<_Uty>::value) {
            // Call destructor for non-trivially destructible types
            size_t _Count = *(--_Ptr);
            for (size_t _Idx = _Count; _Idx > 0; --_Idx) {
                _Array[_Idx - 1].~_Uty();
            }
        }

        ::HeapFree(::GetProcessHeap(), NULL, _Ptr);
    }

};

template<class _Ty>
struct secure_unique_ptr_deleter : public unique_ptr_deleter<_Ty> {
    // deleter for unique_ptr
    constexpr secure_unique_ptr_deleter() CPPREST_NOEXCEPT = default;

    template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
    secure_unique_ptr_deleter(const secure_unique_ptr_deleter<_Ty2>&) CPPREST_NOEXCEPT {
        // construct from another secure_unique_ptr_deleter
    }

    void operator()(_Ty *_Ptr) const CPPREST_NOEXCEPT {
        // delete a pointer
        static_assert(0 < sizeof(_Ty), "can't delete an incomplete type");
        // destroy object at _Ptr
        _Ptr->~_Ty();
        ::SecureZeroMemory(_Ptr, sizeof(_Ty));
        ::HeapFree(::GetProcessHeap(), NULL, _Ptr);
    }
};
#else
template<class _Ty>
using unique_ptr_deleter = std::default_delete<_Ty>

template<class _Ty>
using secure_unique_ptr_deleter = std::default_delete<_Ty>
#endif

template<class _Ty, class _Dx = utility::unique_ptr_deleter<_Ty>>
using unique_ptr = std::unique_ptr<_Ty, _Dx>;

template<class _Ty, class _Dx = utility::secure_unique_ptr_deleter<_Ty>>
using secure_unique_ptr = std::unique_ptr<_Ty, _Dx>;

// TEMPLATE FUNCTION make_unique
template<class _Ty, class _Dx = utility::unique_ptr_deleter<_Ty>, class... _Types> inline
    typename std::enable_if<!std::is_array<_Ty>::value,
    std::unique_ptr<_Ty, _Dx>>::type make_unique(_Types&&... _Args) {
    // make a unique_ptr
#if defined _WIN32 && !defined _DLL
    _Ty* _Ptr = static_cast<_Ty*>(::HeapAlloc(::GetProcessHeap(), NULL, sizeof(_Ty)));
    return std::unique_ptr<_Ty, _Dx>(new(_Ptr) _Ty(std::forward<_Types>(_Args)...));
#else
    return std::unique_ptr<_Ty, _Dx>(new _Ty(std::forward<_Types>(_Args)...));
#endif
}

template<class _Ty, class _Dx = utility::unique_ptr_deleter<_Ty>> inline
typename std::enable_if<std::is_array<_Ty>::value && std::extent<_Ty>::value == 0,
    std::unique_ptr<_Ty, _Dx>>::type make_unique(size_t _Count) {
    // make a unique_ptr
#if defined _WIN32 && !defined _DLL
    typedef typename std::remove_extent<_Ty>::type _Elem;

    size_t _Size = _Count * sizeof(_Elem);
    if (!std::is_trivially_destructible<_Elem>::value) {
        // Extra space for new[] overhead for non-trivially destructible types
        _Size += sizeof(size_t);
    }

    _Elem* _Ptr = static_cast<_Elem*>(::HeapAlloc(::GetProcessHeap(), NULL, _Size));
    _Elem* _Array = new(_Ptr) _Elem[_Count]();

    return std::unique_ptr<_Ty, _Dx>(_Array);
#else
    return std::unique_ptr<_Ty, _Dx>(new _Elem[_Count]());
#endif
}

template<class _Ty, class _Dx = utility::unique_ptr_deleter<_Ty>, class... _Types>
typename std::enable_if<std::extent<_Ty>::value != 0,
    void>::type make_unique(_Types&&...) = delete;


// TEMPLATE FUNCTION make_shared
template<typename _Ty, typename _Alloc = utility::allocator<_Ty>, typename... _Types>
inline std::shared_ptr<_Ty> make_shared(_Types&&... _Args) {
    // make a shared_ptr
    return std::allocate_shared<_Ty, _Alloc>(_Alloc(), std::forward<_Types>(_Args)...);
}


template<class _Ty, class _Alloc = utility::allocator<_Ty>>
using vector = std::vector<_Ty, _Alloc>;

template<class _Kty, class _Ty, class _Pr = std::less<_Kty>, class _Alloc = utility::allocator<std::pair<const _Kty, _Ty>>>
using map = std::map<_Kty, _Ty, _Pr, _Alloc>;

template<class _Kty, class _Ty, class _Hasher = std::hash<_Kty>,  class _Keyeq = std::equal_to<_Kty>, class _Alloc = utility::allocator<std::pair<const _Kty, _Ty>>>
using unordered_map = std::unordered_map<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;

template<class _Ty, class _Alloc = utility::allocator<_Ty>>
using deque = std::deque<_Ty, _Alloc>;

template<class _Ty, class _Container = utility::deque<_Ty>>
using queue = std::queue<_Ty, _Container>;

template<class _Ty, class _Traits = std::char_traits<_Ty>, class _Alloc = utility::allocator<_Ty>>
using basic_string = std::basic_string<_Ty, _Traits, _Alloc>;
template<class _Ty, class _Traits = std::char_traits<_Ty>, class _Alloc = utility::allocator<_Ty>>
using basic_ostringstream = std::basic_ostringstream<_Ty, _Traits, _Alloc>;
template<class _Ty, class _Traits = std::char_traits<_Ty>, class _Alloc = utility::allocator<_Ty>>
using basic_istringstream = std::basic_istringstream<_Ty, _Traits, _Alloc>;
template<class _Ty, class _Traits = std::char_traits<_Ty>, class _Alloc = utility::allocator<_Ty>>
using basic_stringstream = std::basic_stringstream<_Ty, _Traits, _Alloc>;

typedef utility::basic_string<char> string;
typedef std::basic_string<char, std::char_traits<char>, utility::secure_allocator<char>> secure_string;
typedef std::unique_ptr<utility::secure_string, utility::secure_unique_ptr_deleter<utility::secure_string>> secure_unique_string;
typedef utility::basic_ostringstream<char> ostringstream;
typedef utility::basic_istringstream<char> istringstream;
typedef utility::basic_stringstream<char> stringstream;

typedef utility::basic_string<wchar_t> wstring;
typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, utility::secure_allocator<wchar_t>> secure_wstring;
typedef std::unique_ptr<utility::secure_wstring, utility::secure_unique_ptr_deleter<utility::secure_wstring>> secure_unique_wstring;
typedef utility::basic_ostringstream<wchar_t> wostringstream;
typedef utility::basic_istringstream<wchar_t> wistringstream;
typedef utility::basic_stringstream<wchar_t> wstringstream;

#ifdef _UTF16_STRINGS
//
// On Windows, all strings are wide
//
typedef wchar_t char_t;
typedef utility::wstring string_t;
typedef utility::secure_wstring secure_string_t;
#define _XPLATSTR(x) L ## x
typedef utility::wostringstream ostringstream_t;
typedef std::wofstream ofstream_t;
typedef std::wostream ostream_t;
typedef std::wistream istream_t;
typedef std::wifstream ifstream_t;
typedef utility::wistringstream istringstream_t;
typedef utility::wstringstream stringstream_t;
#define ucout std::wcout
#define ucin std::wcin
#define ucerr std::wcerr
#else
//
// On POSIX platforms, all strings are narrow
//
typedef char char_t;
typedef utility::string string_t;
typedef utility::secure_string secure_string_t;
#define _XPLATSTR(x) x
typedef utility::ostringstream ostringstream_t;
typedef std::ofstream ofstream_t;
typedef std::ostream ostream_t;
typedef std::istream istream_t;
typedef std::ifstream ifstream_t;
typedef utility::istringstream istringstream_t;
typedef utility::stringstream stringstream_t;
#define ucout std::cout
#define ucin std::cin
#define ucerr std::cerr
#endif // endif _UTF16_STRINGS

#ifndef _TURN_OFF_PLATFORM_STRING
// The 'U' macro can be used to create a string or character literal of the platform type, i.e. utility::char_t.
// If you are using a library causing conflicts with 'U' macro, it can be turned off by defining the macro
// '_TURN_OFF_PLATFORM_STRING' before including the C++ REST SDK header files, and e.g. use '_XPLATSTR' instead.
#define U(x) _XPLATSTR(x)
#endif // !_TURN_OFF_PLATFORM_STRING

}// namespace utility

typedef char utf8char;
typedef utility::string utf8string;
typedef utility::stringstream utf8stringstream;
typedef utility::ostringstream utf8ostringstream;
typedef std::ostream utf8ostream;
typedef std::istream utf8istream;
typedef utility::istringstream utf8istringstream;

#ifdef _UTF16_STRINGS
typedef wchar_t utf16char;
typedef utility::wstring utf16string;
typedef utility::wstringstream utf16stringstream;
typedef utility::wostringstream utf16ostringstream;
typedef std::wostream utf16ostream;
typedef std::wistream utf16istream;
typedef utility::wistringstream utf16istringstream;
#else
typedef char16_t utf16char;
typedef std::u16string utf16string;
typedef utility::basic_stringstream<utf16char> utf16stringstream;
typedef utility::basic_ostringstream<utf16char> utf16ostringstream;
typedef std::basic_ostream<utf16char> utf16ostream;
typedef std::basic_istream<utf16char> utf16istream;
typedef utility::basic_istringstream<utf16char> utf16istringstream;
#endif


#if defined(_WIN32)
// Include on everything except Windows Desktop ARM, unless explicitly excluded.
#if !defined(CPPREST_EXCLUDE_WEBSOCKETS)
#if defined(WINAPI_FAMILY)
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && defined(_M_ARM)
#define CPPREST_EXCLUDE_WEBSOCKETS
#endif
#else
#if defined(_M_ARM)
#define CPPREST_EXCLUDE_WEBSOCKETS
#endif
#endif
#endif
#endif
