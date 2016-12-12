/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Utilities
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#ifndef _WIN32
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

// Could use C++ standard library if not __GLIBCXX__,
// For testing purposes we just the handwritten on all platforms.
#if defined(CPPREST_STDLIB_UNICODE_CONVERSIONS)
#include <codecvt>
#endif

using namespace web;
using namespace utility;
using namespace utility::conversions;

namespace utility
{

namespace details
{

#if !defined(ANDROID) && !defined(__ANDROID__)
std::once_flag g_c_localeFlag;
std::unique_ptr<scoped_c_thread_locale::xplat_locale, void(*)(scoped_c_thread_locale::xplat_locale *)> g_c_locale(nullptr, [](scoped_c_thread_locale::xplat_locale *){});
scoped_c_thread_locale::xplat_locale scoped_c_thread_locale::c_locale()
{
    std::call_once(g_c_localeFlag, [&]()
    {
        scoped_c_thread_locale::xplat_locale *clocale = new scoped_c_thread_locale::xplat_locale();
#ifdef _WIN32
        *clocale = _create_locale(LC_ALL, "C");
        if (clocale == nullptr || *clocale == nullptr)
        {
            throw std::runtime_error("Unable to create 'C' locale.");
        }
        auto deleter = [](scoped_c_thread_locale::xplat_locale *clocale)
        {
            _free_locale(*clocale);
            delete clocale;
        };
#else
        *clocale = newlocale(LC_ALL, "C", nullptr);
        if (clocale == nullptr || *clocale == nullptr)
        {
            throw std::runtime_error("Unable to create 'C' locale.");
        }
        auto deleter = [](scoped_c_thread_locale::xplat_locale *clocale)
        {
            freelocale(*clocale);
            delete clocale;
        };
#endif
        g_c_locale = std::unique_ptr<scoped_c_thread_locale::xplat_locale, void(*)(scoped_c_thread_locale::xplat_locale *)>(clocale, deleter);
    });
    return *g_c_locale;
}
#endif

#ifdef _WIN32
scoped_c_thread_locale::scoped_c_thread_locale()
    : m_prevLocale(), m_prevThreadSetting(-1)
{
    char *prevLocale = setlocale(LC_ALL, nullptr);
    if (prevLocale == nullptr)
    {
        throw std::runtime_error("Unable to retrieve current locale.");
    }

    if (std::strcmp(prevLocale, "C") != 0)
    {
        m_prevLocale = prevLocale;
        m_prevThreadSetting = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        if (m_prevThreadSetting == -1)
        {
            throw std::runtime_error("Unable to enable per thread locale.");
        }
        if (setlocale(LC_ALL, "C") == nullptr)
        {
             _configthreadlocale(m_prevThreadSetting);
             throw std::runtime_error("Unable to set locale");
        }
    }
}

scoped_c_thread_locale::~scoped_c_thread_locale()
{
    if (m_prevThreadSetting != -1)
    {
        setlocale(LC_ALL, m_prevLocale.c_str());
        _configthreadlocale(m_prevThreadSetting);
    }
}
#elif (defined(ANDROID) || defined(__ANDROID__))
scoped_c_thread_locale::scoped_c_thread_locale() {}
scoped_c_thread_locale::~scoped_c_thread_locale() {}
#else
scoped_c_thread_locale::scoped_c_thread_locale()
    : m_prevLocale(nullptr)
{
    char *prevLocale = setlocale(LC_ALL, nullptr);
    if (prevLocale == nullptr)
    {
        throw std::runtime_error("Unable to retrieve current locale.");
    }

    if (std::strcmp(prevLocale, "C") != 0)
    {
        m_prevLocale = uselocale(c_locale());
        if (m_prevLocale == nullptr)
        {
            throw std::runtime_error("Unable to set locale");
        }
    }
}

scoped_c_thread_locale::~scoped_c_thread_locale()
{
    if (m_prevLocale != nullptr)
    {
        uselocale(m_prevLocale);
    }
}
#endif
}

namespace details
{

const std::error_category & __cdecl platform_category()
{
#ifdef _WIN32
    return windows_category();
#else
    return linux_category();
#endif
}

#ifdef _WIN32

// Remove once VS 2013 is no longer supported.
#if _MSC_VER < 1900
static details::windows_category_impl instance;
#endif
const std::error_category & __cdecl windows_category()
{
#if _MSC_VER >= 1900
    static details::windows_category_impl instance;
#endif
    return instance;
}

std::string windows_category_impl::message(int errorCode) const CPPREST_NOEXCEPT
{
    const size_t buffer_size = 4096;
    DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM;
    LPCVOID lpSource = NULL;

#if !defined(__cplusplus_winrt)
    if (errorCode >= 12000)
    {
        dwFlags = FORMAT_MESSAGE_FROM_HMODULE;
        lpSource = GetModuleHandleA("winhttp.dll"); // this handle DOES NOT need to be freed
    }
#endif

    std::wstring buffer;
    buffer.resize(buffer_size);

    const auto result = ::FormatMessageW(
        dwFlags,
        lpSource,
        errorCode,
        0,
        &buffer[0],
        buffer_size,
        NULL);
    if (result == 0)
    {
        std::ostringstream os;
        os << "Unable to get an error message for error code: " << errorCode << ".";
        return os.str();
    }

    return utility::conversions::to_utf8string(buffer);
}

std::error_condition windows_category_impl::default_error_condition(int errorCode) const CPPREST_NOEXCEPT
{
    // First see if the STL implementation can handle the mapping for common cases.
    const std::error_condition errCondition = std::system_category().default_error_condition(errorCode);
    const std::string errConditionMsg = errCondition.message();
    if(_stricmp(errConditionMsg.c_str(), "unknown error") != 0)
    {
        return errCondition;
    }

    switch(errorCode)
    {
#ifndef __cplusplus_winrt
    case ERROR_WINHTTP_TIMEOUT:
        return std::errc::timed_out;
    case ERROR_WINHTTP_CANNOT_CONNECT:
        return std::errc::host_unreachable;
    case ERROR_WINHTTP_CONNECTION_ERROR:
        return std::errc::connection_aborted;
#endif
    case INET_E_RESOURCE_NOT_FOUND:
    case INET_E_CANNOT_CONNECT:
        return std::errc::host_unreachable;
    case INET_E_CONNECTION_TIMEOUT:
        return std::errc::timed_out;
    case INET_E_DOWNLOAD_FAILURE:
        return std::errc::connection_aborted;
    default:
        break;
    }

    return std::error_condition(errorCode, *this);
}

#else

const std::error_category & __cdecl linux_category()
{
    // On Linux we are using boost error codes which have the exact same
    // mapping and are equivalent with std::generic_category error codes.
    return std::generic_category();
}

#endif

}

#define LOW_3BITS 0x7
#define LOW_4BITS 0xF
#define LOW_5BITS 0x1F
#define LOW_6BITS 0x3F
#define BIT4 0x8
#define BIT5 0x10
#define BIT6 0x20
#define BIT7 0x40
#define BIT8 0x80
#define L_SURROGATE_START 0xDC00
#define L_SURROGATE_END 0xDFFF
#define H_SURROGATE_START 0xD800
#define H_SURROGATE_END 0xDBFF
#define SURROGATE_PAIR_START 0x10000

utf16string __cdecl conversions::utf8_to_utf16(const std::string &s)
{
#if defined(CPPREST_STDLIB_UNICODE_CONVERSIONS)
    std::wstring_convert<std::codecvt_utf8_utf16<utf16char>, utf16char> conversion;
    return conversion.from_bytes(s);
#else
    utf16string dest;
    // Save repeated heap allocations, use less than source string size assuming some
    // of the characters are not just ASCII and collapse.
    dest.reserve(static_cast<size_t>(static_cast<double>(s.size()) * .70));
    
    for (auto src = s.begin(); src != s.end(); ++src)
    {
        if ((*src & BIT8) == 0) // single byte character, 0x0 to 0x7F
        {
            dest.push_back(utf16string::value_type(*src));
        }
        else
        {
            unsigned char numContBytes = 0;
            uint32_t codePoint;
            if ((*src & BIT7) == 0)
            {
                throw std::range_error("UTF-8 string character can never start with 10xxxxxx");
            }
            else if ((*src & BIT6) == 0) // 2 byte character, 0x80 to 0x7FF
            {
                codePoint = *src & LOW_5BITS;
                numContBytes = 1;
            }
            else if ((*src & BIT5) == 0) // 3 byte character, 0x800 to 0xFFFF
            {
                codePoint = *src & LOW_4BITS;
                numContBytes = 2;
            }
            else if ((*src & BIT4) == 0) // 4 byte character, 0x10000 to 0x10FFFF
            {
                codePoint = *src & LOW_3BITS;
                numContBytes = 3;
            }
            else
            {
                throw std::range_error("UTF-8 string has invalid Unicode code point");
            }

            for (unsigned char i = 0; i < numContBytes; ++i)
            {
                if (++src == s.end())
                {
                    throw std::range_error("UTF-8 string is missing bytes in character");
                }
                if ((*src & BIT8) == 0 || (*src & BIT7) != 0)
                {
                    throw std::range_error("UTF-8 continuation byte is missing leading byte");
                }
                codePoint <<= 6;
                codePoint |= *src & LOW_6BITS;
            }

            if (codePoint >= SURROGATE_PAIR_START)
            {
                // In UTF-16 U+10000 to U+10FFFF are represented as two 16-bit code units, surrogate pairs.
                //  - 0x10000 is subtracted from the code point
                //  - high surrogate is 0xD800 added to the top ten bits
                //  - low surrogate is 0xDC00 added to the low ten bits
                codePoint -= SURROGATE_PAIR_START;
                dest.push_back(utf16string::value_type((codePoint >> 10) | H_SURROGATE_START));
                dest.push_back(utf16string::value_type((codePoint & 0x3FF) | L_SURROGATE_START));
            }
            else
            {
                // In UTF-16 U+0000 to U+D7FF and U+E000 to U+FFFF are represented exactly as the Unicode code point value.
                // U+D800 to U+DFFF are not valid characters, for simplicity we assume they are not present but will encode
                // them if encountered.
                dest.push_back(utf16string::value_type(codePoint));
            }
        }
    }
    return dest;
#endif
}

std::string __cdecl conversions::utf16_to_utf8(const utf16string &w)
{
 #if defined(CPPREST_STDLIB_UNICODE_CONVERSIONS)
     std::wstring_convert<std::codecvt_utf8_utf16<utf16char>, utf16char> conversion;
     return conversion.to_bytes(w);
 #else
    std::string dest;
    dest.reserve(w.size());
    for (auto src = w.begin(); src != w.end(); ++src)
    {
        // Check for high surrogate.
        if (*src >= H_SURROGATE_START && *src <= H_SURROGATE_END)
        {
            const auto highSurrogate = *src++;
            if (src == w.end())
            {
                throw std::range_error("UTF-16 string is missing low surrogate");
            }
            const auto lowSurrogate = *src;
            if (lowSurrogate < L_SURROGATE_START || lowSurrogate > L_SURROGATE_END)
            {
                throw std::range_error("UTF-16 string has invalid low surrogate");
            }

            // To get from surrogate pair to Unicode code point:
            // - subract 0xD800 from high surrogate, this forms top ten bits
            // - subract 0xDC00 from low surrogate, this forms low ten bits
            // - add 0x10000
            // Leaves a code point in U+10000 to U+10FFFF range.
            uint32_t codePoint = highSurrogate - H_SURROGATE_START;
            codePoint <<= 10;
            codePoint |= lowSurrogate - L_SURROGATE_START;
            codePoint += SURROGATE_PAIR_START;

            // 4 bytes need using 21 bits
            dest.push_back(char((codePoint >> 18) | 0xF0));                 // leading 3 bits
            dest.push_back(char(((codePoint >> 12) & LOW_6BITS) | BIT8));   // next 6 bits
            dest.push_back(char(((codePoint >> 6) & LOW_6BITS) | BIT8));    // next 6 bits
            dest.push_back(char((codePoint & LOW_6BITS) | BIT8));           // trailing 6 bits
        }
        else
        {
            if (*src <= 0x7F) // single byte character
            {
                dest.push_back(static_cast<char>(*src));
            }
            else if (*src <= 0x7FF) // 2 bytes needed (11 bits used)
            {
                dest.push_back(char((*src >> 6) | 0xC0));               // leading 5 bits
                dest.push_back(char((*src & LOW_6BITS) | BIT8));        // trailing 6 bits
            }
            else // 3 bytes needed (16 bits used)
            {
                dest.push_back(char((*src >> 12) | 0xE0));              // leading 4 bits
                dest.push_back(char(((*src >> 6) & LOW_6BITS) | BIT8)); // middle 6 bits
                dest.push_back(char((*src & LOW_6BITS) | BIT8));        // trailing 6 bits
            }
        }
    }

    return dest;
 #endif
}

utf16string __cdecl conversions::usascii_to_utf16(const std::string &s)
{
    // Ascii is a subset of UTF-8 so just convert to UTF-16
    return utf8_to_utf16(s);
}

utf16string __cdecl conversions::latin1_to_utf16(const std::string &s)
{
    // Latin1 is the first 256 code points in Unicode.
    // In UTF-16 encoding each of these is represented as exactly the numeric code point.
    utf16string dest;
    dest.resize(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        dest[i] = utf16char(static_cast<unsigned char>(s[i]));
    }
    return dest;
}

utf8string __cdecl conversions::latin1_to_utf8(const std::string &s)
{
    return utf16_to_utf8(latin1_to_utf16(s));
}

#ifndef _UTF16_STRINGS
utility::string_t __cdecl conversions::to_string_t(utf16string &&s)
{
    return utf16_to_utf8(std::move(s));
}
#endif

#ifdef _UTF16_STRINGS
utility::string_t __cdecl conversions::to_string_t(std::string &&s)
{
    return utf8_to_utf16(std::move(s));
}
#endif

#ifndef _UTF16_STRINGS
utility::string_t __cdecl conversions::to_string_t(const utf16string &s)
{
    return utf16_to_utf8(s);
}
#endif

#ifdef _UTF16_STRINGS
utility::string_t __cdecl conversions::to_string_t(const std::string &s)
{
    return utf8_to_utf16(s);
}
#endif

std::string __cdecl conversions::to_utf8string(const utf16string &value) { return utf16_to_utf8(value); }

utf16string __cdecl conversions::to_utf16string(const std::string &value) { return utf8_to_utf16(value); }

#ifndef WIN32
datetime datetime::timeval_to_datetime(const timeval &time)
{
    const uint64_t epoch_offset = 11644473600LL; // diff between windows and unix epochs (seconds)
    uint64_t result = epoch_offset + time.tv_sec;
    result *= _secondTicks; // convert to 10e-7
    result += time.tv_usec * 10; // convert and add microseconds, 10e-6 to 10e-7
    return datetime(result);
}
#endif

static bool is_digit(utility::char_t c) { return c >= _XPLATSTR('0') && c <= _XPLATSTR('9'); }

datetime __cdecl datetime::utc_now()
{
#ifdef _WIN32
    ULARGE_INTEGER largeInt;
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);

    largeInt.LowPart = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;

    return datetime(largeInt.QuadPart);
#else //LINUX
    struct timeval time;
    gettimeofday(&time, nullptr);
    return timeval_to_datetime(time);
#endif
}

utility::string_t datetime::to_string(date_format format) const
{
#ifdef _WIN32
    int status;

    ULARGE_INTEGER largeInt;
    largeInt.QuadPart = m_interval;

    FILETIME ft;
    ft.dwHighDateTime = largeInt.HighPart;
    ft.dwLowDateTime = largeInt.LowPart;

    SYSTEMTIME systemTime;
    if (!FileTimeToSystemTime((const FILETIME *)&ft, &systemTime))
    {
        throw utility::details::create_system_error(GetLastError());
    }

    std::wostringstream outStream;
    outStream.imbue(std::locale::classic());

    if (format == RFC_1123)
    {
#if _WIN32_WINNT < _WIN32_WINNT_VISTA
        TCHAR dateStr[18] = {0};
        status = GetDateFormat(LOCALE_INVARIANT, 0, &systemTime, __TEXT("ddd',' dd MMM yyyy"), dateStr, sizeof(dateStr) / sizeof(TCHAR));
#else
        wchar_t dateStr[18] = {0};
        status = GetDateFormatEx(LOCALE_NAME_INVARIANT, 0, &systemTime, L"ddd',' dd MMM yyyy", dateStr, sizeof(dateStr) / sizeof(wchar_t), NULL);
#endif // _WIN32_WINNT < _WIN32_WINNT_VISTA
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
        TCHAR timeStr[10] = {0};
        status = GetTimeFormat(LOCALE_INVARIANT, TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &systemTime, __TEXT("HH':'mm':'ss"), timeStr, sizeof(timeStr) / sizeof(TCHAR));
#else
        wchar_t timeStr[10] = {0};
        status = GetTimeFormatEx(LOCALE_NAME_INVARIANT, TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &systemTime, L"HH':'mm':'ss", timeStr, sizeof(timeStr) / sizeof(wchar_t));
#endif // _WIN32_WINNT < _WIN32_WINNT_VISTA
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

        outStream << dateStr << " " << timeStr << " " << "GMT";
    }
    else if (format == ISO_8601)
    {
        const size_t buffSize = 64;
#if _WIN32_WINNT < _WIN32_WINNT_VISTA
        TCHAR dateStr[buffSize] = {0};
        status = GetDateFormat(LOCALE_INVARIANT, 0, &systemTime, __TEXT("yyyy-MM-dd"), dateStr, buffSize);
#else
        wchar_t dateStr[buffSize] = {0};
        status = GetDateFormatEx(LOCALE_NAME_INVARIANT, 0, &systemTime, L"yyyy-MM-dd", dateStr, buffSize, NULL);
#endif // _WIN32_WINNT < _WIN32_WINNT_VISTA
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
        TCHAR timeStr[buffSize] = {0};
        status = GetTimeFormat(LOCALE_INVARIANT, TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &systemTime, __TEXT("HH':'mm':'ss"), timeStr, buffSize);
#else
        wchar_t timeStr[buffSize] = {0};
        status = GetTimeFormatEx(LOCALE_NAME_INVARIANT, TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &systemTime, L"HH':'mm':'ss", timeStr, buffSize);
#endif // _WIN32_WINNT < _WIN32_WINNT_VISTA
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

        outStream << dateStr << "T" << timeStr;
        uint64_t frac_sec = largeInt.QuadPart % _secondTicks;
        if (frac_sec > 0)
        {
            // Append fractional second, which is a 7-digit value with no trailing zeros
            // This way, '1200' becomes '00012'
            char buf[9] = { 0 };
            sprintf_s(buf, sizeof(buf), ".%07ld", (long int)frac_sec);
            // trim trailing zeros
            for (int i = 7; buf[i] == '0'; i--) buf[i] = '\0';
            outStream << buf;
        }
        outStream << "Z";
    }

    return outStream.str();
#else //LINUX
    uint64_t input = m_interval;
    uint64_t frac_sec = input % _secondTicks;
    input /= _secondTicks; // convert to seconds
    time_t time = (time_t)input - (time_t)11644473600LL;// diff between windows and unix epochs (seconds)

    struct tm datetime;
    gmtime_r(&time, &datetime);

    const int max_dt_length = 64;
    char output[max_dt_length+1] = {0};

    if (format != RFC_1123 && frac_sec > 0)
    {
        // Append fractional second, which is a 7-digit value with no trailing zeros
        // This way, '1200' becomes '00012'
        char buf[9] = { 0 };
        snprintf(buf, sizeof(buf), ".%07ld", (long int)frac_sec);
        // trim trailing zeros
        for (int i = 7; buf[i] == '0'; i--) buf[i] = '\0';
        // format the datetime into a separate buffer
        char datetime_str[max_dt_length+1] = {0};
        strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%dT%H:%M:%S", &datetime);
        // now print this buffer into the output buffer
        snprintf(output, sizeof(output), "%s%sZ", datetime_str, buf);
    }
    else
    {
        strftime(output, sizeof(output),
            format == RFC_1123 ? "%a, %d %b %Y %H:%M:%S GMT" : "%Y-%m-%dT%H:%M:%SZ",
            &datetime);
    }

    return std::string(output);
#endif
}

#ifdef _WIN32
bool __cdecl datetime::system_type_to_datetime(void* pvsysTime, uint64_t seconds, datetime * pdt)
{
    SYSTEMTIME* psysTime = (SYSTEMTIME*)pvsysTime;
    FILETIME fileTime;

    if (SystemTimeToFileTime(psysTime, &fileTime))
    {
        ULARGE_INTEGER largeInt;
        largeInt.LowPart = fileTime.dwLowDateTime;
        largeInt.HighPart = fileTime.dwHighDateTime;

        // Add hundredths of nanoseconds
        largeInt.QuadPart += seconds;

        *pdt = datetime(largeInt.QuadPart);
        return true;
    }
    return false;
}
#endif

// Take a string that represents a fractional second and return the number of ticks
// This is equivalent to doing atof on the string and multiplying by 10000000,
// but does not lose precision
template<typename StringIterator>
uint64_t timeticks_from_second(StringIterator begin, StringIterator end)
{
    int size = (int)(end - begin);
    _ASSERTE(begin[0] == U('.'));
    uint64_t ufrac_second = 0;
    for (int i = 1; i <= 7; ++i)
    {
        ufrac_second *= 10;
        int add = i < size ? begin[i] - U('0') : 0;
        ufrac_second += add;
    }
    return ufrac_second;
}

void extract_fractional_second(const utility::string_t& dateString, utility::string_t& resultString, uint64_t& ufrac_second)
{
    resultString = dateString;
    // First, the string must be strictly longer than 2 characters, and the trailing character must be 'Z'
    if (resultString.size() > 2 && resultString[resultString.size() - 1] == U('Z'))
    {
        // Second, find the last non-digit by scanning the string backwards
        auto last_non_digit = std::find_if_not(resultString.rbegin() + 1, resultString.rend(), is_digit);
        if (last_non_digit < resultString.rend() - 1)
        {
            // Finally, make sure the last non-digit is a dot:
            auto last_dot = last_non_digit.base() - 1;
            if (*last_dot == U('.'))
            {
                // Got it! Now extract the fractional second
                auto last_before_Z = std::end(resultString) - 1;
                ufrac_second = timeticks_from_second(last_dot, last_before_Z);
                // And erase it from the string
                resultString.erase(last_dot, last_before_Z);
            }
        }
    }
}

datetime __cdecl datetime::from_string(const utility::string_t& dateString, date_format format)
{
    // avoid floating point math to preserve precision
    uint64_t ufrac_second = 0;

#ifdef _WIN32
    datetime result;
    if (format == RFC_1123)
    {
        SYSTEMTIME sysTime = {0};

        std::wstring month(3, L'\0');
        std::wstring unused(3, L'\0');

        const wchar_t * formatString = L"%3c, %2d %3c %4d %2d:%2d:%2d %3c";
        auto n = swscanf_s(dateString.c_str(), formatString,
            unused.data(), unused.size(),
            &sysTime.wDay,
            month.data(), month.size(),
            &sysTime.wYear,
            &sysTime.wHour,
            &sysTime.wMinute,
            &sysTime.wSecond,
            unused.data(), unused.size());

        if (n == 8)
        {
            std::wstring monthnames[12] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
            auto loc = std::find_if(monthnames, monthnames+12, [&month](const std::wstring& m) { return m == month;});

            if (loc != monthnames+12)
            {
                sysTime.wMonth = (short) ((loc - monthnames) + 1);
                if (system_type_to_datetime(&sysTime, ufrac_second, &result))
                {
                    return result;
                }
            }
        }
    }
    else if (format == ISO_8601)
    {
        // Unlike FILETIME, SYSTEMTIME does not have enough precision to hold seconds in 100 nanosecond
        // increments. Therefore, start with seconds and milliseconds set to 0, then add them separately

        // Try to extract the fractional second from the timestamp
        utility::string_t input;
        extract_fractional_second(dateString, input, ufrac_second);
        {
            SYSTEMTIME sysTime = { 0 };
            const wchar_t * formatString = L"%4d-%2d-%2dT%2d:%2d:%2dZ";
            auto n = swscanf_s(input.c_str(), formatString,
                &sysTime.wYear,
                &sysTime.wMonth,
                &sysTime.wDay,
                &sysTime.wHour,
                &sysTime.wMinute,
                &sysTime.wSecond);

            if (n == 3 || n == 6)
            {
                if (system_type_to_datetime(&sysTime, ufrac_second, &result))
                {
                    return result;
                }
            }
        }
        {
            SYSTEMTIME sysTime = {0};
            DWORD date = 0;

            const wchar_t * formatString = L"%8dT%2d:%2d:%2dZ";
            auto n = swscanf_s(input.c_str(), formatString,
                &date,
                &sysTime.wHour,
                &sysTime.wMinute,
                &sysTime.wSecond);

            if (n == 1 || n == 4)
            {
                sysTime.wDay = date % 100;
                date /= 100;
                sysTime.wMonth = date % 100;
                date /= 100;
                sysTime.wYear = (WORD)date;

                if (system_type_to_datetime(&sysTime, ufrac_second, &result))
                {
                    return result;
                }
            }
        }
        {
            SYSTEMTIME sysTime = {0};
            GetSystemTime(&sysTime);    // Fill date portion with today's information
            sysTime.wSecond = 0;
            sysTime.wMilliseconds = 0;

            const wchar_t * formatString = L"%2d:%2d:%2dZ";
            auto n = swscanf_s(input.c_str(), formatString,
                &sysTime.wHour,
                &sysTime.wMinute,
                &sysTime.wSecond);

            if (n == 3)
            {
                if (system_type_to_datetime(&sysTime, ufrac_second, &result))
                {
                    return result;
                }
            }
        }
    }

    return datetime();
#else
    std::string input(dateString);

    struct tm output = tm();

    if (format == RFC_1123)
    {
        strptime(input.data(), "%a, %d %b %Y %H:%M:%S GMT", &output);
    }
    else
    {
        // Try to extract the fractional second from the timestamp
        utility::string_t input;
        extract_fractional_second(dateString, input, ufrac_second);

        auto result = strptime(input.data(), "%Y-%m-%dT%H:%M:%SZ", &output);

        if (result == nullptr)
        {
            result = strptime(input.data(), "%Y%m%dT%H:%M:%SZ", &output);
        }
        if (result == nullptr)
        {
            // Fill the date portion with the epoch,
            // strptime will do the rest
            memset(&output, 0, sizeof(struct tm));
            output.tm_year = 70;
            output.tm_mon = 1;
            output.tm_mday = 1;
            result = strptime(input.data(), "%H:%M:%SZ", &output);
        }
        if (result == nullptr)
        {
            result = strptime(input.data(), "%Y-%m-%d", &output);
        }
        if (result == nullptr)
        {
            result = strptime(input.data(), "%Y%m%d", &output);
        }
        if (result == nullptr)
        {
            return datetime();
        }
    }

#if (defined(ANDROID) || defined(__ANDROID__))
    // HACK: The (nonportable?) POSIX function timegm is not available in
    //       bionic. As a workaround[1][2], we set the C library timezone to
    //       UTC, call mktime, then set the timezone back. However, the C
    //       environment is fundamentally a shared global resource and thread-
    //       unsafe. We can protect our usage here, however any other code might
    //       manipulate the environment at the same time.
    //
    // [1] http://linux.die.net/man/3/timegm
    // [2] http://www.gnu.org/software/libc/manual/html_node/Broken_002ddown-Time.html
    time_t time;

    static boost::mutex env_var_lock;
    {
        boost::lock_guard<boost::mutex> lock(env_var_lock);
        std::string prev_env;
        auto prev_env_cstr = getenv("TZ");
        if (prev_env_cstr != nullptr)
        {
            prev_env = prev_env_cstr;
        }
        setenv("TZ", "UTC", 1);

        time = mktime(&output);

        if (prev_env_cstr)
        {
            setenv("TZ", prev_env.c_str(), 1);
        }
        else
        {
            unsetenv("TZ");
        }
    }
#else
    time_t time = timegm(&output);
#endif

    struct timeval tv = timeval();
    tv.tv_sec = time;
    auto result = timeval_to_datetime(tv);

    // fractional seconds are already in correct format so just add them.
    result = result + ufrac_second;
    return result;
#endif
}

/// <summary>
/// Converts a timespan/interval in seconds to xml duration string as specified by
/// http://www.w3.org/TR/xmlschema-2/#duration
/// </summary>
utility::string_t __cdecl timespan::seconds_to_xml_duration(utility::seconds durationSecs)
{
    auto numSecs = durationSecs.count();

    // Find the number of minutes
    auto numMins =  numSecs / 60;
    if (numMins > 0)
    {
        numSecs = numSecs % 60;
    }

    // Hours
    auto numHours = numMins / 60;
    if (numHours > 0)
    {
        numMins = numMins % 60;
    }

    // Days
    auto numDays = numHours / 24;
    if (numDays > 0)
    {
        numHours = numHours % 24;
    }

    // The format is:
    // PdaysDThoursHminutesMsecondsS
    utility::ostringstream_t oss;
    oss.imbue(std::locale::classic());

    oss << _XPLATSTR("P");
    if (numDays > 0)
    {
        oss << numDays << _XPLATSTR("D");
    }

    oss << _XPLATSTR("T");

    if (numHours > 0)
    {
        oss << numHours << _XPLATSTR("H");
    }

    if (numMins > 0)
    {
        oss << numMins << _XPLATSTR("M");
    }

    if (numSecs > 0)
    {
        oss << numSecs << _XPLATSTR("S");
    }

    return oss.str();
}

utility::seconds __cdecl timespan::xml_duration_to_seconds(const utility::string_t &timespanString)
{
    // The format is:
    // PnDTnHnMnS
    // if n == 0 then the field could be omitted
    // The final S could be omitted

    int64_t numSecs = 0;

    utility::istringstream_t is(timespanString);
    is.imbue(std::locale::classic());
    auto eof = std::char_traits<utility::char_t>::eof();

    std::basic_istream<utility::char_t>::int_type c;
    c = is.get(); // P

    while (c != eof)
    {
        int val = 0;
        c = is.get();

        while (is_digit((utility::char_t)c))
        {
            val = val * 10 + (c - L'0');
            c = is.get();

            if (c == '.')
            {
                // decimal point is not handled
                do { c = is.get(); } while(is_digit((utility::char_t)c));
            }
        }

        if (c == L'D') numSecs += val * 24 * 3600; // days
        if (c == L'H') numSecs += val * 3600; // Hours
        if (c == L'M') numSecs += val * 60; // Minutes
        if (c == L'S' || c == eof)
        {
            numSecs += val; // seconds
            break;
        }
    }

    return utility::seconds(numSecs);
}

const utility::string_t nonce_generator::c_allowed_chars(_XPLATSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));

utility::string_t nonce_generator::generate()
{
    std::uniform_int_distribution<> distr(0, static_cast<int>(c_allowed_chars.length() - 1));
    utility::string_t result;
    result.reserve(length());
    std::generate_n(std::back_inserter(result), length(), [&]() { return c_allowed_chars[distr(m_random)]; } );
    return result;
}


}
