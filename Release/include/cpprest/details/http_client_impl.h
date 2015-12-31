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
* HTTP Library: Client-side APIs, non-public declarations used in the implementation.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "cpprest/details/basic_types.h"
#include "cpprest/details/http_helpers.h"

#ifdef _WIN32
# define CRLF _XPLATSTR("\r\n")
#else
# define CRLF std::string("\r\n")
#endif

namespace web { namespace http { namespace client { namespace details
{

#ifdef _WIN32
static const utility::char_t * get_with_body = _XPLATSTR("A GET or HEAD request should not have an entity body.");

// Helper function to trim leading and trailing null characters from a string.
static void trim_nulls(utility::string_t &str)
{
    size_t index;
    for(index = 0; index < str.size() && str[index] == 0; ++index);
    str.erase(0, index);
    for(index = str.size(); index > 0 && str[index - 1] == 0; --index);
    str.erase(index);
}

#endif

// Flatten the http_headers into a name:value pairs separated by a carriage return and line feed.
static utility::string_t flatten_http_headers(const http_headers &headers)
{
    utility::string_t flattened_headers;
    for(auto iter = headers.begin(); iter != headers.end(); ++iter)
    {
        flattened_headers.append(iter->first);
        flattened_headers.push_back(':');
        flattened_headers.append(iter->second);
        flattened_headers.append(CRLF);
    }
    return flattened_headers;
}

#ifdef _WIN32
/// <summary>
/// Parses a string containing Http headers.
/// </summary>
static void parse_headers_string(_Inout_z_ utf16char *headersStr, http_headers &headers)
{
    utf16char *context = nullptr;
    utf16char *line = wcstok_s(headersStr, CRLF, &context);
    while(line != nullptr)
    {
        const utility::string_t header_line(line);
        const size_t colonIndex = header_line.find_first_of(_XPLATSTR(":"));
        if(colonIndex != utility::string_t::npos)
        {
            utility::string_t key = header_line.substr(0, colonIndex);
            utility::string_t value = header_line.substr(colonIndex + 1, header_line.length() - colonIndex - 1);
            http::details::trim_whitespace(key);
            http::details::trim_whitespace(value);
            headers.add(key, value);
        }
        line = wcstok_s(nullptr, CRLF, &context);
    }
}
#endif

class _http_client_communicator;

// Request context encapsulating everything necessary for creating and responding to a request.
class request_context
{
public:

    // Destructor to clean up any held resources.
    virtual ~request_context()
    {
    }

    void complete_headers()
    {
        // We have already read (and transmitted) the request body. Should we explicitly close the stream?
        // Well, there are test cases that assumes that the istream is valid when t receives the response!
        // For now, we will drop our reference which will close the stream if the user doesn't have one.
        m_request.set_body(Concurrency::streams::istream());
        m_request_completion.set(m_response);
    }

    /// <summary>
    /// Completes this request, setting the underlying task completion event, and cleaning up the handles
    /// </summary>
    void complete_request(utility::size64_t body_size)
    {
        m_response._get_impl()->_complete(body_size);

        finish();
    }

    void report_error(unsigned long error_code, const std::string &errorMessage)
    {
        report_exception(http_exception(static_cast<int>(error_code), errorMessage));
    }

#ifdef _WIN32
    void report_error(unsigned long error_code, const std::wstring &errorMessage)
    {
        report_exception(http_exception(static_cast<int>(error_code), errorMessage));
    }
#endif

    template<typename _ExceptionType>
    void report_exception(const _ExceptionType &e)
    {
        report_exception(std::make_exception_ptr(e));
    }

    virtual void report_exception(std::exception_ptr exceptionPtr)
    {
        auto response_impl = m_response._get_impl();

        // If cancellation has been triggered then ignore any errors.
        if(m_request._cancellation_token().is_canceled())
        {
            exceptionPtr = std::make_exception_ptr(http_exception((int)std::errc::operation_canceled, std::generic_category()));
        }

        // First try to complete the headers with an exception.
        if(m_request_completion.set_exception(exceptionPtr))
        {
            // Complete the request with no msg body. The exception
            // should only be propagated to one of the tce.
            response_impl->_complete(0);
        }
        else
        {
            // Complete the request with an exception
            response_impl->_complete(0, exceptionPtr);
        }

        finish();
    }

    virtual concurrency::streams::streambuf<uint8_t> _get_readbuffer()
    {
        auto instream = m_request.body();

        _ASSERTE((bool)instream);
        return instream.streambuf();
    }

    concurrency::streams::streambuf<uint8_t> _get_writebuffer()
    {
        auto outstream = m_response._get_impl()->outstream();

        _ASSERTE((bool)outstream);
        return outstream.streambuf();
    }

    // Reference to the http_client implementation.
    std::shared_ptr<_http_client_communicator> m_http_client;

    // request/response pair.
    http_request m_request;
    http_response m_response;

    utility::size64_t m_uploaded;
    utility::size64_t m_downloaded;

    // task completion event to signal request is completed.
    pplx::task_completion_event<http_response> m_request_completion;

    // Registration for cancellation notification if enabled.
    pplx::cancellation_token_registration m_cancellationRegistration;

protected:

    request_context(const std::shared_ptr<_http_client_communicator> &client, const http_request &request)
        : m_http_client(client),
        m_request(request),
        m_uploaded(0),
        m_downloaded(0)
    {
        auto responseImpl = m_response._get_impl();

        // Copy the user specified output stream over to the response
        responseImpl->set_outstream(request._get_impl()->_response_stream(), false);

        // Prepare for receiving data from the network. Ideally, this should be done after
        // we receive the headers and determine that there is a response body. We will do it here
        // since it is not immediately apparent where that would be in the callback handler
        responseImpl->_prepare_to_receive_data();
    }

    virtual void finish();
};

//
// Interface used by client implementations. Concrete implementations are responsible for
// sending HTTP requests and receiving the responses.
//
class _http_client_communicator
{
public:

    // Destructor to clean up any held resources.
    virtual ~_http_client_communicator() {}

    // Asynchronously send a HTTP request and process the response.
    void async_send_request(const std::shared_ptr<request_context> &request)
    {
        if(m_client_config.guarantee_order())
        {
            // Send to call block to be processed.
            push_request(request);
        }
        else
        {
            // Schedule a task to start sending.
            pplx::create_task([this, request]
            {
                open_and_send_request(request);
            });
        }
    }

    void finish_request()
    {
        // If guarantee order is specified we don't need to do anything.
        if(m_client_config.guarantee_order())
        {
            pplx::extensibility::scoped_critical_section_t l(m_open_lock);

            --m_scheduled;

            if( !m_requests_queue.empty())
            {
                auto request = m_requests_queue.front();
                m_requests_queue.pop();

                // Schedule a task to start sending.
                pplx::create_task([this, request]
                {
                    open_and_send_request(request);
                });
            }
        }
    }

    const http_client_config& client_config() const
    {
        return m_client_config;
    }

    const uri & base_uri() const
    {
        return m_uri;
    }

protected:
    _http_client_communicator(http::uri address, http_client_config client_config)
        : m_uri(std::move(address)), m_client_config(std::move(client_config)), m_opened(false), m_scheduled(0)
    {
    }

    // Method to open client.
    virtual unsigned long open() = 0;

    // HTTP client implementations must implement send_request.
    virtual void send_request(_In_ const std::shared_ptr<request_context> &request) = 0;

    // URI to connect to.
    const http::uri m_uri;

private:

    http_client_config m_client_config;

    bool m_opened;

    pplx::extensibility::critical_section_t m_open_lock;

    // Wraps opening the client around sending a request.
    void open_and_send_request(const std::shared_ptr<request_context> &request)
    {
        // First see if client needs to be opened.
        auto error = open_if_required();

        if (error != 0)
        {
            // Failed to open
            request->report_error(error, _XPLATSTR("Open failed"));

            // DO NOT TOUCH the this pointer after completing the request
            // This object could be freed along with the request as it could
            // be the last reference to this object
            return;
        }

        send_request(request);
    }

    unsigned long open_if_required()
    {
        unsigned long error = 0;

        if(!m_opened)
        {
            pplx::extensibility::scoped_critical_section_t l(m_open_lock);

            // Check again with the lock held
            if (!m_opened)
            {
                error = open();

                if (error == 0)
                {
                    m_opened = true;
                }
            }
        }

        return error;
    }

    void push_request(const std::shared_ptr<request_context> &request)
    {
        pplx::extensibility::scoped_critical_section_t l(m_open_lock);

        if(++m_scheduled == 1)
        {
            // Schedule a task to start sending.
            pplx::create_task([this, request]()
            {
                open_and_send_request(request);
            });
        }
        else
        {
            m_requests_queue.push(request);
        }
    }

    // Queue used to guarantee ordering of requests, when applicable.
    std::queue<std::shared_ptr<request_context>> m_requests_queue;
    int m_scheduled;
};

inline void request_context::finish()
{
    // If cancellation is enabled and registration was performed, unregister.
    if(m_cancellationRegistration != pplx::cancellation_token_registration())
    {
        _ASSERTE(m_request._cancellation_token() != pplx::cancellation_token::none());
        m_request._cancellation_token().deregister_callback(m_cancellationRegistration);
    }

    m_http_client->finish_request();
}

class http_network_handler : public http_pipeline_stage
{
public:
    http_network_handler(const uri &base_uri, const http_client_config &client_config);

    virtual pplx::task<http_response> propagate(http_request request);

    const std::shared_ptr<details::_http_client_communicator>& http_client_impl() const
    {
        return m_http_client_impl;
    }

private:
    std::shared_ptr<_http_client_communicator> m_http_client_impl;
};

// Helper function to check to make sure the uri is valid.
void verify_uri(const uri &uri)
{
    // Some things like proper URI schema are verified by the URI class.
    // We only need to check certain things specific to HTTP.
    if (uri.scheme() != _XPLATSTR("http") && uri.scheme() != _XPLATSTR("https"))
    {
        throw std::invalid_argument("URI scheme must be 'http' or 'https'");
    }

    if(uri.host().empty())
    {
        throw std::invalid_argument("URI must contain a hostname.");
    }
}

} // namespace details

http_client::http_client(const uri &base_uri)
{
    build_pipeline(base_uri, http_client_config());
}

http_client::http_client(const uri &base_uri, const http_client_config &client_config)
{
    build_pipeline(base_uri, client_config);
}

void http_client::build_pipeline(const uri &base_uri, const http_client_config &client_config)
{
    if (base_uri.scheme().empty())
    {
        auto uribuilder = uri_builder(base_uri);
        uribuilder.set_scheme(_XPLATSTR("http"));
        uri uriWithScheme = uribuilder.to_uri();
        details::verify_uri(uriWithScheme);
        m_pipeline = ::web::http::http_pipeline::create_pipeline(std::make_shared<details::http_network_handler>(uriWithScheme, client_config));
    }
    else
    {
        details::verify_uri(base_uri);
        m_pipeline = ::web::http::http_pipeline::create_pipeline(std::make_shared<details::http_network_handler>(base_uri, client_config));
    }

#if !defined(CPPREST_TARGET_XP)
    add_handler(std::static_pointer_cast<http::http_pipeline_stage>(
        std::make_shared<oauth1::details::oauth1_handler>(client_config.oauth1())));
#endif

    add_handler(std::static_pointer_cast<http::http_pipeline_stage>(
        std::make_shared<oauth2::details::oauth2_handler>(client_config.oauth2())));
}

const http_client_config & http_client::client_config() const
{
    auto ph = std::static_pointer_cast<details::http_network_handler>(m_pipeline->last_stage());
    return ph->http_client_impl()->client_config();
}

const uri & http_client::base_uri() const
{
    auto ph = std::static_pointer_cast<details::http_network_handler>(m_pipeline->last_stage());
    return ph->http_client_impl()->base_uri();
}

}}} // namespaces
