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
* http_msg.h
*
* HTTP Library: Request and reply message definitions.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <system_error>

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else 
#include "pplx/pplxtasks.h"
#endif

#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/xxpublic.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/streams.h"
#include "cpprest/containerstream.h"

#ifndef WIN32
#include <boost/algorithm/string/predicate.hpp>
#endif

namespace web {

/// <summary>
/// Declaration to avoid making a dependency on IISHost.
/// </summary>
namespace iis_host
{
    class http_iis_receiver;
}

namespace http
{
// URI class has been moved from web::http namespace to web namespace.
// The below using declarations ensure we dont break existing code.
// Please use the web::uri class going forward.
using web::uri;
using web::uri_builder;

namespace client
{
    class http_client;
}

/// <summary>
/// Predefined method strings for the standard HTTP methods mentioned in the
/// HTTP 1.1 specification.
/// </summary>
typedef utility::string_t method;
class methods
{
public:
#define _METHODS
#define DAT(a,b) _ASYNCRTIMP const static method a;
#include "cpprest/http_constants.dat"
#undef _METHODS
#undef DAT
};

typedef unsigned short status_code;

/// <summary>
/// Predefined values for all of the standard HTTP 1.1 response status codes.
/// </summary>
class status_codes
{
public:
#define _PHRASES
#define DAT(a,b,c) const static status_code a=b;
#include "cpprest/http_constants.dat"
#undef _PHRASES
#undef DAT
};

namespace message_direction
{
    /// <summary>
    /// Enumeration used to denote the direction of a message: a request with a body is
    /// an upload, a response with a body is a download.
    /// </summary>
    enum direction {
        upload, 
        download 
    };
}

typedef utility::string_t reason_phrase;
typedef std::function<void(message_direction::direction, utility::size64_t)> progress_handler;

struct http_status_to_phrase
{
    unsigned short id;
    reason_phrase phrase;
};

/// <summary>
/// Binds an individual reference to a string value.
/// </summary>
/// <typeparam name="key_type">The type of string value.</typeparam>
/// <typeparam name="_t">The type of the value to bind to.</typeparam>
/// <param name="text">The string value.</param>
/// <param name="ref">The value to bind to.</param>
/// <returns><c>true</c> if the binding succeeds, <c>false</c> otherwise.</returns>
template<typename key_type, typename _t>
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
bool bind(const key_type &text, utility::string_t &ref) //const 
{ 
    ref = text; 
    return true; 
}

/// <summary>
/// Constants for the HTTP headers mentioned in RFC 2616.
/// </summary>
class header_names
{
public:
#define _HEADER_NAMES
#define DAT(a,b) _ASYNCRTIMP const static utility::string_t a;
#include "cpprest/http_constants.dat"
#undef _HEADER_NAMES
#undef DAT
};

/// <summary>
/// Represents an HTTP error. This class holds an error message and an optional error code.
/// </summary>
class http_exception : public std::exception
{
public:

    /// <summary>
    /// Creates an <c>http_exception</c> with just a string message and no error code.
    /// </summary>
    /// <param name="whatArg">Error message string.</param>
    http_exception(const utility::string_t &whatArg) 
        : m_msg(utility::conversions::to_utf8string(whatArg)) {}

    /// <summary>
    /// Creates an <c>http_exception</c> with from a error code using the current platform error category.
    /// The message of the error code will be used as the what() string message.
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    http_exception(int errorCode) 
        : m_errorCode(utility::details::create_error_code(errorCode))
    {
        m_msg = m_errorCode.message();
    }

    /// <summary>
    /// Creates an <c>http_exception</c> with from a error code using the current platform error category. 
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    /// <param name="whatArg">Message to use in what() string.</param>
    http_exception(int errorCode, const utility::string_t &whatArg) 
        : m_errorCode(utility::details::create_error_code(errorCode)),
          m_msg(utility::conversions::to_utf8string(whatArg))
    {}

    /// <summary>
    /// Creates an <c>http_exception</c> with from a error code and category. The message of the error code will be used
    /// as the <c>what</c> string message.
    /// </summary>
    /// <param name="errorCode">Error code value.</param>
    /// <param name="cat">Error category for the code.</param>
    http_exception(int errorCode, const std::error_category &cat) : m_errorCode(std::error_code(errorCode, cat))
    {
        m_msg = m_errorCode.message();
    }

    ~http_exception() _noexcept {}

    const char* what() const _noexcept
    {
        return m_msg.c_str();
    }

    const std::error_code & error_code() const
    {
        return m_errorCode;
    }

private:
    std::string m_msg;
    std::error_code m_errorCode;
};

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
#ifdef _MS_WINDOWS
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
    /// <remark>If the header field exists, the value will be combined as comma separated string.</remark>
    template<typename _t1>
    void add(const key_type& name, const _t1& value)
    {
        if (has(name))
        {
            m_headers[name] =  m_headers[name].append(_XPLATSTR(", ") + utility::conversions::print_string(value));
        }
        else
        {
            m_headers[name] = utility::conversions::print_string(value);
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
    /// Removes all elements from the hearders
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
                http::bind(iter->second, value);
                return true;
            }
            return http::bind(iter->second, value);
        }
        else
        {
            return false;
        }
    }

    /// <summary>
    /// Returns an iterator refering to the first header field.
    /// </summary>
    /// <returns>An iterator to the beginning of the HTTP headers</returns>
    iterator begin() { return m_headers.begin(); }
    const_iterator begin() const { return m_headers.begin(); }

    /// <summary>
    /// Returns an iterator referring to the past-the-end header field.
    /// </summary>
            /// <returns>An iterator to the element past the end of the HTTP headers</returns>
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

    // Headers are stored in a map with case insensitive key.
    std::map<utility::string_t, utility::string_t, _case_insensitive_cmp> m_headers;
};

namespace details
{

/// <summary>
/// Base class for HTTP messages.
/// This class is to store common functionality so it isn't duplicated on
/// both the request and response side.
/// </summary>
class http_msg_base
{
public:

    friend class http::client::http_client;

    _ASYNCRTIMP http_msg_base();

    virtual ~http_msg_base() {}

    http_headers &headers() { return m_headers; }

    _ASYNCRTIMP void set_body(concurrency::streams::istream instream, utility::string_t contentType);
    _ASYNCRTIMP void set_body(concurrency::streams::istream instream, utility::size64_t contentLength, utility::string_t contentType);

    _ASYNCRTIMP utility::string_t _extract_string();
    _ASYNCRTIMP json::value _extract_json();
    _ASYNCRTIMP std::vector<unsigned char> _extract_vector();

    virtual _ASYNCRTIMP utility::string_t to_string() const;

    /// <summary>
    /// Completes this message
    /// </summary>
    virtual _ASYNCRTIMP void _complete(utility::size64_t bodySize, std::exception_ptr exceptionPtr = std::exception_ptr());

    /// <summary>
    /// Set the stream through which the message body could be read
    /// </summary>
    void set_instream(concurrency::streams::istream instream)  { m_inStream = instream; }

    /// <summary>
    /// Get the stream through which the message body could be read
    /// </summary>
    concurrency::streams::istream instream() const { return m_inStream; }

    /// <summary>
    /// Set the stream through which the message body could be written
    /// </summary>
    void set_outstream(concurrency::streams::ostream outstream, bool is_default)  { m_outStream = outstream; m_default_outstream = is_default; }

    /// <summary>
    /// Get the stream through which the message body could be written
    /// </summary>
    concurrency::streams::ostream outstream() const { return m_outStream; }

    pplx::task_completion_event<utility::size64_t> _get_data_available()  { return m_data_available; }

    /// <summary>
    /// Prepare the message with an output stream to receive network data
    /// </summary>
    _ASYNCRTIMP void _prepare_to_receive_data();

    /// <summary>
    /// Determine the content length
    /// </summary>
    /// <returns>
    /// size_t::max if there is content with unknown length (transfer_encoding:chunked)
    /// 0           if there is no content 
    /// length      if there is content with known length
    /// </returns>
    /// <remarks>
    /// This routine should only be called after a msg (request/response) has been
    /// completely constructed.
    /// </remarks>
    _ASYNCRTIMP size_t _get_content_length();

protected:

    /// <summary>
    /// Stream to read the message body.
    /// By default this is an invalid stream. The user could set the instream on
    /// a request by calling set_request_stream(...). This would also be set when
    /// set_body() is called - a stream from the body is constructed and set.
    /// Even in the presense of msg body this stream could be invalid. An example
    /// would be when the user sets an ostream for the response. With that API the
    /// user does not provide the ability to read the msg body.
    /// Thus m_instream is valid when there is a msg body and it can actually be read
    /// </summary>
    concurrency::streams::istream m_inStream;

    /// <summary>
    /// stream to write the msg body
    /// By default this is an invalid stream. The user could set this on the response
    /// (for http_client). In all the other cases we would construct one to transfer
    /// the data from the network into the message body.
    /// </summary>
    concurrency::streams::ostream m_outStream;

    bool m_default_outstream;

    /// <summary> The TCE is used to signal the availability of the message body. </summary>
    pplx::task_completion_event<utility::size64_t> m_data_available;

    http_headers m_headers;
};

/// <summary>
/// Base structure for associating internal server information
/// with an HTTP request/response.
/// </summary>
class _http_server_context
{
public:
    _http_server_context() {}
    virtual ~_http_server_context() {}
private:
};

/// <summary>
/// Internal representation of an HTTP response.
/// </summary>
class _http_response : public http::details::http_msg_base
{
public:
    _http_response() : m_status_code((std::numeric_limits<uint16_t>::max)()), m_error_code(0) { }

    _http_response(http::status_code code) : m_status_code(code), m_error_code(0) {}

    http::status_code status_code() const { return m_status_code; } 

    void set_status_code(http::status_code code) { m_status_code = code; }

    unsigned long error_code() const { return m_error_code; } 

    void set_error_code(unsigned long code) { m_error_code = code; }

    const http::reason_phrase & reason_phrase() const { return m_reason_phrase; }

    void set_reason_phrase(http::reason_phrase reason) { m_reason_phrase = std::move(reason); }

    _ASYNCRTIMP utility::string_t to_string() const;

    _http_server_context * _get_server_context() const { return m_server_context.get(); }

    void _set_server_context(std::shared_ptr<details::_http_server_context> server_context) { m_server_context = std::move(server_context); }

private:
    std::shared_ptr<_http_server_context> m_server_context;

    unsigned long m_error_code;

    http::status_code m_status_code;
    http::reason_phrase m_reason_phrase;
};

} // namespace details


#pragma region HTTP response message

/// <summary>
/// Represents an HTTP response.
/// </summary>
class http_response
{
public:

    /// <summary>
    /// Constructs a response with an empty status code, no headers, and no body.
    /// </summary>
            /// <returns>A new HTTP response.</returns>
    http_response() : _m_impl(std::make_shared<details::_http_response>()) { }

    /// <summary>
    /// Constructs a response with given status code, no headers, and no body.
    /// </summary>
            /// <param name="code">HTTP status code to use in response.</param>
            /// <returns>A new HTTP response.</returns>
    http_response(http::status_code code) 
        : _m_impl(std::make_shared<details::_http_response>(code)) { }

    /// <summary>
    /// Copy constructor.
    /// </summary>
    /// <param name="response">The http_response to copy from.</param>
    http_response(const http_response &response) : _m_impl(response._m_impl) {}

    /// <summary>
    /// Assignment operator.
    /// </summary>
    http_response &operator=(const http_response &response)
    {
        _m_impl = response._m_impl;
        return *this;
    }

    /// <summary>
    /// Gets the status code of the response message.
    /// </summary>
    /// <returns>status code.</returns>
    http::status_code status_code() const { return _m_impl->status_code(); }

    /// <summary>
    /// Sets the status code of the response message.
    /// </summary>
            /// <param name="code">Status code to set.</param>
    /// <remarks>
    /// This will overwrite any previously set status code.
    /// </remarks>
    void set_status_code(http::status_code code) const { _m_impl->set_status_code(code); }

    /// <summary>
    /// Gets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    /// </summary>
    /// <returns>Reason phrase.</returns>
    const http::reason_phrase & reason_phrase() const { return _m_impl->reason_phrase(); }

    /// <summary>
    /// Sets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    /// </summary>
    /// <param name="reason">The reason phrase to set.</param>
    void set_reason_phrase(http::reason_phrase reason) const { _m_impl->set_reason_phrase(std::move(reason)); }

    /// <summary>
    /// Gets the headers of the response message.
    /// </summary>
    /// <returns>HTTP headers for this response.</returns>
    /// <remarks>
    /// Use the <seealso cref="http_headers::add Method"/> to fill in desired headers.
    /// </remarks>
    http_headers &headers() { return _m_impl->headers(); }
    const http_headers &headers() const { return _m_impl->headers(); }

    /// <summary>
    /// Generates a string representation of the message, including the body when possible.
    /// </summary>
    /// <returns>A string representation of this HTTP request.</returns>
    utility::string_t to_string() const { return _m_impl->to_string(); }

    /// <summary>
    /// Extracts the body of the response message as a string value, checking that the content type is a MIME text type.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// </summary>
    /// <returns>String containing body of the message.</returns>
    pplx::task<utility::string_t> extract_string() const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_string(); });
    }

    /// <summary>
    /// Extracts the body of the response message into a json value, checking that the content type is application\json.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// </summary>
    /// <returns>JSON value from the body of this message.</returns>
    pplx::task<json::value> extract_json() const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_json(); });
    }

    /// <summary>
    /// Extracts the body of the response message into a vector of bytes.
    /// </summary>
    /// <returns>The body of the message as a vector of bytes.</returns>
    pplx::task<std::vector<unsigned char>> extract_vector() const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_vector(); });
    }

    /// <summary>
    /// Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
    /// the character encoding of the string is the OS's default code page and will perform appropriate
    /// conversions to UTF-8.
    /// </summary>
    /// <param name="body_text">String containing body text.</param>
    /// <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain".</param>
    /// <remarks>
    /// This will overwrite any previously set body data and "Content-Type" header.
    /// </remarks>
    void set_body(const utility::string_t &body_text, utility::string_t content_type = utility::string_t(_XPLATSTR("text/plain")))
    { 
        if(content_type.find(_XPLATSTR("charset=")) != content_type.npos)
        {
            throw std::invalid_argument("content_type can't contain a 'charset'.");
        }

        auto utf8body = utility::conversions::to_utf8string(body_text);

        auto length = utf8body.size();
        set_body(concurrency::streams::bytestream::open_istream<std::string>(std::move(utf8body)), length, std::move(content_type.append(_XPLATSTR("; charset=utf-8"))));
    }

#ifdef _MS_WINDOWS
    /// <summary>
    /// Sets the body of the message to contain a UTF-8 string. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'text/plain; charset=utf-8'.
    /// </summary>
    /// <param name="body_text">String containing body text as a UTF-8 string.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    void set_body(std::string body_text, utility::string_t content_type = utility::string_t(_XPLATSTR("text/plain; charset=utf-8")))
    {
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, std::move(content_type));
    }
#endif

    /// <summary>
    /// Sets the body of the message to contain json value. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/json'.
    /// </summary>
    /// <param name="body_text">json value.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    void set_body(const json::value &body_data)
    {
        auto body_text = utility::conversions::to_utf8string(body_data.serialize());
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
    }

    /// <summary>
    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    /// </summary>
    /// <param name="body_data">Vector containing body data.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    void set_body(std::vector<unsigned char> body_data)
    {
        auto length = body_data.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_data)), length);
    }
    
    /// <summary>
    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is
    /// sent.
    /// </summary>
    /// <param name="stream">A readable, open asynchronous stream.</param>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    /// </remarks>
    void set_body(concurrency::streams::istream stream, utility::string_t content_type = _XPLATSTR("application/octet-stream"))
    {
        _m_impl->set_body(stream, std::move(content_type));
    }

    /// <summary>
    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is
    /// sent.
    /// </summary>
    /// <param name="stream">A readable, open asynchronous stream.</param>
    /// <param name="content_length">The size of the data to be sent in the body.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    /// </remarks>
    void set_body(concurrency::streams::istream stream, utility::size64_t content_length, utility::string_t content_type = _XPLATSTR("application/octet-stream"))
    {
        _m_impl->set_body(stream, content_length, std::move(content_type));
    }

    /// <summary>
    /// Produces a stream which the caller may use to retrieve data from an incoming request.
    /// </summary>
    /// <returns>A readable, open asynchronous stream.</returns>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of getting the body of the request.
    /// It is not necessary to wait until the message has been sent before starting to write to the
    /// stream, but it is advisable to do so, since it will allow the network I/O to start earlier
    /// and the work of sending data can be overlapped with the production of more data.
    /// </remarks>
    concurrency::streams::istream body() const
    {
        return  _m_impl->instream();
    }

    /// <summary>
    /// Signals the user (client) when all the data for this response message has been received.
    /// </summary>
    /// <returns>A <c>task</c> which is completed when all of the response body has been received.</returns>
    pplx::task<http::http_response> content_ready() const
    {
        auto impl = _get_impl();
        return pplx::create_task(impl->_get_data_available()).then([impl](utility::size64_t) -> http_response { return http_response(impl); });
    }

    /// <summary>
    /// Gets the error code of the response. This is used for errors other than HTTP status codes.
    /// </summary>
    /// <returns>The error code.</returns>
    unsigned long error_code() const { return _m_impl->error_code(); }

    /// <summary>
    /// Sets the error code of the response. This is used for errors other than HTTP status codes.
    /// </summary>
    /// <param name="code">The error code</param>
    void set_error_code(unsigned long code) const { _m_impl->set_error_code(code); }

    std::shared_ptr<http::details::_http_response> _get_impl() const { return _m_impl; }

    http::details::_http_server_context * _get_server_context() const { return _m_impl->_get_server_context(); }
    void _set_server_context(std::shared_ptr<http::details::_http_server_context> server_context) { _m_impl->_set_server_context(std::move(server_context)); }

private:

    http_response(std::shared_ptr<http::details::_http_response> impl)
        : _m_impl(impl)
    {
    }

private:
    std::shared_ptr<http::details::_http_response> _m_impl;
};

#pragma endregion

namespace details {
/// <summary>
/// Internal representation of an HTTP request message.
/// </summary>
class _http_request : public http::details::http_msg_base, public std::enable_shared_from_this<_http_request>
{
public:

    _ASYNCRTIMP _http_request();

    _http_request(const http::method& mtd)
        :  m_method(mtd), 
        m_initiated_response(0),
        m_server_context(), 
        m_listener_path(_XPLATSTR("")),
        m_cancellationToken(pplx::cancellation_token::none())
    {
        if(mtd.empty())
        {
            throw std::invalid_argument("Invalid HTTP method specified. Method can't be an empty string.");
        }
    }

    _ASYNCRTIMP _http_request(std::shared_ptr<http::details::_http_server_context> server_context);

    virtual ~_http_request() { }

    http::method &method() { return m_method; }

    uri &request_uri() { return m_uri; }

    _ASYNCRTIMP uri relative_uri() const;

    _ASYNCRTIMP void set_request_uri(const uri&);

    pplx::cancellation_token cancellation_token() { return m_cancellationToken; }

    void set_cancellation_token(pplx::cancellation_token token)
    {
        m_cancellationToken = token;
    }

    _ASYNCRTIMP utility::string_t to_string() const;

    _ASYNCRTIMP pplx::task<void> reply(http_response response);

    pplx::task<http_response> get_response()
    {
        return pplx::task<http_response>(m_response);
    }

    _ASYNCRTIMP pplx::task<void> _reply_if_not_already(http::status_code status);

    void set_response_stream(concurrency::streams::ostream stream)
    {
        m_response_stream = stream;
    }

    void set_progress_handler(progress_handler handler)
    {
        m_progress_handler = std::make_shared<progress_handler>(handler);
    }

    concurrency::streams::ostream _response_stream() const { return m_response_stream; }

    std::shared_ptr<progress_handler> _progress_handler() const { return m_progress_handler; }

    http::details::_http_server_context * _get_server_context() const { return m_server_context.get(); }

    void _set_server_context(std::shared_ptr<http::details::_http_server_context> server_context) { m_server_context = std::move(server_context); }

    void _set_listener_path(const utility::string_t &path) { m_listener_path = path; }


private:

    // Actual initiates sending the response, without checking if a response has already been sent.
    pplx::task<void> _reply_impl(http_response response);

    // Tracks whether or not a response has already been started for this message.
    pplx::details::atomic_long m_initiated_response;

    pplx::cancellation_token m_cancellationToken;

    http::method m_method;

    http::uri m_uri;
    utility::string_t m_listener_path;
    std::shared_ptr<http::details::_http_server_context> m_server_context;

    concurrency::streams::ostream m_response_stream;

    std::shared_ptr<progress_handler> m_progress_handler;

    pplx::task_completion_event<http_response> m_response;
};


}  // namespace details

#pragma region HTTP Request

/// <summary>
/// Represents an HTTP request.
/// </summary>
class http_request
{
public:
    /// <summary>
    /// Constructs a new HTTP request with the given request method.
    /// </summary>
    /// <param name="method">Request method.</param>
    http_request() 
        : _m_impl(std::make_shared<http::details::_http_request>()) {}

    /// <summary>
    /// Constructs a new HTTP request with the given request method.
    /// </summary>
    /// <param name="mtd">Request method.</param>
    http_request(http::method mtd) 
        : _m_impl(std::make_shared<http::details::_http_request>(std::move(mtd))) {}

    /// <summary>
    /// Copy constructor.
    /// </summary>
    http_request(const http_request &message) : _m_impl(message._m_impl) {}

    /// <summary>
    /// Destructor frees any held resources.
    /// </summary>
    ~http_request() {}

    /// <summary>
    /// Assignment operator.
    /// </summary>
    http_request &operator=(const http_request &message) 
    {
        _m_impl = message._m_impl;
        return *this;
    }

    /// <summary>
    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    /// </summary>
    /// <returns>Request method of this HTTP request.</returns>
    const http::method &method() const { return _m_impl->method(); }

    /// <summary>
    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    /// </summary>
    /// <param name="method">Request method of this HTTP request.</param>
    void set_method(http::method method) const { _m_impl->method() = std::move(method); }

    /// <summary>
    /// Get the underling URI of the request message.
    /// </summary>
    /// <returns>The uri of this message.</returns>
    uri request_uri() const { return _m_impl->request_uri(); }

    /// <summary>
    /// Set the underling URI of the request message.
    /// </summary>
    /// <param name="uri">The uri for this message.</param>
    void set_request_uri(const uri& uri) { return _m_impl->set_request_uri(uri); }

    /// <summary>
    /// Gets a reference the URI path, query, and fragment part of this request message.
    /// This will be appended to the base URI specified at construction of the http_client.
    /// </summary>
    /// <returns>A string.</returns>
    /// <remarks>When the request is the one passed to a listener's handler, the
    /// relative URI is the request URI less the listener's path. In all other circumstances,
    /// request_uri() and relative_uri() will return the same value.
    /// </remarks>
    uri relative_uri() const { return _m_impl->relative_uri(); }

    /// <summary>
    /// Gets a reference to the headers of the response message.
    /// </summary>
    /// <returns>HTTP headers for this response.</returns>
    /// <remarks>
    /// Use the http_headers::add to fill in desired headers.
    /// </remarks>
    http_headers &headers() { return _m_impl->headers(); }
    
    /// <summary>
    /// Gets a const reference to the headers of the response message.
    /// </summary>
    /// <returns>HTTP headers for this response.</returns>
    /// <remarks>
    /// Use the http_headers::add to fill in desired headers.
    /// </remarks>
    const http_headers &headers() const { return _m_impl->headers(); }

    /// <summary>
    /// Extract the body of the request message as a string value, checking that the content type is a MIME text type.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// </summary>
    /// <returns>String containing body of the message.</returns>
    pplx::task<utility::string_t> extract_string()
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_string(); });
    }

    /// <summary>
    /// Extracts the body of the request message into a json value, checking that the content type is application\json.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// </summary>
    /// <returns>JSON value from the body of this message.</returns>
    pplx::task<json::value> extract_json() const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_json(); });
    }

    /// <summary>
    /// Extract the body of the response message into a vector of bytes. Extracting a vector can be done on 
    /// </summary>
    /// <returns>The body of the message as a vector of bytes.</returns>
    pplx::task<std::vector<unsigned char>> extract_vector() const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_vector(); });
    }

    /// <summary>
    /// Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
    /// the character encoding of the string is the OS's default code page and will perform appropriate
    /// conversions to UTF-8.
    /// </summary>
    /// <param name="body_text">String containing body text.</param>
    /// <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain".</param>
    /// <remarks>
    /// This will overwrite any previously set body data and "Content-Type" header.
    /// </remarks>
    void set_body(const utility::string_t &body_text, utility::string_t content_type = utility::string_t(_XPLATSTR("text/plain")))
    { 
        if(content_type.find(_XPLATSTR("charset=")) != content_type.npos)
        {
            throw std::invalid_argument("content_type can't contain a 'charset'.");
        }

        auto utf8body = utility::conversions::to_utf8string(body_text);

        auto length = utf8body.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(utf8body)), length, std::move(content_type.append(_XPLATSTR("; charset=utf-8"))));
    }

#ifdef _MS_WINDOWS
    /// <summary>
    /// Sets the body of the message to contain a UTF-8 string. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'text/plain; charset=utf-8'.
    /// </summary>
    /// <param name="body_text">String containing body text as a UTF-8 string.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    void set_body(std::string body_text, utility::string_t content_type = utility::string_t(_XPLATSTR("text/plain; charset=utf-8")))
    {
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, std::move(content_type));
    }
#endif

    /// <summary>
    /// Sets the body of the message to contain json value. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/json'.
    /// </summary>
    /// <param name="body_text">json value.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    void set_body(const json::value &body_data)
    {
        auto body_text = utility::conversions::to_utf8string(body_data.serialize());
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
    }

    /// <summary>
    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    /// </summary>
    /// <param name="body_data">Vector containing body data.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    void set_body(std::vector<unsigned char> body_data)
    {
        auto length = body_data.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_data)), length);
    }

    /// <summary>
    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is
    /// sent.
    /// </summary>
    /// <param name="stream">A readable, open asynchronous stream.</param>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    /// </remarks>
    void set_body(concurrency::streams::istream stream, utility::string_t content_type = _XPLATSTR("application/octet-stream"))
    {
        _m_impl->set_body(stream, std::move(content_type));
    }

    /// <summary>
    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is
    /// sent.
    /// </summary>
    /// <param name="stream">A readable, open asynchronous stream.</param>
    /// <param name="content_length">The size of the data to be sent in the body.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    /// </remarks>
    void set_body(concurrency::streams::istream stream, utility::size64_t content_length, utility::string_t content_type = _XPLATSTR("application/octet-stream"))
    {
        _m_impl->set_body(stream, content_length, std::move(content_type));
    }

    /// <summary>
    /// Produces a stream which the caller may use to retrieve data from an incoming request.
    /// </summary>
    /// <returns>A readable, open asynchronous stream.</returns>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of getting the body of the request.
    /// It is not necessary to wait until the message has been sent before starting to write to the
    /// stream, but it is advisable to do so, since it will allow the network I/O to start earlier
    /// and the work of sending data can be overlapped with the production of more data.
    /// </remarks>
    concurrency::streams::istream body() const
    {
        return _m_impl->instream();
    }

    /// <summary>
    /// Defines a stream that will be relied on to hold the body of the HTTP response message that
    /// results from the request.
    /// </summary>
    /// <param name="stream">A writable, open asynchronous stream.</param>
    /// <remarks>
    /// If this function is called, the body of the response should not be accessed in any other
    /// way.
    /// </remarks>
    void set_response_stream(concurrency::streams::ostream stream)
    {
        return _m_impl->set_response_stream(stream);
    }

    /// <summary>
    /// Defines a callback function that will be invoked for every chunk of data uploaded or downloaded
    /// as part of the request.
    /// </summary>
    /// <param name="handler">A function representing the progress handler. It's parameters are:
    ///    up:       a <c>message_direction::direction</c> value  indicating the direction of the message
    ///              that is being reported.
    ///    progress: the number of bytes that have been processed so far.
    /// </param>
    /// <remarks>
    ///   **EXPERIMENTAL**
    ///   This function is subject to change based on user feedback.
    ///
    ///   This function will be called at least once for upload and at least once for
    ///   the download body, unless there is some exception generated. An HTTP message with an error
    ///   code is not an exception. This means, that even if there is no body, the progress handler
    ///   will be called.
    ///
    ///   Setting the chunk size on the http_client does not guarantee that the client will be using
    ///   exactly that increment for uploading and downloading data.
    ///
    ///   The handler will be called only once for each combination of argument values, in order. Depending
    ///   on how a service responds, some download values may come before all upload values have been
    ///   reported.
    ///
    ///   The progress handler will be called on the thread processing the request. This means that
    ///   the implementation of the handler must take care not to block the thread or do anything
    ///   that takes significant amounts of time. In particular, do not do any kind of I/O from within
    ///   the handler, do not update user interfaces, and to not acquire any locks. If such activities
    ///   are necessary, it is the handler's responsibilty to execute that work on a separate thread.
    /// </remarks>
    void set_progress_handler(progress_handler handler)
    {
        return _m_impl->set_progress_handler(handler);
    }

    /// <summary>
    /// Asynchronously responses to this HTTP request.
    /// </summary>
    /// <param name="response">Response to send.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    pplx::task<void> reply(http_response response) const { return _m_impl->reply(response); }

    /// <summary>
    /// Asynchronously responses to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    pplx::task<void> reply(http::status_code status) const
    { 
        return reply(http_response(status)); 
    }

    /// <summary>
    /// Responses to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <param name="body_data">Json value to use in the response body.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    pplx::task<void> reply(http::status_code status, const json::value &body_data) const
    {
        http_response response(status);
        response.set_body(body_data);
        return reply(response);
    }

    /// <summary>
    /// Responses to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <param name="content_type">Content type of the body.</param>
    /// <param name="body_data">String containing the text to use in the response body.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    /// <remarks>
    /// The response may be sent either synchronously or asychronously depending on an internal
    /// algorithm on whether we decide or not to copy the body data. Either way callers of this function do NOT
    /// need to block waiting for the response to be sent to before the body data is destroyed or goes
    /// out of scope.
    /// </remarks>
    pplx::task<void> reply(http::status_code status, const utility::string_t &body_data, utility::string_t content_type = _XPLATSTR("text/plain")) const
    {
        http_response response(status);
        response.set_body(body_data, std::move(content_type));
        return reply(response);  
    }

    /// <summary>
    /// Responses to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    pplx::task<void> reply(status_code status, concurrency::streams::istream body, utility::string_t content_type = _XPLATSTR("application/octet-stream")) const
    {
        http_response response(status);
        response.set_body(body, std::move(content_type));
        return reply(response);  
    }

    /// <summary>
    /// Responses to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <param name="content_length">The size of the data to be sent in the body..</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <param name="body">An asynchronous stream representing the body data.</param>
    /// <returns>A task that is completed once a response from the request is received.</returns>
    pplx::task<void> reply(status_code status, concurrency::streams::istream body, utility::size64_t content_length, utility::string_t content_type = _XPLATSTR("application/octet-stream")) const
    {
        http_response response(status);
        response.set_body(body, content_length, std::move(content_type));
        return reply(response);  
    }

    /// <summary>
    /// Signals the user (listener) when all the data for this request message has been received.
    /// </summary>
    /// <returns>A <c>task</c> which is completed when all of the response body has been received</returns>
    pplx::task<http_request> content_ready() const
    {
        auto impl = _get_impl();
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return http_request(impl); });
    }

    /// <summary>
    /// Gets a task representing the response that will eventually be sent.
    /// </summary>
    /// <returns>A task that is completed once response is sent.</returns>
    pplx::task<http_response> get_response() const
    {
        return _m_impl->get_response();
    }

    /// <summary>
    /// Generates a string representation of the message, including the body when possible.
    /// </summary>
    /// <returns>A string representation of this HTTP request.</returns>
    utility::string_t to_string() const { return _m_impl->to_string(); }

    /// <summary>
    /// Sends a response if one has not already been sent.
    /// </summary>
    pplx::task<void> _reply_if_not_already(status_code status) { return _m_impl->_reply_if_not_already(status); }

    /// <summary>
    /// Gets the server context associated with this HTTP message.
    /// </summary>
    http::details::_http_server_context * _get_server_context() const { return _m_impl->_get_server_context(); }

    /// <summary>
    /// These are used for the initial creation of the HTTP request.
    /// </summary>
    static http_request _create_request(std::shared_ptr<http::details::_http_server_context> server_context) { return http_request(std::move(server_context)); }
    void _set_server_context(std::shared_ptr<http::details::_http_server_context> server_context) { _m_impl->_set_server_context(std::move(server_context)); }

    void _set_listener_path(const utility::string_t &path) { _m_impl->_set_listener_path(path); }

    std::shared_ptr<http::details::_http_request> _get_impl() const { return _m_impl; }

    void _set_cancellation_token(pplx::cancellation_token token)
    {
        _m_impl->set_cancellation_token(token);
    }

    pplx::cancellation_token _cancellation_token()
    {
        return _m_impl->cancellation_token();
    }

private:
    friend class http::details::_http_request;
    friend class http::client::http_client;

    http_request(std::shared_ptr<http::details::_http_server_context> server_context) : _m_impl(std::shared_ptr<details::_http_request>(new details::_http_request(std::move(server_context)))) {}
    http_request(std::shared_ptr<http::details::_http_request> message) : _m_impl(message) {}

    /// <summary>
    /// This constructor overload is only needed when <c>'const'</c> functions in <c>_http_request</c> need to log messages (<c>log::post</c>).
    /// call <c>shared_from_this()</c> then get back a <c>std::shared_ptr&lt;const details::_http_request&gt;</c>, 
    /// from which we need to create an <c>http_request</c>.
    /// </summary>
    http_request(std::shared_ptr<const http::details::_http_request> message)
    {
        _m_impl = std::shared_ptr<http::details::_http_request>(const_cast<http::details::_http_request *>(message.get()));
    }

    std::shared_ptr<http::details::_http_request> _m_impl;
};

#pragma endregion

/// <summary>
/// HTTP client handler class, used to represent an HTTP pipeline stage.
/// </summary>
/// <remarks>
/// When a request goes out, it passes through a series of stages, customizable by
/// the application and/or libraries. The default stage will interact with lower-level
/// communication layers to actually send the message on the network. When creating a client
/// instance, an application may add pipeline stages in front of the already existing
/// stages. Each stage has a reference to the next stage available in the <seealso cref="http_pipeline_stage::next_stage Method"/>
/// value.
/// </remarks>
class http_pipeline_stage : public std::enable_shared_from_this<http_pipeline_stage>
{
public:

    virtual ~http_pipeline_stage()
    {
    }

    /// <summary>
    /// Runs this stage against the given request and passes onto the next stage.
    /// </summary>
    /// <param name="request">The HTTP request.</param>
    /// <returns>A task of the HTTP response.</returns>
    virtual pplx::task<http_response> propagate(http_request request) = 0;

protected:

    http_pipeline_stage()
    {
    }

    /// <summary>
    /// Gets the next stage in the pipeline.
    /// </summary>
    /// <returns>A shared pointer to a pipeline stage.</returns>
    std::shared_ptr<http_pipeline_stage> next_stage() const
    {
        return m_next_stage;
    }

    /// <summary>
    /// Gets a shared pointer to this pipeline stage.
    /// </summary>
    /// <returns>A shared pointer to a pipeline stage.</returns>
    std::shared_ptr<http_pipeline_stage> current_stage()
    {
        return this->shared_from_this();
    }

private:
    friend class http_pipeline;

    void set_next_stage(std::shared_ptr<http_pipeline_stage> next)
    {
        m_next_stage = next; 
    }

    std::shared_ptr<http_pipeline_stage> m_next_stage;

    // No copy or assignment.
    http_pipeline_stage & operator=(const http_pipeline_stage &);
    http_pipeline_stage(const http_pipeline_stage &);
};

namespace details {

class function_pipeline_wrapper : public http::http_pipeline_stage
{
public:
    function_pipeline_wrapper(std::function<pplx::task<http_response>(http_request, std::shared_ptr<http::http_pipeline_stage>)> handler) : m_handler(handler)
    {
    }

    virtual pplx::task<http_response> propagate(http_request request)
    {
        return m_handler(request, next_stage());
    }
private:

    std::function<pplx::task<http_response>(http_request, std::shared_ptr<http::http_pipeline_stage>)> m_handler;
};

} // namespace details

class http_pipeline
{
public:

    ~http_pipeline()
    {
        
    }
    /// <summary>
    /// Create an http pipeline that consists of a linear chain of stages
    /// </summary>
    /// <param name="last">The final stage</param>
    static std::shared_ptr<http_pipeline> create_pipeline(std::shared_ptr<http_pipeline_stage> last)
    {
        return std::shared_ptr<http_pipeline>(new http_pipeline(last));
    }

    /// <summary>
    /// Initiate an http request into the pipeline
    /// </summary>
    /// <param name="request">Http request</param>
    pplx::task<http_response> propagate(http_request request)
    {
        std::shared_ptr<http_pipeline_stage> first;
        {
            pplx::extensibility::scoped_recursive_lock_t l(m_lock);
            first = (m_stages.size() > 0) ? m_stages[0] : m_last_stage;
        }
        return first->propagate(request);
    }

    /// <summary>
    /// Adds an HTTP pipeline stage to the pipeline.
    /// </summary>
    /// <param name="stage">A pipeline stage.</param>
    void append(std::shared_ptr<http_pipeline_stage> stage)
    {
        pplx::extensibility::scoped_recursive_lock_t l(m_lock);

        if ( m_stages.size() > 0 )
        {
            std::shared_ptr<http_pipeline_stage> penultimate = m_stages[m_stages.size()-1];
            penultimate->set_next_stage(stage);
        }
        stage->set_next_stage(m_last_stage);

        m_stages.push_back(stage);
    }

    void set_last_stage(std::shared_ptr<http_pipeline_stage> last)
    {
        m_last_stage = last;
    }

    const std::shared_ptr<http_pipeline_stage>& last_stage() const
    {
        return m_last_stage;
    }

private:

    http_pipeline(std::shared_ptr<http_pipeline_stage> last) : m_last_stage(last) 
    {
    }

private:

    // The vector of pipeline stages.
    std::vector<std::shared_ptr<http_pipeline_stage>> m_stages;

    // The last stage is always set up by the client or listener and cannot
    // be changed. All application-defined stages are executed before the
    // last stage, which is typically a send or dispatch.
    std::shared_ptr<http_pipeline_stage> m_last_stage;

    pplx::extensibility::recursive_lock_t m_lock;

    // No copy or assignment.
    http_pipeline & operator=(const http_pipeline &);
    http_pipeline(const http_pipeline &);
};

} // namespace http
} // namespace web
