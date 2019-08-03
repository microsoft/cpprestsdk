/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * String utilities.
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#pragma once

#include "cpprest/details/basic_types.h"
#include <cstdint>
#include <limits.h>
#include <locale.h>
#include <string>
#include <typeinfo>
#include <vector>

#ifndef _WIN32
#include <sys/time.h>
#if !defined(ANDROID) && !defined(__ANDROID__) && defined(HAVE_XLOCALE_H) // CodePlex 269
/* Systems using glibc: xlocale.h has been removed from glibc 2.26
   The above include of locale.h is sufficient
   Further details: https://sourceware.org/git/?p=glibc.git;a=commit;h=f0be25b6336db7492e47d2e8e72eb8af53b5506d */
#include <xlocale.h>
#endif
#endif

/// Various utilities for string conversions and date and time manipulation.
namespace utility
{
/// Functions for Unicode string conversions.
namespace conversions
{
/// <summary>
/// Converts a UTF-16 string to a UTF-8 string.
/// </summary>
/// <param name="w">A two byte character UTF-16 string.</param>
/// <returns>A single byte character UTF-8 string.</returns>
_ASYNCRTIMP std::string __cdecl utf16_to_utf8(utf16string_view w);

/// <summary>
/// Converts a UTF-8 string to a UTF-16
/// </summary>
/// <param name="s">A single byte character UTF-8 string.</param>
/// <returns>A two byte character UTF-16 string.</returns>
_ASYNCRTIMP utf16string __cdecl utf8_to_utf16(utility::nstring_view_t s);

/// <summary>
/// Converts a ASCII (us-ascii) string to a UTF-16 string.
/// </summary>
/// <param name="s">A single byte character us-ascii string.</param>
/// <returns>A two byte character UTF-16 string.</returns>
_ASYNCRTIMP utf16string __cdecl usascii_to_utf16(utility::nstring_view_t s);

/// <summary>
/// Converts a Latin1 (iso-8859-1) string to a UTF-16 string.
/// </summary>
/// <param name="s">A single byte character UTF-8 string.</param>
/// <returns>A two byte character UTF-16 string.</returns>
_ASYNCRTIMP utf16string __cdecl latin1_to_utf16(utility::nstring_view_t s);

/// <summary>
/// Converts a Latin1 (iso-8859-1) string to a UTF-8 string.
/// </summary>
/// <param name="s">A single byte character UTF-8 string.</param>
/// <returns>A single byte character UTF-8 string.</returns>
_ASYNCRTIMP utf8string __cdecl latin1_to_utf8(utility::nstring_view_t s);

/// <summary>
/// Converts to a platform dependent Unicode string type.
/// </summary>
/// <param name="s">A single byte character UTF-8 string.</param>
/// <returns>A platform dependent string type.</returns>
#ifdef _UTF16_STRINGS
_ASYNCRTIMP utility::string_t __cdecl to_string_t(std::string&& s);
#else
inline utility::string_t&& to_string_t(std::string&& s) { return std::move(s); }
#endif

/// <summary>
/// Converts to a platform dependent Unicode string type.
/// </summary>
/// <param name="s">A two byte character UTF-16 string.</param>
/// <returns>A platform dependent string type.</returns>
#ifdef _UTF16_STRINGS
inline utility::string_t&& to_string_t(utf16string&& s) { return std::move(s); }
#else
_ASYNCRTIMP utility::string_t __cdecl to_string_t(utf16string&& s);
#endif
/// <summary>
/// Converts to a platform dependent Unicode string type.
/// </summary>
/// <param name="s">A single byte character UTF-8 string.</param>
/// <returns>A platform dependent string type.</returns>
#ifdef _UTF16_STRINGS
_ASYNCRTIMP utility::string_t __cdecl to_string_t(utility::nstring_view_t s);
#else
inline const utility::string_t& to_string_t(const std::string& s) { return s; }
#if CPPREST_USE_STRING_VIEWS
inline utility::string_view_t to_string_t(utility::nstring_view_t s) { return s; }
#endif
#endif

#if CPPREST_USE_STRING_VIEWS
// Provide precise overload for string literals to resolve ambiguity of other overloads.
template<typename CharType>
inline auto to_string_t(const CharType * s) { return to_string_t(std::basic_string_view(s)); }
#endif

/// <summary>
/// Converts to a platform dependent Unicode string type.
/// </summary>
/// <param name="s">A two byte character UTF-16 string.</param>
/// <returns>A platform dependent string type.</returns>
#ifdef _UTF16_STRINGS
inline const utility::string_t& to_string_t(const utf16string& s) { return s; }
#if CPPREST_USE_STRING_VIEWS
inline utf16string_view to_string_t(utf16string_view s) { return s; }
#endif
#else
_ASYNCRTIMP utility::string_t __cdecl to_string_t(utf16string_view s);
#endif

/// <summary>
/// Converts to a UTF-16 from string.
/// </summary>
/// <param name="value">A single byte character UTF-8 string.</param>
/// <returns>A two byte character UTF-16 string.</returns>
_ASYNCRTIMP utf16string __cdecl to_utf16string(utility::nstring_view_t value);

/// <summary>
/// Converts to a UTF-16 from string.
/// </summary>
/// <param name="value">A two byte character UTF-16 string.</param>
/// <returns>A two byte character UTF-16 string.</returns>
inline const utf16string& to_utf16string(const utf16string& value) { return value; }
#if CPPREST_USE_STRING_VIEWS
inline utf16string_view to_utf16string(utf16string_view value) { return value; }
#endif
/// <summary>
/// Converts to a UTF-16 from string.
/// </summary>
/// <param name="value">A two byte character UTF-16 string.</param>
/// <returns>A two byte character UTF-16 string.</returns>
inline utf16string&& to_utf16string(utf16string&& value) { return std::move(value); }

#if CPPREST_USE_STRING_VIEWS
// Provide precise overload for string literals to resolve ambiguity of other overloads.
template<typename CharType>
inline auto to_utf16string(const CharType * s) { return to_utf16string(std::basic_string_view(s)); }
#endif

/// <summary>
/// Converts to a UTF-8 string.
/// </summary>
/// <param name="value">A single byte character UTF-8 string.</param>
/// <returns>A single byte character UTF-8 string.</returns>
inline std::string&& to_utf8string(std::string&& value) { return std::move(value); }

/// <summary>
/// Converts to a UTF-8 string.
/// </summary>
/// <param name="value">A single byte character UTF-8 string.</param>
/// <returns>A single byte character UTF-8 string.</returns>
inline const std::string& to_utf8string(const std::string& value) { return value; }
#if CPPREST_USE_STRING_VIEWS
inline utility::nstring_view_t to_utf8string(utility::nstring_view_t value) { return value; }
#endif

/// <summary>
/// Converts to a UTF-8 string.
/// </summary>
/// <param name="value">A two byte character UTF-16 string.</param>
/// <returns>A single byte character UTF-8 string.</returns>
_ASYNCRTIMP std::string __cdecl to_utf8string(utf16string_view value);

#if CPPREST_USE_STRING_VIEWS
// Provide precise overload for string literals to resolve ambiguity of other overloads.
template<typename CharType>
inline auto to_utf8string(const CharType * s) { return to_utf8string(std::basic_string_view(s)); }
#endif

template<typename Source>
CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if "
                      "locale support is required.")
utility::string_t print_string(const Source& val, const std::locale& loc = std::locale())
{
    utility::ostringstream_t oss;
    oss.imbue(loc);
    oss << val;
    if (oss.bad())
    {
        throw std::bad_cast();
    }
    return oss.str();
}

CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if "
                      "locale support is required.")
inline utility::string_t print_string(const utility::string_t& val) { return val; }

namespace details
{
#if defined(__ANDROID__)
template<class T>
inline std::string to_string(const T t)
{
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << t;
    return os.str();
}
#endif

template<class T>
inline utility::string_t to_string_t(const T t)
{
#ifdef _UTF16_STRINGS
    using std::to_wstring;
    return to_wstring(t);
#else
#if !defined(__ANDROID__)
    using std::to_string;
#endif
    return to_string(t);
#endif
}

template<typename Source>
utility::string_t print_string(const Source& val)
{
    utility::ostringstream_t oss;
    oss.imbue(std::locale::classic());
    oss << val;
    if (oss.bad())
    {
        throw std::bad_cast();
    }
    return oss.str();
}

inline const utility::string_t& print_string(const utility::string_t& val) { return val; }
#if CPPREST_USE_STRING_VIEWS
inline utility::string_view_t print_string(utility::string_view_t val) { return val; }
#endif

template<typename Source>
utf8string print_utf8string(const Source& val)
{
    return conversions::to_utf8string(print_string(val));
}
inline const utf8string& print_utf8string(const utf8string& val) { return val; }

template<typename Target>
Target scan_string(const utility::string_t& str)
{
    Target t;
    utility::istringstream_t iss(str);
    iss.imbue(std::locale::classic());
    iss >> t;
    if (iss.bad())
    {
        throw std::bad_cast();
    }
    return t;
}

inline const utility::string_t& scan_string(const utility::string_t& str) { return str; }
} // namespace details

template<typename Target>
CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if "
                      "locale support is required.")
Target scan_string(const utility::string_t& str, const std::locale& loc = std::locale())
{
    Target t;
    utility::istringstream_t iss(str);
    iss.imbue(loc);
    iss >> t;
    if (iss.bad())
    {
        throw std::bad_cast();
    }
    return t;
}

CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if "
                      "locale support is required.")
inline utility::string_t scan_string(const utility::string_t& str) { return str; }
} // namespace conversions

namespace details
{
/// <summary>
/// Cross platform RAII container for setting thread local locale.
/// </summary>
class scoped_c_thread_locale
{
public:
    _ASYNCRTIMP scoped_c_thread_locale();
    _ASYNCRTIMP ~scoped_c_thread_locale();

#if !defined(ANDROID) && !defined(__ANDROID__) // CodePlex 269
#ifdef _WIN32
    typedef _locale_t xplat_locale;
#else
    typedef locale_t xplat_locale;
#endif

    static _ASYNCRTIMP xplat_locale __cdecl c_locale();
#endif
private:
#ifdef _WIN32
    std::string m_prevLocale;
    int m_prevThreadSetting;
#elif !(defined(ANDROID) || defined(__ANDROID__))
    locale_t m_prevLocale;
#endif
    scoped_c_thread_locale(const scoped_c_thread_locale&);
    scoped_c_thread_locale& operator=(const scoped_c_thread_locale&);
};

/// <summary>
/// Our own implementation of alpha numeric instead of std::isalnum to avoid
/// taking global lock for performance reasons.
/// </summary>
inline bool __cdecl is_alnum(const unsigned char uch) CPPREST_NOEXCEPT
{ // test if uch is an alnum character
    // special casing char to avoid branches
    // clang-format off
    static CPPREST_CONSTEXPR bool is_alnum_table[UCHAR_MAX + 1] = {
        /*        X0 X1 X2 X3 X4 X5 X6 X7 X8 X9 XA XB XC XD XE XF */
        /* 0X */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* 1X */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* 2X */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* 3X */   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, /* 0-9 */
        /* 4X */   0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* A-Z */
        /* 5X */   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
        /* 6X */   0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* a-z */
        /* 7X */   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
        /* non-ASCII values initialized to 0 */
    };
    // clang-format on
    return (is_alnum_table[uch]);
}

/// <summary>
/// Our own implementation of alpha numeric instead of std::isalnum to avoid
/// taking global lock for performance reasons.
/// </summary>
inline bool __cdecl is_alnum(const char ch) CPPREST_NOEXCEPT { return (is_alnum(static_cast<unsigned char>(ch))); }

/// <summary>
/// Our own implementation of alpha numeric instead of std::isalnum to avoid
/// taking global lock for performance reasons.
/// </summary>
template<class Elem>
inline bool __cdecl is_alnum(Elem ch) CPPREST_NOEXCEPT
{
    // assumes 'x' == L'x' for the ASCII range
    typedef typename std::make_unsigned<Elem>::type UElem;
    const auto uch = static_cast<UElem>(ch);
    return (uch <= static_cast<UElem>('z') && is_alnum(static_cast<unsigned char>(uch)));
}


/// <summary>
/// Cross platform utility function for performing case insensitive string equality comparison.
/// </summary>
/// <param name="left">First string to compare.</param>
/// <param name="right">Second strong to compare.</param>
/// <returns>true if the strings are equivalent, false otherwise</returns>
_ASYNCRTIMP bool __cdecl str_iequal(utility::nstring_view_t left, utility::nstring_view_t right) CPPREST_NOEXCEPT;

/// <summary>
/// Cross platform utility function for performing case insensitive string equality comparison.
/// </summary>
/// <param name="left">First string to compare.</param>
/// <param name="right">Second strong to compare.</param>
/// <returns>true if the strings are equivalent, false otherwise</returns>
_ASYNCRTIMP bool __cdecl str_iequal(utility::wstring_view_t left, utility::wstring_view_t right) CPPREST_NOEXCEPT;

/// <summary>
/// Cross platform utility function for performing case insensitive string less-than comparison.
/// </summary>
/// <param name="left">First string to compare.</param>
/// <param name="right">Second strong to compare.</param>
/// <returns>true if a lowercase view of left is lexicographically less than a lowercase view of right; otherwise,
/// false.</returns>
_ASYNCRTIMP bool __cdecl str_iless(utility::nstring_view_t left, utility::nstring_view_t right) CPPREST_NOEXCEPT;

/// <summary>
/// Cross platform utility function for performing case insensitive string less-than comparison.
/// </summary>
/// <param name="left">First string to compare.</param>
/// <param name="right">Second strong to compare.</param>
/// <returns>true if a lowercase view of left is lexicographically less than a lowercase view of right; otherwise,
/// false.</returns>
_ASYNCRTIMP bool __cdecl str_iless(utility::wstring_view_t left, utility::wstring_view_t right) CPPREST_NOEXCEPT;

/// <summary>
/// Convert a string to lowercase in place.
/// </summary>
/// <param name="target">The string to convert to lowercase.</param>
_ASYNCRTIMP void __cdecl inplace_tolower(std::string& target) CPPREST_NOEXCEPT;

/// <summary>
/// Convert a string to lowercase in place.
/// </summary>
/// <param name="target">The string to convert to lowercase.</param>
_ASYNCRTIMP void __cdecl inplace_tolower(std::wstring& target) CPPREST_NOEXCEPT;

} // namespace details

} // namespace utility
