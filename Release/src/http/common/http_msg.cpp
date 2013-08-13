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
* http_msg.cpp
*
* HTTP Library: Request and reply message definitions.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "cpprest/http_msg.h"
#include "cpprest/producerconsumerstream.h"
#include "cpprest/http_helpers.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace utility::conversions;
using namespace http::details;

namespace web { namespace http
{

#define CRLF _XPLATSTR("\r\n")

static utility::string_t _g_emptyString;

utility::string_t http_headers::content_type() const
{
    utility::string_t result;
    match(http::header_names::content_type, result);
    return result;
}

void http_headers::set_content_type(utility::string_t type)
{
    add(http::header_names::content_type, std::move(type));
}

utility::string_t http_headers::cache_control() const
{
    utility::string_t result;
    match(http::header_names::cache_control, result);
    return result;
}

void http_headers::set_cache_control(utility::string_t control)
{
    add(http::header_names::cache_control, std::move(control));
}

utility::string_t http_headers::date() const
{
    utility::string_t result;
    match(http::header_names::date, result);
    return result;
}

void http_headers::set_date(const utility::datetime& date)
{
    add(http::header_names::date, date.to_string(utility::datetime::RFC_1123));
}

utility::size64_t http_headers::content_length() const
{
    size_t length = 0;
    match(http::header_names::content_length, length);
    return length;
}

void http_headers::set_content_length(utility::size64_t length)
{
    add(http::header_names::content_length, length);
}

static const utility::char_t * stream_was_set_explicitly = _XPLATSTR("A stream was set on the message and extraction is not possible");
static const utility::char_t * textual_content_type_missing = _XPLATSTR("Content-Type must be textual to extract a string.");
static const utility::char_t * unsupported_charset = _XPLATSTR("Charset must be iso-8859-1, utf-8, utf-16, utf-16le, or utf-16be to be extracted.");

http_msg_base::http_msg_base() 
    : m_headers(), 
	  m_default_outstream(false)
{
}

/// <summary>
/// Prepare the message with an output stream to receive network data
/// </summary>
void http_msg_base::_prepare_to_receive_data()
{
    // See if the user specified an outstream
    if (!outstream())
    {
        // The user did not specify an outstream.
        // We will create one...
        concurrency::streams::producer_consumer_buffer<uint8_t> buf;
        set_outstream(buf.create_ostream(), true);

        // Since we are creating the streambuffer, set the input stream
        // so that the user can retrieve the data.
        set_instream(buf.create_istream());
    }

    // If the user did specify an outstream we leave the instream
    // as invalid. It is assumed that user either has a read head 
    // to the out streambuffer or the data is streamed into a container
    // or media (like file) that the user can read from...
}

/// <summary>
///     Determine the content length
/// </summary>
/// <returns>
///     size_t::max if there is content with unknown length (transfer_encoding:chunked)
///     0           if there is no content 
///     length      if there is content with known length
/// </returns>
/// <remarks>
///     This routine should only be called after a msg (request/response) has been
///     completely constructed.
/// </remarks>
size_t http_msg_base::_get_content_length()
{
    // An invalid response_stream indicates that there is no body
    if ((bool)instream())
    {
        size_t content_length = 0;
        utility::string_t transfer_encoding;

        bool has_cnt_length = headers().match(header_names::content_length, content_length);
        bool has_xfr_encode = headers().match(header_names::transfer_encoding, transfer_encoding);

        if (has_xfr_encode)
        {
            return std::numeric_limits<size_t>::max();
        }

        if (has_cnt_length)
        {
            return content_length;
        }

        // Neither is set. Assume transfer-encoding for now (until we have the ability to determine
        // the length of the stream).
        headers().add(header_names::transfer_encoding, _XPLATSTR("chunked"));
        return std::numeric_limits<size_t>::max();
    }

    return 0;
}

/// <summary>
/// Completes this message
/// </summary>
void http_msg_base::_complete(utility::size64_t body_size, std::exception_ptr exceptionPtr)
{
    // Close the write head
    if ((bool)outstream())
    {
		if ( !(exceptionPtr == std::exception_ptr()) )
			outstream().close(exceptionPtr).get();
		else if ( m_default_outstream )
			outstream().close().get();
    }

    if(exceptionPtr == std::exception_ptr())
    {
        _get_data_available().set(body_size);
    }
    else
    {
        _get_data_available().set_exception(exceptionPtr);
        // The exception for body will be observed by default, because read body is not always required.
        pplx::create_task(_get_data_available()).then([](pplx::task<utility::size64_t> t) {
            try {
                t.get();
            } catch (...) {
            }
        });

    }
}

utility::string_t details::http_msg_base::_extract_string()
{
    utility::string_t content, charset;
    parse_content_type_and_charset(headers().content_type(), content, charset);

    // Content-Type must have textual type.
    if(!is_content_type_textual(content))
    {
        throw http_exception(textual_content_type_missing);
    }

    if (!instream())
    {
        throw http_exception(stream_was_set_explicitly);
    }

    auto buf_r = instream().streambuf();

    if (buf_r.in_avail() == 0)
    {
        return utility::string_t();
    }

    // static_casts are used because we allocated these buffers and I know with certainly they
    // are of the type corresponding to the charset.

#ifdef _MS_WINDOWS
    if(_wcsicmp(charset.c_str(), charset_types::usascii.c_str()) == 0)
#else
    if(boost::iequals(charset, charset_types::usascii))
#endif
    {
        std::string body;
        body.resize((std::string::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return to_string_t(usascii_to_utf16(body));
    }

    // Latin1
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::latin1.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::latin1))
#endif
    {
        std::string body;
        body.resize((std::string::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return to_string_t(latin1_to_utf16(body));
    }

    // utf-8.
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf8.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf8))
#endif
    {
        std::string body;
        body.resize((std::string::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return to_string_t(body);
    }

    // utf-16.
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16))
#endif
    {
        std::vector<uint8_t> body((std::vector<uint8_t>::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return to_string_t(convert_utf16_to_utf16(body));
    }

    // utf-16le
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16le.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16le))
#endif
    {
        utility::string_t body;
        body.resize(buf_r.in_avail()/sizeof(utf16char));
        buf_r.getn((uint8_t*)&body[0], body.size()*sizeof(utf16char)); // There is no risk of blocking.
        return body;
    }

    // utf-16be
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16be.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16be))
#endif
    {
        std::vector<uint8_t> body((std::vector<uint8_t>::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return to_string_t(convert_utf16be_to_utf16le(body, false));
    }
    
    else
    {
        throw http_exception(unsupported_charset);
    }
}

json::value details::http_msg_base::_extract_json()
{
    utility::string_t content, charset;
    parse_content_type_and_charset(headers().content_type(), content, charset);

    // Content-Type must be "application/json" or one of the unofficial, but common JSON types.
    if(!is_content_type_json(content))
    {
		const utility::string_t actualContentType = utility::conversions::to_string_t(content);
        throw http_exception((_XPLATSTR("Content-Type must be JSON to extract (is: ") + actualContentType + _XPLATSTR(")")).c_str());
    }
    
    if (!instream())
    {
        throw http_exception(stream_was_set_explicitly);
    }

    auto buf_r = instream().streambuf();

    if (buf_r.in_avail() == 0)
    {
        return json::value::parse(utility::string_t());
    }

    // static_casts are used because we allocated these buffers and I know with certainly they
    // are of the type corresponding to the charset.

    // Latin1
#ifdef _MS_WINDOWS
    if(_wcsicmp(charset.c_str(), charset_types::latin1.c_str()) == 0)
#else
    if(boost::iequals(charset, charset_types::latin1))
#endif
    {
        std::string body;
        body.resize(buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return json::value::parse(to_string_t(latin1_to_utf16(body)));
    }

    // utf-8 and usascii
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf8.c_str()) == 0 || _wcsicmp(charset.c_str(), charset_types::usascii.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf8) || boost::iequals(charset, charset_types::usascii))
#endif
    {
        std::string body;
        body.resize(buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return json::value::parse(to_string_t(body));
    }

    // utf-16.
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16))
#endif
    {
        std::vector<uint8_t> body((std::vector<uint8_t>::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return json::value::parse(to_string_t(convert_utf16_to_utf16(body)));
    }

    // utf-16le
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16le.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16le))
#endif
    {
        utf16string body;
        body.resize((utf16string::size_type)buf_r.in_avail()/sizeof(utf16char));
        buf_r.getn((uint8_t*)&body[0], body.size()*sizeof(utf16char)).get(); // There is no risk of blocking.
        return json::value::parse(to_string_t(body));
    }

    // utf-16be
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16be.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16be))
#endif
    {
        std::vector<uint8_t> body((std::vector<uint8_t>::size_type)buf_r.in_avail());
        buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
        return json::value::parse(to_string_t(convert_utf16be_to_utf16le(body, false)));
    }
    
    else
    {   
        throw http_exception(unsupported_charset);
    }
}

std::vector<uint8_t> details::http_msg_base::_extract_vector()
{
    if (!instream())
    {
        throw http_exception(stream_was_set_explicitly);
    }

    std::vector<uint8_t> body;
    auto buf_r = instream().streambuf();	
	size_t size = buf_r.in_avail();

    body.resize((std::vector<uint8_t>::size_type)buf_r.in_avail());
	if (size > 0)
	{
		body.resize(size);
		buf_r.getn((uint8_t*)&body[0], body.size()).get(); // There is no risk of blocking.
	}

    return body;
}

// Helper function to convert message body without extracting.
static utility::string_t convert_body_to_string_t(const utility::string_t &content_type, concurrency::streams::istream instream)
{
    if (!instream)
    {
        // The instream is not yet set
        return utility::string_t();
    }

    concurrency::streams::streambuf<uint8_t>  streambuf = instream.streambuf();

    _ASSERTE((bool)streambuf);
    _ASSERTE(streambuf.is_open());
    _ASSERTE(streambuf.can_read());

    utility::string_t content, charset;
    parse_content_type_and_charset(content_type, content, charset);

    // Content-Type must have textual type.
    if(!is_content_type_textual(content) || streambuf.in_avail() == 0)
    {
        return utility::string_t();
    }

    std::vector<unsigned char> body((std::vector<unsigned char>::size_type)streambuf.in_avail());
    size_t copied = streambuf.scopy(&body[0], body.size());
    if (copied == 0 || copied == (size_t)-1) 
        return _XPLATSTR("");

    // Latin1
#ifdef _MS_WINDOWS
    if(_wcsicmp(charset.c_str(), charset_types::latin1.c_str()) == 0)
#else
    if(boost::iequals(charset, charset_types::latin1))
#endif
    {
        return to_string_t(latin1_to_utf16(convert_bytes_to_string(&body[0], body.size())));
    }

    // utf-8.
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf8.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf8) )
#endif
    {
        return to_string_t(convert_utf8_to_utf16(&body[0], body.size()));
    }

    // utf-16.
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16) )
#endif
    {
        return to_string_t(convert_utf16_to_utf16(&body[0], body.size()));
    }

    // utf-16le
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16le.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16le) )
#endif
    {
        return to_string_t(convert_bytes_to_wstring(&body[0], body.size()));
    }

    // utf-16be
#ifdef _MS_WINDOWS
    else if(_wcsicmp(charset.c_str(), charset_types::utf16be.c_str()) == 0)
#else
    else if(boost::iequals(charset, charset_types::utf16be) )
#endif
    {
        return to_string_t(convert_utf16be_to_utf16le(&body[0], body.size(), false));
    }
    
    else
    {
        return utility::string_t();
    }
}

//
// Helper function to generate a wstring from given http_headers and message body.
//
static utility::string_t http_headers_body_to_string(const http_headers &headers, concurrency::streams::istream instream)
{
    utility::ostringstream_t buffer;

    for (auto iter = headers.begin(); iter != headers.end(); ++iter)
    {
        buffer << iter->first << _XPLATSTR(": ") << iter->second << CRLF;
    }
    buffer << CRLF;

    utility::string_t content_type;
    if(headers.match(http::header_names::content_type, content_type))
    {
        buffer << convert_body_to_string_t(content_type, instream);
    }

    return buffer.str();
}

utility::string_t details::http_msg_base::to_string() const
{
    return http_headers_body_to_string(m_headers, instream());
}

static void set_content_type_if_not_present(http::http_headers &headers, utility::string_t content_type)
{
    utility::string_t temp;
    if(!headers.match(http::header_names::content_type, temp))
    {
        headers.add(http::header_names::content_type, std::move(content_type));
    }
}

void details::http_msg_base::set_body(streams::istream instream, utility::string_t contentType)
{
    set_content_type_if_not_present(headers(), std::move(contentType));
    set_instream(instream);
}

void details::http_msg_base::set_body(streams::istream instream, utility::size64_t contentLength, utility::string_t contentType)
{
    headers().add(http::header_names::content_length, contentLength);
    set_body(instream, std::move(contentType));
    m_data_available.set(contentLength);
}

details::_http_request::_http_request()
  : m_initiated_response(0), 
    m_server_context(), 
    m_listener_path(_XPLATSTR(""))
{
}

details::_http_request::_http_request(std::shared_ptr<http::details::_http_server_context> server_context)
  : m_initiated_response(0), 
    m_server_context(std::move(server_context)),
    m_listener_path(_XPLATSTR(""))
{
}

#define _METHODS
#define DAT(a,b) const method methods::a = b;
#include "cpprest/http_constants.dat"
#undef _METHODS
#undef DAT

#define _HEADER_NAMES
#define DAT(a,b) const utility::string_t header_names::a = _XPLATSTR(b);
#include "cpprest/http_constants.dat"
#undef _HEADER_NAMES
#undef DAT

#define _MIME_TYPES
#define DAT(a,b) const utility::string_t mime_types::a = _XPLATSTR(b);
#include "cpprest/http_constants.dat"
#undef _MIME_TYPES
#undef DAT

#define _CHARSET_TYPES
#define DAT(a,b) const utility::string_t charset_types::a = _XPLATSTR(b);
#include "cpprest/http_constants.dat"
#undef _CHARSET_TYPES
#undef DAT

// This is necessary for Linux because of a bug in GCC 4.7
#ifndef _MS_WINDOWS
#define _PHRASES
#define DAT(a,b,c) const status_code status_codes::a;
#include "cpprest/http_constants.dat"
#undef _PHRASES
#undef DAT
#endif
}} // namespace web::http
