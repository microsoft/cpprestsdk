/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <system_error>
#include "cpprest/asyncrt_utils.h"

namespace web { namespace http {

/// <summary>
/// Binds an individual reference to a string value.
/// </summary>
/// <typeparam name="key_type">The type of string value.</typeparam>
/// <typeparam name="_t">The type of the value to bind to.</typeparam>
/// <param name="text">The string value.</param>
/// <param name="ref">The value to bind to.</param>
/// <returns><c>true</c> if the binding succeeds, <c>false</c> otherwise.</returns>
template<typename key_type, typename _t>
CASABLANCA_DEPRECATED("This API is deprecated and will be removed in a future release, std::istringstream instead.")
bool bind(const key_type &text, _t &ref) // const
{
    utility::istringstream_t iss(text);
    iss >> ref;
    if (iss.fail() || !iss.eof())
    {
        return false;
    }

    return true;
}

/// <summary>
/// Binds an individual reference to a string value.
/// This specialization is need because <c>istringstream::&gt;&gt;</c> delimits on whitespace.
/// </summary>
/// <typeparam name="key_type">The type of the string value.</typeparam>
/// <param name="text">The string value.</param>
/// <param name="ref">The value to bind to.</param>
/// <returns><c>true</c> if the binding succeeds, <c>false</c> otherwise.</returns>
template <typename key_type>
CASABLANCA_DEPRECATED("This API is deprecated and will be removed in a future release.")
bool bind(const key_type &text, utility::string_t &ref) //const
{
    ref = text;
    return true;
}

/// <summary>
/// Represents HTTP headers, acts like a map.
/// </summary>
class http_headers
{
public:
    /// Function object to perform case insensitive comparison of wstrings.
    struct _case_insensitive_cmp
    {
        bool operator()(const utility::string_t &str1, const utility::string_t &str2) const
        {
#ifdef _WIN32
            return _wcsicmp(str1.c_str(), str2.c_str()) < 0;
#else
            return utility::cmp::icmp(str1, str2) < 0;
#endif
        }
    };

    /// <summary>
    /// STL-style typedefs
    /// </summary>
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::key_type key_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::key_compare key_compare;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::allocator_type allocator_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::size_type size_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::difference_type difference_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::pointer pointer;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_pointer const_pointer;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::reference reference;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_reference const_reference;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::iterator iterator;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_iterator const_iterator;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::reverse_iterator reverse_iterator;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_reverse_iterator const_reverse_iterator;

    /// <summary>
    /// Constructs an empty set of HTTP headers.
    /// </summary>
    http_headers() {}

    /// <summary>
    /// Copy constructor.
    /// </summary>
    /// <param name="other">An <c>http_headers</c> object to copy from.</param>
    http_headers(const http_headers &other) : m_headers(other.m_headers) {}

    /// <summary>
    /// Assignment operator.
    /// </summary>
    /// <param name="other">An <c>http_headers</c> object to copy from.</param>
    http_headers &operator=(const http_headers &other)
    {
        if(this != &other)
        {
            m_headers = other.m_headers;
        }
        return *this;
    }

    /// <summary>
    /// Move constructor.
    /// </summary>
    /// <param name="other">An <c>http_headers</c> object to move.</param>
    http_headers(http_headers &&other) : m_headers(std::move(other.m_headers)) {}

    /// <summary>
    /// Move assignment operator.
    /// </summary>
    /// <param name="other">An <c>http_headers</c> object to move.</param>
    http_headers &operator=(http_headers &&other)
    {
        if(this != &other)
        {
            m_headers = std::move(other.m_headers);
        }
        return *this;
    }

    /// <summary>
    /// Adds a header field using the '&lt;&lt;' operator.
    /// </summary>
    /// <param name="name">The name of the header field.</param>
    /// <param name="value">The value of the header field.</param>
    /// <remarks>If the header field exists, the value will be combined as comma separated string.</remarks>
    template<typename _t1>
    void add(const key_type& name, const _t1& value)
    {
        if (has(name))
        {
            m_headers[name].append(_XPLATSTR(", ")).append(utility::conversions::details::print_string(value));
        }
        else
        {
            m_headers[name] = utility::conversions::details::print_string(value);
        }
    }

    /// <summary>
    /// Removes a header field.
    /// </summary>
    /// <param name="name">The name of the header field.</param>
    void remove(const key_type& name)
    {
        m_headers.erase(name);
    }

    /// <summary>
    /// Removes all elements from the headers.
    /// </summary>
    void clear() { m_headers.clear(); }

    /// <summary>
    /// Checks if there is a header with the given key.
    /// </summary>
    /// <param name="name">The name of the header field.</param>
    /// <returns><c>true</c> if there is a header with the given name, <c>false</c> otherwise.</returns>
    bool has(const key_type& name) const { return m_headers.find(name) != m_headers.end(); }

    /// <summary>
    /// Returns the number of header fields.
    /// </summary>
    /// <returns>Number of header fields.</returns>
    size_type size() const { return m_headers.size(); }

    /// <summary>
    /// Tests to see if there are any header fields.
    /// </summary>
    /// <returns><c>true</c> if there are no headers, <c>false</c> otherwise.</returns>
    bool empty() const { return m_headers.empty(); }

    /// <summary>
    /// Returns a reference to header field with given name, if there is no header field one is inserted.
    /// </summary>
    utility::string_t & operator[](const key_type &name) { return m_headers[name]; }

    /// <summary>
    /// Checks if a header field exists with given name and returns an iterator if found. Otherwise
    /// and iterator to end is returned.
    /// </summary>
    /// <param name="name">The name of the header field.</param>
    /// <returns>An iterator to where the HTTP header is found.</returns>
    iterator find(const key_type &name) { return m_headers.find(name); }
    const_iterator find(const key_type &name) const { return m_headers.find(name); }

    /// <summary>
    /// Attempts to match a header field with the given name using the '>>' operator.
    /// </summary>
    /// <param name="name">The name of the header field.</param>
    /// <param name="value">The value of the header field.</param>
    /// <returns><c>true</c> if header field was found and successfully stored in value parameter.</returns>
    template<typename _t1>
    bool match(const key_type &name, _t1 &value) const
    {
        auto iter = m_headers.find(name);
        if (iter != m_headers.end())
        {
            // Check to see if doesn't have a value.
            if(iter->second.empty())
            {
                bind_impl(iter->second, value);
                return true;
            }
            return bind_impl(iter->second, value);
        }
        else
        {
            return false;
        }
    }

    /// <summary>
    /// Returns an iterator referring to the first header field.
    /// </summary>
    /// <returns>An iterator to the beginning of the HTTP headers</returns>
    iterator begin() { return m_headers.begin(); }
    const_iterator begin() const { return m_headers.begin(); }

    /// <summary>
    /// Returns an iterator referring to the past-the-end header field.
    /// </summary>
    /// <returns>An iterator to the element past the end of the HTTP headers.</returns>
    iterator end() { return m_headers.end(); }
    const_iterator end() const { return m_headers.end(); }

    /// <summary>
    /// Gets the content length of the message.
    /// </summary>
    /// <returns>The length of the content.</returns>
    _ASYNCRTIMP utility::size64_t content_length() const;

    /// <summary>
    /// Sets the content length of the message.
    /// </summary>
    /// <param name="length">The length of the content.</param>
    _ASYNCRTIMP void set_content_length(utility::size64_t length);

    /// <summary>
    /// Gets the content type of the message.
    /// </summary>
    /// <returns>The content type of the body.</returns>
    _ASYNCRTIMP utility::string_t content_type() const;

    /// <summary>
    /// Sets the content type of the message.
    /// </summary>
    /// <param name="type">The content type of the body.</param>
    _ASYNCRTIMP void set_content_type(utility::string_t type);

    /// <summary>
    /// Gets the cache control header of the message.
    /// </summary>
    /// <returns>The cache control header value.</returns>
    _ASYNCRTIMP utility::string_t cache_control() const;

    /// <summary>
    /// Sets the cache control header of the message.
    /// </summary>
    /// <param name="control">The cache control header value.</param>
    _ASYNCRTIMP void set_cache_control(utility::string_t control);

    /// <summary>
    /// Gets the date header of the message.
    /// </summary>
    /// <returns>The date header value.</returns>
    _ASYNCRTIMP utility::string_t date() const;

    /// <summary>
    /// Sets the date header of the message.
    /// </summary>
    /// <param name="date">The date header value.</param>
    _ASYNCRTIMP void set_date(const utility::datetime& date);

private:

    template<typename _t>
    bool bind_impl(const key_type &text, _t &ref) const
    {
        utility::istringstream_t iss(text);
        iss.imbue(std::locale::classic());
        iss >> ref;
        if (iss.fail() || !iss.eof())
        {
            return false;
        }

        return true;
    }

    bool bind_impl(const key_type &text, utf16string &ref) const
    {
        ref = utility::conversions::to_utf16string(text);
        return true;
    }
    bool bind_impl(const key_type &text, std::string &ref) const
    {
        ref = utility::conversions::to_utf8string(text);
        return true;
    }

    // Headers are stored in a map with case insensitive key.
    std::map<utility::string_t, utility::string_t, _case_insensitive_cmp> m_headers;
};

}}