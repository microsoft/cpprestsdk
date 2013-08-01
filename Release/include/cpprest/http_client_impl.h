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
* http_client_impl.h
*
* HTTP Library: Client-side APIs, non-public declarations used in the implementation.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "cpprest/http_helpers.h"
#ifdef _MS_WINDOWS
#if !defined(__cplusplus_winrt)
#include <winhttp.h>
#else
#include <Strsafe.h>
// Important for WP8
#define __WRL_NO_DEFAULT_LIB__
#include <wrl.h>
#include <msxml6.h>
using namespace std;
using namespace Platform;
using namespace Microsoft::WRL;
#endif

using namespace web; 
using namespace concurrency;
#else
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>
#include <pplx/threadpool.h>
#endif

using namespace utility;

#ifdef _MS_WINDOWS
# define CRLF _XPLATSTR("\r\n")
#else
# define CRLF std::string("\r\n")
#endif


using namespace web::http::details;

namespace web { namespace http { namespace client { namespace details
{
    const bool valid_chars [] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //16-31
        0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, //32-47
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, //48-63
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //64-79
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, //80-95
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //96-111
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0 //112-127
    };

#ifdef _MS_WINDOWS
            static const utility::char_t * get_with_body = _XPLATSTR("A GET or HEAD request should not have an entity body.");
#endif

#if (!defined(_MS_WINDOWS) || defined(__cplusplus_winrt))
            // Checks if the method contains any invalid characters
            static bool validate_method(const utility::string_t& method)
            {
                for (auto iter = method.begin(); iter != method.end(); iter++)
                {
                    char_t ch = *iter;
                        
                    if (size_t(ch) >= 128)
                        return false;
                        
                    if (!valid_chars[ch])
                        return false;
                }

                return true;
            }
#endif

            // Helper function to trim leading and trailing null characters from a string.
            static void trim_nulls(utility::string_t &str)
            {
                size_t index;
                for(index = 0; index < str.size() && str[index] == 0; ++index);
                str.erase(0, index);
                for(index = str.size(); index > 0 && str[index - 1] == 0; --index);
                str.erase(index);
            }

            // Flatten the http_headers into a name:value pairs separated by a carriage return and line feed.
            static utility::string_t flatten_http_headers(const http_headers &headers)
            {
                utility::string_t flattened_headers;
                for(auto iter = headers.begin(); iter != headers.end(); ++iter)
                {
                    utility::string_t temp(iter->first + _XPLATSTR(":") + iter->second + CRLF);
                    flattened_headers.append(utility::string_t(temp.begin(), temp.end()));
                }
                return flattened_headers;
            }

#ifdef _MS_WINDOWS
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
                        headers[key] = value;
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
                    m_received_hdrs = true;
                    m_request_completion.set(m_response);
                }

                /// <summary>
                /// Completes this request, setting the underlying task completion event, and cleaning up the handles
                /// </summary>
                void complete_request(size_t body_size)
                {
                    m_response.set_error_code(0);
                    m_response._get_impl()->_complete(body_size);

                    finish();

                    delete this;
                }

#ifdef _MS_WINDOWS
                // Helper function to report an error, set task completion event, and close the request handle.

                void report_error(const utility::string_t & errorMessage)
                {
                    report_exception(http_exception(errorMessage));
                }

#endif

                void report_error(unsigned long error_code, const utility::string_t & errorMessage)
                {
                    m_response.set_error_code(error_code);
                    report_exception(http_exception((int)m_response.error_code(), errorMessage));
                }

                template<typename _ExceptionType>
                void report_exception(_ExceptionType e)
                {
                    report_exception(std::make_exception_ptr(e));
                }

                void report_exception(std::exception_ptr exceptionPtr)
                {
                    auto recvd_hdrs = m_received_hdrs;
                    auto response_impl = m_response._get_impl();
                    auto request_completion = m_request_completion;

                    if ( recvd_hdrs )
                    {
                        // Complete the request with an exception
                        response_impl->_complete(0, exceptionPtr);
                    }
                    else
                    {
                        // Complete the headers with an exception
                        request_completion.set_exception(exceptionPtr);

                        // Complete the request with no msg body. The exception
                        // should only be propagated to one of the tce.
                        response_impl->_complete(0);
                    }

                    finish();

                    delete this;
                }

                concurrency::streams::streambuf<uint8_t> _get_readbuffer()
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

                // request/response pair.
                http_request m_request;
                http_response m_response;

                size64_t m_uploaded;
                size64_t m_downloaded;

                size_t m_total_response_size;

                bool m_received_hdrs;

                std::exception_ptr m_exceptionPtr;

                // task completion event to signal request is completed.
                pplx::task_completion_event<http_response> m_request_completion;

                // Reference to the http_client implementation.
                std::shared_ptr<_http_client_communicator> m_http_client;

                virtual void cleanup() = 0;

            protected:

                request_context(std::shared_ptr<_http_client_communicator> client, http_request &request)
                    : m_http_client(client), 
                      m_request(request), 
                      m_total_response_size(0), 
                      m_received_hdrs(false), 
                      m_exceptionPtr(), 
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

                void finish();
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

                // Asychronously send a HTTP request and process the response.
                void async_send_request(request_context *request)
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
                    pplx::extensibility::scoped_critical_section_t l(m_open_lock);

                    --m_scheduled;

                    if( !m_requests_queue.empty())
                    {
                        request_context *request = m_requests_queue.front();
                        m_requests_queue.pop();

                        // Schedule a task to start sending.
                        pplx::create_task([this, request]
                        {
                            open_and_send_request(request);
                        });
                    }
                }

                const http_client_config& client_config() const
                {
                    return m_client_config;
                }

            protected:
                _http_client_communicator(const http::uri &address, const http_client_config& client_config)
                    : m_uri(address), m_client_config(client_config), m_opened(false), m_scheduled(0)
                {
                }

                // Method to open client.
                virtual unsigned long open() = 0;

                // HTTP client implementations must implement send_request.
                virtual void send_request(_In_ request_context *request) = 0;

                // URI to connect to.
                const http::uri m_uri;

            private:

                http_client_config m_client_config;

                bool m_opened;

                pplx::extensibility::critical_section_t m_open_lock;

                // Wraps opening the client around sending a request.
                void open_and_send_request(request_context *request)
                {
                    // First see if client needs to be opened.
                    auto error = open_if_required();

                    if (error != S_OK)
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
                    unsigned long error = S_OK;

                    if( !m_opened )
                    {
                        pplx::extensibility::scoped_critical_section_t l(m_open_lock);

                        // Check again with the lock held
                        if ( !m_opened )
                        {
                            error = open();

                            if (error == S_OK)
                            {
                                m_opened = true;
                            }
                        }
                    }

                    return error;
                }

                void push_request(_In_opt_ request_context *request)
                {
                    if (request == nullptr) return;

                    pplx::extensibility::scoped_critical_section_t l(m_open_lock);

                    if(++m_scheduled == 1)
                    {
                        // Schedule a task to start sending.
                        pplx::create_task([this, request]() -> void
                        {
                            open_and_send_request(request);
                        });
                    }
                    else
                    {
                        m_requests_queue.push(request);
                    }
                }

                // Queue used to guarantee ordering of requests, when appliable.
                std::queue<request_context *> m_requests_queue;
                int                           m_scheduled;
            };

            inline void request_context::finish()
            {
                m_http_client->finish_request();
            }

}}}} // namespaces
