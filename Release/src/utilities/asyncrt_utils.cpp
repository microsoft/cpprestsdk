/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Utilities
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#include "stdafx.h"

#include <cpprest/asyncrt_utils.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <time.h>

#if defined(_WIN32)
#include "windows_config.h"
#endif

using namespace utility;

namespace utility
{
namespace details
{
const std::error_category& __cdecl platform_category()
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
const std::error_category& __cdecl windows_category()
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

    std::wstring buffer(buffer_size, 0);

    const auto result = ::FormatMessageW(dwFlags, lpSource, errorCode, 0, &buffer[0], buffer_size, NULL);

    if (result == 0)
    {
        return "Unable to get an error message for error code: " + std::to_string(errorCode) + ".";
    }

    // strip exceeding characters of the initial resize call
    buffer.resize(result);

    return utility::conversions::to_utf8string(buffer);
}

std::error_condition windows_category_impl::default_error_condition(int errorCode) const CPPREST_NOEXCEPT
{
    // First see if the STL implementation can handle the mapping for common cases.
    const std::error_condition errCondition = std::system_category().default_error_condition(errorCode);
    const std::string errConditionMsg = errCondition.message();
    if (!utility::details::str_iequal(errConditionMsg, "unknown error"))
    {
        return errCondition;
    }

    switch (errorCode)
    {
#ifndef __cplusplus_winrt
        case ERROR_WINHTTP_TIMEOUT: return std::errc::timed_out;
        case ERROR_WINHTTP_CANNOT_CONNECT: return std::errc::host_unreachable;
        case ERROR_WINHTTP_CONNECTION_ERROR: return std::errc::connection_aborted;
#endif
        case INET_E_RESOURCE_NOT_FOUND:
        case INET_E_CANNOT_CONNECT: return std::errc::host_unreachable;
        case INET_E_CONNECTION_TIMEOUT: return std::errc::timed_out;
        case INET_E_DOWNLOAD_FAILURE: return std::errc::connection_aborted;
        default: break;
    }

    return std::error_condition(errorCode, *this);
}

#else

const std::error_category& __cdecl linux_category()
{
    // On Linux we are using boost error codes which have the exact same
    // mapping and are equivalent with std::generic_category error codes.
    return std::generic_category();
}

#endif

} // namespace details


static const int64_t NtToUnixOffsetSeconds = 11644473600; // diff between windows and unix epochs (seconds)

static bool year_is_leap_year(int year) { return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); }

static const int SecondsInMinute = 60;
static const int SecondsInHour = SecondsInMinute * 60;
static const int SecondsInDay = SecondsInHour * 24;

static const int DaysInYear = 365;
static const int DaysIn4Years = DaysInYear * 4 + 1;
static const int DaysIn100Years = DaysIn4Years * 25 - 1;
static const int DaysIn400Years = DaysIn100Years * 4 + 1;

static const int SecondsInYear = SecondsInDay * DaysInYear;
static const int SecondsIn4Years = SecondsInDay * DaysIn4Years;
static const int64_t SecondsIn100Years = static_cast<int64_t>(SecondsInDay) * DaysIn100Years;
static const int64_t SecondsIn400Years = static_cast<int64_t>(SecondsInDay) * DaysIn400Years;
static const int64_t SecondsFrom1900To2001 = INT64_C(3187296000);

static const int64_t NtTo1900OffsetInterval = INT64_C(0x014F373BFDE04000);

static int count_leap_years(const int yearsSince1900)
{
    int tmpYears = yearsSince1900 + 299; // shift into 1601, the first 400 year cycle including 1900

    int year400 = tmpYears / 400;
    tmpYears -= year400 * 400;
    int result = year400 * 97;

    int year100 = tmpYears / 100;
    tmpYears -= year100 * 100;
    result += year100 * 24;

    result += tmpYears / 4;

    // subtract off leap years from 1601
    result -= 72;

    return result;
}

// The following table assumes no leap year; leap year is added separately
static const unsigned short cumulative_days_to_month[12] = {
    0,   // Jan
    31,  // Feb
    59,  // Mar
    90,  // Apr
    120, // May
    151, // Jun
    181, // Jul
    212, // Aug
    243, // Sep
    273, // Oct
    304, // Nov
    334  // Dec
};

static const unsigned short cumulative_days_to_month_leap[12] = {
    0,   // Jan
    31,  // Feb
    60,  // Mar
    91,  // Apr
    121, // May
    152, // Jun
    182, // Jul
    213, // Aug
    244, // Sep
    274, // Oct
    305, // Nov
    335  // Dec
};

datetime __cdecl datetime::utc_now()
{
#ifdef _WIN32
    ULARGE_INTEGER largeInt;
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);

    largeInt.LowPart = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;

    return datetime(largeInt.QuadPart);
#else // LINUX
    struct timeval time;
    gettimeofday(&time, nullptr);
    int64_t result = NtToUnixOffsetSeconds + time.tv_sec;
    result *= _secondTicks;      // convert to 10e-7
    result += time.tv_usec * 10; // convert and add microseconds, 10e-6 to 10e-7
    return datetime(static_cast<interval_type>(result));
#endif
}

static const char dayNames[] = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
static const char monthNames[] = "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec";

struct compute_year_result
{
    int year;
    int secondsLeftThisYear;
};

static const int64_t secondsFrom1601To1900 = INT64_C(9435484800);

static compute_year_result compute_year(int64_t secondsSince1900)
{
    int64_t secondsLeft = secondsSince1900 + secondsFrom1601To1900; // shift to start of this 400 year cycle

    int year400 = static_cast<int>(secondsLeft / SecondsIn400Years);
    secondsLeft -= year400 * SecondsIn400Years;

    int year100 = static_cast<int>(secondsLeft / SecondsIn100Years);
    secondsLeft -= year100 * SecondsIn100Years;

    int year4 = static_cast<int>(secondsLeft / SecondsIn4Years);
    int secondsInt = static_cast<int>(secondsLeft - year4 * SecondsIn4Years);

    int year1 = secondsInt / SecondsInYear;
    secondsInt -= year1 * SecondsInYear;

    // shift back to 1900 base from 1601:
    return {year400 * 400 + year100 * 100 + year4 * 4 + year1 - 299, secondsInt};
}

utility::string_t datetime::to_string(date_format format) const
{
    if (m_interval > INT64_C(2650467743990000000))
    {
        throw std::out_of_range("The requested year exceeds the year 9999.");
    }

    const int64_t epochAdjusted = static_cast<int64_t>(m_interval) - NtTo1900OffsetInterval;
    const int64_t secondsSince1900 = epochAdjusted / _secondTicks; // convert to seconds
    const int fracSec = static_cast<int>(epochAdjusted % _secondTicks);

    const auto yearData = compute_year(secondsSince1900);
    const int year = yearData.year;
    const int yearDay = yearData.secondsLeftThisYear / SecondsInDay;
    int leftover = yearData.secondsLeftThisYear % SecondsInDay;
    const int hour = leftover / SecondsInHour;
    leftover = leftover % SecondsInHour;
    const int minute = leftover / SecondsInMinute;
    leftover = leftover % SecondsInMinute;

    const auto& monthTable = year_is_leap_year(year) ? cumulative_days_to_month_leap : cumulative_days_to_month;
    int month = 0;
    while (month < 11 && monthTable[month + 1] <= yearDay)
    {
        ++month;
    }

    const auto monthDay = yearDay - monthTable[month] + 1;
    const auto weekday = static_cast<int>((secondsSince1900 / SecondsInDay + 1) % 7);

    char outBuffer[38]; // Thu, 01 Jan 1970 00:00:00 GMT\0
                        // 1970-01-01T00:00:00.1234567Z\0
    char* outCursor = outBuffer;
    switch (format)
    {
        case RFC_1123:
#ifdef _MSC_VER
            sprintf_s(outCursor,
                      26,
                      "%s, %02d %s %04d %02d:%02d:%02d",
                      dayNames + 4 * weekday,
                      monthDay,
                      monthNames + 4 * month,
                      year + 1900,
                      hour,
                      minute,
                      leftover);
#else  // ^^^ _MSC_VER // !_MSC_VER vvv
            sprintf(outCursor,
                    "%s, %02d %s %04d %02d:%02d:%02d",
                    dayNames + 4 * weekday,
                    monthDay,
                    monthNames + 4 * month,
                    year + 1900,
                    hour,
                    minute,
                    leftover);
#endif // _MSC_VER
            outCursor += 25;
            memcpy(outCursor, " GMT", 4);
            outCursor += 4;
            return utility::string_t(outBuffer, outCursor);
        case ISO_8601:
#ifdef _MSC_VER
            sprintf_s(outCursor,
                      20,
                      "%04d-%02d-%02dT%02d:%02d:%02d",
                      year + 1900,
                      month + 1,
                      monthDay,
                      hour,
                      minute,
                      leftover);
#else  // ^^^ _MSC_VER // !_MSC_VER vvv
            sprintf(
                outCursor, "%04d-%02d-%02dT%02d:%02d:%02d", year + 1900, month + 1, monthDay, hour, minute, leftover);
#endif // _MSC_VER
            outCursor += 19;
            if (fracSec != 0)
            {
                // Append fractional second, which is a 7-digit value with no trailing zeros
                // This way, '1200' becomes '00012'
#ifdef _MSC_VER
                size_t appended = sprintf_s(outCursor, 9, ".%07d", fracSec);
#else  // ^^^ _MSC_VER // !_MSC_VER vvv
                size_t appended = sprintf(outCursor, ".%07d", fracSec);
#endif // _MSC_VER
                while (outCursor[appended - 1] == '0')
                {
                    --appended; // trim trailing zeros
                }

                outCursor += appended;
            }

            *outCursor = 'Z';
            ++outCursor;
            return utility::string_t(outBuffer, outCursor);
        default: throw std::invalid_argument("Unrecognized date format.");
    }
}

template<class CharT>
static bool string_starts_with(const CharT* haystack, const char* needle)
{
    while (*needle)
    {
        if (*haystack != static_cast<CharT>(*needle))
        {
            return false;
        }

        ++haystack;
        ++needle;
    }

    return true;
}

#define ascii_isdigit(c) ((unsigned char)((unsigned char)(c) - '0') <= 9)
#define ascii_isdigit6(c) ((unsigned char)((unsigned char)(c) - '0') <= 6)
#define ascii_isdigit5(c) ((unsigned char)((unsigned char)(c) - '0') <= 5)
#define ascii_isdigit3(c) ((unsigned char)((unsigned char)(c) - '0') <= 3)
#define ascii_isdigit2(c) ((unsigned char)((unsigned char)(c) - '0') <= 2)
#define ascii_isdigit1(c) ((unsigned char)((unsigned char)(c) - '0') <= 1)

static const unsigned char max_days_in_month[12] = {
    31, // Jan
    00, // Feb, special handling for leap years
    31, // Mar
    30, // Apr
    31, // May
    30, // Jun
    31, // Jul
    31, // Aug
    30, // Sep
    31, // Oct
    30, // Nov
    31  // Dec
};

static bool validate_day_month(int day, int month, int year)
{
    int maxDaysThisMonth;
    if (month == 1)
    { // Feb needs leap year testing
        maxDaysThisMonth = 28 + year_is_leap_year(year);
    }
    else
    {
        maxDaysThisMonth = max_days_in_month[month];
    }

    return day >= 1 && day <= maxDaysThisMonth;
}

static int get_year_day(int month, int monthDay, int year)
{
    return cumulative_days_to_month[month] + monthDay + (year_is_leap_year(year) && month > 1) - 1;
}

template<class CharT>
static int atoi2(const CharT* str)
{
    return (static_cast<unsigned char>(str[0]) - '0') * 10 + (static_cast<unsigned char>(str[1]) - '0');
}

static int64_t timezone_adjust(int64_t result, unsigned char chSign, int adjustHours, int adjustMinutes)
{
    if (adjustHours > 23)
    {
        return -1;
    }

    // adjustMinutes > 59 is impossible due to digit 5 check
    const int tzAdjust = adjustMinutes * 60 + adjustHours * 60 * 60;
    if (chSign == '-')
    {
        if (INT64_MAX - result < tzAdjust)
        {
            return -1;
        }

        result += tzAdjust;
    }
    else
    {
        if (tzAdjust > result)
        {
            return -1;
        }

        result -= tzAdjust;
    }

    return result;
}

/*
https://tools.ietf.org/html/rfc822
https://tools.ietf.org/html/rfc1123

date-time   =  [ day "," ] date time        ; dd mm yy
                                            ;  hh:mm:ss zzz

day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
            /  "Fri"  / "Sat" /  "Sun"

date        =  1*2DIGIT month 2DIGIT        ; day month year
                                            ;  e.g. 20 Jun 82
RFC1123 changes this to:
date        =  1*2DIGIT month 2*4DIGIT        ; day month year
                                              ;  e.g. 20 Jun 1982
This implementation only accepts 4 digit years.

month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
            /  "May"  /  "Jun" /  "Jul"  /  "Aug"
            /  "Sep"  /  "Oct" /  "Nov"  /  "Dec"

time        =  hour zone                    ; ANSI and Military

hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
                                            ; 00:00:00 - 23:59:59

zone        =  "UT"  / "GMT"                ; Universal Time
                                            ; North American : UT
            /  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
            /  "CST" / "CDT"                ;  Central:  - 6/ - 5
            /  "MST" / "MDT"                ;  Mountain: - 7/ - 6
            /  "PST" / "PDT"                ;  Pacific:  - 8/ - 7

// military time deleted by RFC 1123

            / ( ("+" / "-") 4DIGIT )        ; Local differential
                                            ;  hours+min. (HHMM)
*/


datetime __cdecl datetime::from_string(const utility::string_t& dateString, date_format format)
{
    datetime result;
    int64_t secondsSince1900;
    uint64_t fracSec = 0;
    auto str = dateString.c_str();
    if (format == RFC_1123)
    {
        int parsedWeekday = 0;
        for (; parsedWeekday < 7; ++parsedWeekday)
        {
            if (string_starts_with(str, dayNames + parsedWeekday * 4) && str[3] == _XPLATSTR(',') &&
                str[4] == _XPLATSTR(' '))
            {
                str += 5; // parsed day of week
                break;
            }
        }

        int monthDay;
        if (ascii_isdigit3(str[0]) && ascii_isdigit(str[1]) && str[2] == _XPLATSTR(' '))
        {
            monthDay = atoi2(str); // validity checked later
            str += 3;              // parsed day
        }
        else if (ascii_isdigit(str[0]) && str[1] == _XPLATSTR(' '))
        {
            monthDay = str[0] - _XPLATSTR('0');
            str += 2; // parsed day
        }
        else
        {
            return result;
        }

        if (monthDay == 0)
        {
            return result;
        }

        int month = 0;
        for (;;)
        {
            if (string_starts_with(str, monthNames + month * 4))
            {
                break;
            }

            ++month;
            if (month == 12)
            {
                return result;
            }
        }

        if (str[3] != _XPLATSTR(' '))
        {
            return result;
        }

        str += 4; // parsed month

        if (!ascii_isdigit(str[0]) || !ascii_isdigit(str[1]) || !ascii_isdigit(str[2]) || !ascii_isdigit(str[3]) ||
            str[4] != ' ')
        {
            return result;
        }

        int year = (str[0] - _XPLATSTR('0')) * 1000 + (str[1] - _XPLATSTR('0')) * 100 + (str[2] - _XPLATSTR('0')) * 10 +
                   (str[3] - _XPLATSTR('0'));
        if (year < 1900)
        {
            return result;
        }

        // days in month validity check
        if (!validate_day_month(monthDay, month, year))
        {
            return result;
        }

        str += 5; // parsed year
        const int yearDay = get_year_day(month, monthDay, year);

        if (!ascii_isdigit2(str[0]) || !ascii_isdigit(str[1]) || str[2] != _XPLATSTR(':') || !ascii_isdigit5(str[3]) ||
            !ascii_isdigit(str[4]))
        {
            return result;
        }

        const int hour = atoi2(str);
        if (hour > 23)
        {
            return result;
        }

        str += 3; // parsed hour
        const int minute = atoi2(str);
        str += 2; // parsed mins

        int sec;
        if (str[0] == ':')
        {
            if (!ascii_isdigit6(str[1]) || !ascii_isdigit(str[2]) || str[3] != _XPLATSTR(' '))
            {
                return result;
            }

            sec = atoi2(str + 1);
            str += 4; // parsed seconds
        }
        else if (str[0] == _XPLATSTR(' '))
        {
            sec = 0;
            str += 1; // parsed seconds
        }
        else
        {
            return result;
        }

        if (sec > 60)
        { // 60 to allow leap seconds
            return result;
        }

        year -= 1900;
        int daysSince1900 = year * DaysInYear + count_leap_years(year) + yearDay;

        if (parsedWeekday != 7)
        {
            const int actualWeekday = (daysSince1900 + 1) % 7;

            if (parsedWeekday != actualWeekday)
            {
                return result;
            }
        }

        secondsSince1900 =
            static_cast<int64_t>(daysSince1900) * SecondsInDay + hour * SecondsInHour + minute * SecondsInMinute + sec;

        if (!string_starts_with(str, "GMT") && !string_starts_with(str, "UT"))
        {
            // some timezone adjustment necessary
            auto tzCh = _XPLATSTR('-');
            int tzHours;
            int tzMinutes = 0;
            if (string_starts_with(str, "EDT"))
            {
                tzHours = 4;
            }
            else if (string_starts_with(str, "EST") || string_starts_with(str, "CDT"))
            {
                tzHours = 5;
            }
            else if (string_starts_with(str, "CST") || string_starts_with(str, "MDT"))
            {
                tzHours = 6;
            }
            else if (string_starts_with(str, "MST") || string_starts_with(str, "PDT"))
            {
                tzHours = 7;
            }
            else if (string_starts_with(str, "PST"))
            {
                tzHours = 8;
            }
            else if ((tzCh == _XPLATSTR('+') || tzCh == _XPLATSTR('-')) && ascii_isdigit2(str[1]) &&
                     ascii_isdigit(str[2]) && ascii_isdigit5(str[3]) && ascii_isdigit(str[4]))
            {
                tzCh = str[0];
                tzHours = atoi2(str + 1);
                tzMinutes = atoi2(str + 3);
            }
            else
            {
                return result;
            }

            secondsSince1900 = timezone_adjust(secondsSince1900, static_cast<unsigned char>(tzCh), tzHours, tzMinutes);
            if (secondsSince1900 < 0)
            {
                return result;
            }
        }
    }
    else if (format == ISO_8601)
    {
        // parse year
        if (!ascii_isdigit(str[0]) || !ascii_isdigit(str[1]) || !ascii_isdigit(str[2]) || !ascii_isdigit(str[3]))
        {
            return result;
        }

        int year = (str[0] - _XPLATSTR('0')) * 1000 + (str[1] - _XPLATSTR('0')) * 100 + (str[2] - _XPLATSTR('0')) * 10 +
                   (str[3] - _XPLATSTR('0'));
        if (year < 1900)
        {
            return result;
        }

        str += 4;
        if (*str == _XPLATSTR('-'))
        {
            ++str;
        }

        // parse month
        if (!ascii_isdigit1(str[0]) || !ascii_isdigit(str[1]))
        {
            return result;
        }

        int month = atoi2(str);
        if (month < 1 || month > 12)
        {
            return result;
        }

        month -= 1;
        str += 2;

        if (*str == _XPLATSTR('-'))
        {
            ++str;
        }

        // parse day
        if (!ascii_isdigit3(str[0]) || !ascii_isdigit(str[1]))
        {
            return result;
        }

        int monthDay = atoi2(str);
        if (!validate_day_month(monthDay, month, year))
        {
            return result;
        }

        const int yearDay = get_year_day(month, monthDay, year);

        str += 2;
        year -= 1900;
        int daysSince1900 = year * DaysInYear + count_leap_years(year) + yearDay;

        if (str[0] != _XPLATSTR('T') && str[0] != _XPLATSTR('t'))
        {
            // No time
            secondsSince1900 = static_cast<int64_t>(daysSince1900) * SecondsInDay;

            result.m_interval =
                static_cast<interval_type>(secondsSince1900 * _secondTicks + fracSec + NtTo1900OffsetInterval);
            return result;
        }

        ++str; // skip 'T'

        // parse hour
        if (!ascii_isdigit2(str[0]) || !ascii_isdigit(str[1]))
        {
            return result;
        }

        const int hour = atoi2(str);
        str += 2;
        if (hour > 23)
        {
            return result;
        }

        if (*str == _XPLATSTR(':'))
        {
            ++str;
        }

        // parse minute
        if (!ascii_isdigit5(str[0]) || !ascii_isdigit(str[1]))
        {
            return result;
        }

        const int minute = atoi2(str);
        // minute > 59 is impossible because we checked that the first digit is <= 5 in the basic format
        // check above

        str += 2;

        if (*str == _XPLATSTR(':'))
        {
            ++str;
        }

        // parse seconds
        if (!ascii_isdigit6(str[0]) || !ascii_isdigit(str[1]))
        {
            return result;
        }

        const int sec = atoi2(str);
        // We allow 60 to account for leap seconds
        if (sec > 60)
        {
            return result;
        }

        str += 2;
        if (str[0] == _XPLATSTR('.') && ascii_isdigit(str[1]))
        {
            ++str;
            int digits = 7;
            for (;;)
            {
                fracSec *= 10;
                fracSec += *str - _XPLATSTR('0');
                --digits;
                ++str;
                if (digits == 0)
                {
                    while (ascii_isdigit(*str))
                    {
                        // consume remaining fractional second digits we can't use
                        ++str;
                    }

                    break;
                }

                if (!ascii_isdigit(*str))
                {
                    // no more digits in the input, do the remaining multiplies we need
                    for (; digits != 0; --digits)
                    {
                        fracSec *= 10;
                    }

                    break;
                }
            }
        }

        secondsSince1900 =
            static_cast<int64_t>(daysSince1900) * SecondsInDay + hour * SecondsInHour + minute * SecondsInMinute + sec;

        if (str[0] == _XPLATSTR('Z') || str[0] == _XPLATSTR('z'))
        {
            // no adjustment needed for zulu time
        }
        else if (str[0] == _XPLATSTR('+') || str[0] == _XPLATSTR('-'))
        {
            const unsigned char offsetDirection = static_cast<unsigned char>(str[0]);
            if (!ascii_isdigit2(str[1]) || !ascii_isdigit(str[2]) || str[3] != _XPLATSTR(':') ||
                !ascii_isdigit5(str[4]) || !ascii_isdigit(str[5]))
            {
                return result;
            }

            secondsSince1900 = timezone_adjust(secondsSince1900, offsetDirection, atoi2(str + 1), atoi2(str + 4));
            if (secondsSince1900 < 0)
            {
                return result;
            }
        }
        else
        {
            // the timezone is malformed, but cpprestsdk currently accepts this as no timezone
        }
    }
    else
    {
        throw std::invalid_argument("unrecognized date format");
    }

    result.m_interval = static_cast<interval_type>(secondsSince1900 * _secondTicks + fracSec + NtTo1900OffsetInterval);
    return result;
}

/// <summary>
/// Converts a timespan/interval in seconds to xml duration string as specified by
/// http://www.w3.org/TR/xmlschema-2/#duration
/// </summary>
utility::string_t __cdecl timespan::seconds_to_xml_duration(utility::seconds durationSecs)
{
    auto numSecs = durationSecs.count();

    // Find the number of minutes
    auto numMins = numSecs / 60;
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
    utility::string_t result;
    // (approximate mins/hours/secs as 2 digits each + 1 prefix character) + 1 for P prefix + 1 for T
    size_t baseReserveSize = ((numHours > 0) + (numMins > 0) + (numSecs > 0)) * 3 + 1;
    if (numDays > 0)
    {
        utility::string_t daysStr = utility::conversions::details::to_string_t(numDays);
        result.reserve(baseReserveSize + daysStr.size() + 1);
        result += _XPLATSTR('P');
        result += daysStr;
        result += _XPLATSTR('D');
    }
    else
    {
        result.reserve(baseReserveSize);
        result += _XPLATSTR('P');
    }

    result += _XPLATSTR('T');

    if (numHours > 0)
    {
        result += utility::conversions::details::to_string_t(numHours);
        result += _XPLATSTR('H');
    }

    if (numMins > 0)
    {
        result += utility::conversions::details::to_string_t(numMins);
        result += _XPLATSTR('M');
    }

    if (numSecs > 0)
    {
        result += utility::conversions::details::to_string_t(numSecs);
        result += _XPLATSTR('S');
    }

    return result;
}

utility::seconds __cdecl timespan::xml_duration_to_seconds(const utility::string_t& timespanString)
{
    // The format is:
    // PnDTnHnMnS
    // if n == 0 then the field could be omitted
    // The final S could be omitted

    int64_t numSecs = 0;
    auto cursor = timespanString.c_str();
    auto c = *cursor++; // skip 'P'
    while (c)
    {
        int val = 0;
        c = *cursor++;

        while (ascii_isdigit(c))
        {
            val = val * 10 + (c - _XPLATSTR('0'));
            c = *cursor++;

            if (c == _XPLATSTR('.'))
            {
                // decimal point is not handled
                do
                {
                    c = *cursor++;
                } while (ascii_isdigit(c));
            }
        }

        if (c == _XPLATSTR('D')) numSecs += val * 24 * 3600; // days
        if (c == _XPLATSTR('H')) numSecs += val * 3600;      // Hours
        if (c == _XPLATSTR('M')) numSecs += val * 60;        // Minutes
        if (c == _XPLATSTR('S') || c == _XPLATSTR('\0'))
        {
            numSecs += val; // seconds
            break;
        }
    }

    return utility::seconds(numSecs);
}

static const char c_allowed_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static const int chars_count = static_cast<int>(sizeof(c_allowed_chars) - 1);

utility::string_t nonce_generator::generate()
{
    std::uniform_int_distribution<> distr(0, chars_count - 1);
    utility::string_t result;
    result.reserve(length());
    std::generate_n(std::back_inserter(result), length(), [&] {
        return static_cast<utility::char_t>(c_allowed_chars[distr(m_random)]);
    });
    return result;
}

} // namespace utility
