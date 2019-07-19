/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Various common utilities.
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#pragma once

#include "cpprest/details/basic_types.h"
#include "cpprest/string_utils.h"
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <system_error>

/// Various utilities for string conversions and date and time manipulation.
namespace utility
{
// Left over from VS2010 support, remains to avoid breaking.
typedef std::chrono::seconds seconds;

/// Functions for converting to/from std::chrono::seconds to xml string.
namespace timespan
{
/// <summary>
/// Converts a timespan/interval in seconds to xml duration string as specified by
/// http://www.w3.org/TR/xmlschema-2/#duration
/// </summary>
_ASYNCRTIMP utility::string_t __cdecl seconds_to_xml_duration(utility::seconds numSecs);

/// <summary>
/// Converts an xml duration to timespan/interval in seconds
/// http://www.w3.org/TR/xmlschema-2/#duration
/// </summary>
_ASYNCRTIMP utility::seconds __cdecl xml_duration_to_seconds(const utility::string_t& timespanString);
} // namespace timespan

namespace details
{

#ifdef _WIN32

/// <summary>
/// Category error type for Windows OS errors.
/// </summary>
class windows_category_impl : public std::error_category
{
public:
    virtual const char* name() const CPPREST_NOEXCEPT { return "windows"; }

    virtual std::string message(int errorCode) const CPPREST_NOEXCEPT;

    virtual std::error_condition default_error_condition(int errorCode) const CPPREST_NOEXCEPT;
};

/// <summary>
/// Gets the one global instance of the windows error category.
/// </summary>
/// </returns>An error category instance.</returns>
_ASYNCRTIMP const std::error_category& __cdecl windows_category();

#else

/// <summary>
/// Gets the one global instance of the linux error category.
/// </summary>
/// </returns>An error category instance.</returns>
_ASYNCRTIMP const std::error_category& __cdecl linux_category();

#endif

/// <summary>
/// Gets the one global instance of the current platform's error category.
/// </summary>
_ASYNCRTIMP const std::error_category& __cdecl platform_category();

/// <summary>
/// Creates an instance of std::system_error from a OS error code.
/// </summary>
inline std::system_error __cdecl create_system_error(unsigned long errorCode)
{
    std::error_code code((int)errorCode, platform_category());
    return std::system_error(code, code.message());
}

/// <summary>
/// Creates a std::error_code from a OS error code.
/// </summary>
inline std::error_code __cdecl create_error_code(unsigned long errorCode)
{
    return std::error_code((int)errorCode, platform_category());
}

/// <summary>
/// Creates the corresponding error message from a OS error code.
/// </summary>
inline utility::string_t __cdecl create_error_message(unsigned long errorCode)
{
    return utility::conversions::to_string_t(create_error_code(errorCode).message());
}

} // namespace details

class datetime
{
public:
    typedef uint64_t interval_type;

    /// <summary>
    /// Defines the supported date and time string formats.
    /// </summary>
    enum date_format
    {
        RFC_1123,
        ISO_8601
    };

    /// <summary>
    /// Returns the current UTC time.
    /// </summary>
    static _ASYNCRTIMP datetime __cdecl utc_now();

    /// <summary>
    /// An invalid UTC timestamp value.
    /// </summary>
    enum : interval_type
    {
        utc_timestamp_invalid = static_cast<interval_type>(-1)
    };

    /// <summary>
    /// Returns seconds since Unix/POSIX time epoch at 01-01-1970 00:00:00.
    /// If time is before epoch, utc_timestamp_invalid is returned.
    /// </summary>
    static interval_type utc_timestamp()
    {
        const auto seconds = utc_now().to_interval() / _secondTicks;
        if (seconds >= 11644473600LL)
        {
            return seconds - 11644473600LL;
        }
        else
        {
            return utc_timestamp_invalid;
        }
    }

    datetime() : m_interval(0) {}

    /// <summary>
    /// Creates <c>datetime</c> from a string representing time in UTC in RFC 1123 format.
    /// </summary>
    /// <returns>Returns a <c>datetime</c> of zero if not successful.</returns>
    static _ASYNCRTIMP datetime __cdecl from_string(const utility::string_t& timestring, date_format format = RFC_1123);

    /// <summary>
    /// Returns a string representation of the <c>datetime</c>.
    /// </summary>
    _ASYNCRTIMP utility::string_t to_string(date_format format = RFC_1123) const;

    /// <summary>
    /// Returns the integral time value.
    /// </summary>
    interval_type to_interval() const { return m_interval; }

    datetime operator-(interval_type value) const { return datetime(m_interval - value); }

    datetime operator+(interval_type value) const { return datetime(m_interval + value); }

    bool operator==(datetime dt) const { return m_interval == dt.m_interval; }

    bool operator!=(const datetime& dt) const { return !(*this == dt); }

    static interval_type from_milliseconds(unsigned int milliseconds) { return milliseconds * _msTicks; }

    static interval_type from_seconds(unsigned int seconds) { return seconds * _secondTicks; }

    static interval_type from_minutes(unsigned int minutes) { return minutes * _minuteTicks; }

    static interval_type from_hours(unsigned int hours) { return hours * _hourTicks; }

    static interval_type from_days(unsigned int days) { return days * _dayTicks; }

    bool is_initialized() const { return m_interval != 0; }

private:
    friend int operator-(datetime t1, datetime t2);

    static const interval_type _msTicks = static_cast<interval_type>(10000);
    static const interval_type _secondTicks = 1000 * _msTicks;
    static const interval_type _minuteTicks = 60 * _secondTicks;
    static const interval_type _hourTicks = 60 * 60 * _secondTicks;
    static const interval_type _dayTicks = 24 * 60 * 60 * _secondTicks;

    // Private constructor. Use static methods to create an instance.
    datetime(interval_type interval) : m_interval(interval) {}

    // Storing as hundreds of nanoseconds 10e-7, i.e. 1 here equals 100ns.
    interval_type m_interval;
};

inline int operator-(datetime t1, datetime t2)
{
    auto diff = (t1.m_interval - t2.m_interval);

    // Round it down to seconds
    diff /= 10 * 1000 * 1000;

    return static_cast<int>(diff);
}

/// <summary>
/// Nonce string generator class.
/// </summary>
class nonce_generator
{
public:
    /// <summary>
    /// Define default nonce length.
    /// </summary>
    enum
    {
        default_length = 32
    };

    /// <summary>
    /// Nonce generator constructor.
    /// </summary>
    /// <param name="length">Length of the generated nonce string.</param>
    nonce_generator(int length = default_length)
        : m_random(static_cast<unsigned int>(utility::datetime::utc_timestamp())), m_length(length)
    {
    }

    /// <summary>
    /// Generate a nonce string containing random alphanumeric characters (A-Za-z0-9).
    /// Length of the generated string is set by length().
    /// </summary>
    /// <returns>The generated nonce string.</returns>
    _ASYNCRTIMP utility::string_t generate();

    /// <summary>
    /// Get length of generated nonce string.
    /// </summary>
    /// <returns>Nonce string length.</returns>
    int length() const { return m_length; }

    /// <summary>
    /// Set length of the generated nonce string.
    /// </summary>
    /// <param name="length">Lenght of nonce string.</param>
    void set_length(int length) { m_length = length; }

private:
    std::mt19937 m_random;
    int m_length;
};

} // namespace utility
