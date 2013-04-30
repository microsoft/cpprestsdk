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
* http_client.cpp
*
* HTTP Library: Client-side APIs.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "http_helpers.h"

#ifdef _MS_WINDOWS
#if !defined(__cplusplus_winrt)
#include <winhttp.h>
#else
#include <Strsafe.h>
#include <wrl.h>
#include <msxml6.h>
using namespace std;
using namespace Platform;
using namespace Microsoft::WRL;
#endif

using namespace web; 
using namespace utility;
using namespace concurrency;
#else
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <threadpool.h>
#endif

#ifdef _MS_WINDOWS
# define CRLF U("\r\n")
#else
# define CRLF std::string("\r\n")
#endif

using namespace web::http::details;

namespace web { namespace http
{
    namespace client
    {
        namespace details
        {

#ifdef _MS_WINDOWS
            static const utility::char_t * get_with_body = U("A GET or HEAD request should not have an entity body.");
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
                    utility::string_t temp(iter->first + U(":") + iter->second + CRLF);
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
                    const size_t colonIndex = header_line.find_first_of(U(":"));
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
                virtual ~request_context() {}

                // TFS 579619 - 1206: revisit whether error_code is really needed for Linux
                void complete_headers(const unsigned long error_code = 0)
                {
                    // We have already read (and transmitted) the request body. Should we explicitly close the stream?
                    // Well, there are test cases that assumes that the istream is valid when t receives the response!
                    // For now, we will drop our reference which will close the stream if the user doesn't have one.
                    m_request.set_body(Concurrency::streams::istream());

                    m_received_hdrs = true;
                    m_response.set_error_code(error_code);

                    if(m_response.error_code() == 0)
                    {
                        m_request_completion.set(m_response);
                    }
#ifdef _MS_WINDOWS
                    else
                    {
                        m_request_completion.set_exception(http_exception(m_response.error_code()));
                    }
#else
                    else
                    {
                        m_request_completion.set_exception(http_exception((int)error_code, U("Error reading headers")));
                    }
#endif
                }

                /// <summary>
                /// Completes this request, setting the underlying task completion event, and cleaning up the handles
                /// </summary>
                void complete_request(size_t body_size)
                {
                    m_response.set_error_code(0);
                    m_response._get_impl()->_complete(body_size);

                    finish();

                    cleanup();
                }

#ifdef _MS_WINDOWS
                // Helper function to report an error, set task completion event, and close the request handle.

                // Note: I'm keeping the message parameter in case we decide to log errors again, or add the message
                //       to the exception message. For now.
                void report_error(const utility::string_t & errorMessage)
                {
                    report_error(GetLastError(), errorMessage);
                }

#endif

                void report_error(unsigned long error_code, const utility::string_t & errorMessage)
                {
                    UNREFERENCED_PARAMETER(errorMessage);

                    m_response.set_error_code(error_code);
                    report_exception(http_exception((int)m_response.error_code()));
                }

                template<typename _ExceptionType>
                void report_exception(_ExceptionType e)
                {
                    report_exception(std::make_exception_ptr(e));
                }

                void report_exception(std::exception_ptr exceptionPtr)
                {
                    if ( m_received_hdrs )
                    {
                        // Complete the request with an exception
                        m_response._get_impl()->_complete(0, exceptionPtr);
                    }
                    else
                    {
                        // Complete the headers with an exception
                        m_request_completion.set_exception(exceptionPtr);

                        // Complete the request with no msg body. The exception
                        // should only be propagated to one of the tce.
                        m_response._get_impl()->_complete(0);
                    }

                    finish();

                    cleanup();
                }

                concurrency::streams::streambuf<uint8_t> _get_readbuffer()
                {
                    auto instream = m_request.body();

                    _ASSERTE((bool)instream);
                    _ASSERTE(instream.is_open());

                    return instream.streambuf();
                }

                concurrency::streams::streambuf<uint8_t> _get_writebuffer()
                {
                    auto outstream = m_response._get_impl()->outstream();

                    _ASSERTE((bool)outstream);
                    _ASSERTE(outstream.is_open());

                    return outstream.streambuf();
                }

                // request/response pair.
                http_request m_request;
                http_response m_response;

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
                    : m_http_client(client), m_request(request), m_total_response_size(0), m_received_hdrs(false), m_exceptionPtr()
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
                virtual void send_request(request_context *request) = 0;

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
                        request->report_error(error, U("Open failed"));

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

#if defined(__cplusplus_winrt)

            /// <summary>
            /// This class acts as a bridge between the underlying request and response streams, allowing the
            /// WinRT implementation to do all the work on our behalf and simplifying both the sending and
            /// receiving actions.
            /// </summary>
            /// <remarks>
            /// These operations are completely synchronous, so it's important to block on both
            /// read and write operations. The I/O will be done off the UI thread, so there is no risk
            /// of causing the UI to become unresponsive.
            /// This class has the appearance of being general-purpose, but it does have functionality that
            /// is specific to the needs of the http client: it counts bytes written.
            /// </remarks>
            class ISequentialStream_bridge
                : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<ClassicCom>, ISequentialStream>
            {
            public:
                ISequentialStream_bridge(streams::streambuf<uint8_t> buf, request_context *request, size_t read_length = std::numeric_limits<size_t>::max())
                    : m_buffer(buf),
                    m_request(request),
                    m_read_length(read_length),
                    m_bytes_written(0)
                {
                    // read_length is the initial length of the ISequentialStream that is avaiable for read
                    // This is required because IXHR2 attempts to read more data that what is specified by
                    // the content_length. (Specifically, it appears to be reading 128K chunks regardless of
                    // the content_length specified).
                }

                // ISequentialStream implementation
                virtual HRESULT STDMETHODCALLTYPE Read( _Out_writes_ (cb) void *pv, _In_ ULONG cb, _Out_ ULONG *pcbRead)
                {
                    if ( pcbRead != nullptr )
                        *pcbRead = 0;

                    try
                    {
                        // Do not read more than the specified read_length
                        msl::utilities::SafeInt<size_t> safe_count = static_cast<size_t>(cb);
                        size_t size_to_read = safe_count.Min(m_read_length);
                        
                        size_t count = m_buffer.getn((uint8_t *)pv, size_to_read).get();
                        
                        if (count == 0 && size_to_read != 0)
                        {
                            *pcbRead = (ULONG)count;
                            return (HRESULT)STG_E_READFAULT;
                        }
                        
                        _ASSERTE(count != static_cast<size_t>(-1));

                        if (pcbRead != nullptr)
                        {
                            *pcbRead = (ULONG)count;

                            if (m_read_length != std::numeric_limits<size_t>::max())
                            {
                                _ASSERTE(m_read_length >= count);
                                m_read_length -= count;
                            }
                        }

                        return S_OK;
                    }
                    catch(...)
                    {
                        m_request->m_exceptionPtr = std::current_exception();
                        return (HRESULT)STG_E_READFAULT;
                    }
                }

                virtual HRESULT STDMETHODCALLTYPE Write( const void *pv, ULONG cb, ULONG *pcbWritten )
                {
                    if ( pcbWritten != nullptr )
                        *pcbWritten = 0;

                    try
                    {
                        if ( cb == 0 )
                        {
                            m_buffer.sync().wait();
                            return S_OK;
                        }

                        size_t count = m_buffer.putn((const uint8_t *)pv, (size_t)cb).get();

                        _ASSERTE(count != static_cast<size_t>(-1));

                        if (pcbWritten != nullptr )
                        {
                            _ASSERTE(count <= static_cast<size_t>(ULONG_MAX));
                            *pcbWritten = (ULONG)count;
                        }
                        m_bytes_written += count;

                        return S_OK;
                    }
                    catch (const http_exception &exc)
                    {
                        return (HRESULT)exc.error_code().value();
                    }
                    catch(const std::system_error &exc)
                    {
                        return (HRESULT)exc.code().value();
                    }
                }

                size_t total_bytes() const
                {
                    return m_bytes_written;
                }

            private:
                concurrency::streams::streambuf<uint8_t> m_buffer;
                
                request_context *m_request;

                // Final length of the ISequentialStream after writes
                size_t m_bytes_written;

                // Length of the ISequentialStream for reads. This is equivalent
                // to the amount of data that the ISequentialStream is allowed
                // to read from the underlying streambuffer.
                size_t m_read_length;
            };

            // Additional information necessary to track a WinRT request.
            class winrt_request_context : public request_context
            {
            public:

                // Factory function to create requests on the heap.
                static request_context * create_request_context(std::shared_ptr<_http_client_communicator> client, http_request &request)
                {
                    return new winrt_request_context(client, request);
                }

                IXMLHTTPRequest2 *m_hRequest;

                Microsoft::WRL::ComPtr<ISequentialStream_bridge> m_stream_bridge;

                virtual void cleanup()
                {
                    // Unlike WinHTTP, we can delete the object right away
                    delete this;
                }

            private:

                // Request contexts must be created through factory function.
                winrt_request_context(std::shared_ptr<_http_client_communicator> client, http_request &request) 
                    : request_context(client, request), m_hRequest(nullptr) {
                }

            };

            // Implementation of IXMLHTTPRequest2Callback.
            class HttpRequestCallback :
                public RuntimeClass<RuntimeClassFlags<ClassicCom>, IXMLHTTPRequest2Callback, FtmBase>
            {
            public:
                HttpRequestCallback(winrt_request_context *request)
                    : m_request(request)
                {
                    AddRef();
                }

                //
                // IXMLHTTPRequest2Callback methods

                // Called when the HTTP request is being redirected to a new URL.
                HRESULT STDMETHODCALLTYPE OnRedirect(_In_opt_ IXMLHTTPRequest2*, const WCHAR*) {
                    return S_OK;
                }

                // Called when HTTP headers have been received and processed.
                HRESULT STDMETHODCALLTYPE OnHeadersAvailable(_In_ IXMLHTTPRequest2* xmlReq, DWORD dw, const WCHAR* phrase)
                {
                    http_response response = m_request->m_response;
                    response.set_status_code((http::status_code)dw);
                    response.set_reason_phrase(phrase);

                    utf16char *hdrStr = nullptr;
                    HRESULT hr = xmlReq->GetAllResponseHeaders(&hdrStr);

                    if ( hr == S_OK )
                    {
                        parse_headers_string(hdrStr, response.headers());
                        m_request->complete_headers();
                    }
                    else
                    {
                        m_request->report_error(hr, L"Failure getting response headers");
                    }

                    return hr;
                }

                // Called when a portion of the entity body has been received.
                HRESULT STDMETHODCALLTYPE OnDataAvailable(_In_opt_ IXMLHTTPRequest2*, _In_opt_ ISequentialStream* ) {
                    return S_OK;
                }

                // Called when the entire entity response has been received.
                HRESULT STDMETHODCALLTYPE OnResponseReceived(_In_opt_ IXMLHTTPRequest2*, _In_opt_ ISequentialStream* )
                {
                    if ( m_request != nullptr )
                    {
                        IXMLHTTPRequest2 * req = m_request->m_hRequest;

                        m_request->complete_request(m_request->m_stream_bridge->total_bytes());
                        m_request = nullptr;

                        if ( req != nullptr ) req->Release();
                    }

                    Release();

                    return S_OK;
                }

                // Called when an error occurs during the HTTP request.
                HRESULT STDMETHODCALLTYPE OnError(_In_opt_ IXMLHTTPRequest2*, HRESULT hrError)
                {
                    if ( m_request != nullptr)
                    {
                        IXMLHTTPRequest2 * req = m_request->m_hRequest;
                            
                        if (m_request->m_exceptionPtr != nullptr)
                            m_request->report_exception(m_request->m_exceptionPtr);
                        else    
                            m_request->report_error(hrError, L"HTTP");

                        m_request = nullptr;

                        if ( req != nullptr ) req->Release();
                    }

                    Release();

                    return S_OK;
                }

            private:
                winrt_request_context *m_request;
            };

            // WinRT client.
            class winrt_client : public _http_client_communicator
            {
            public:
                winrt_client(const http::uri &address, const http_client_config& client_config)
                    : _http_client_communicator(address, client_config) { }

            protected:

                // Method to open client.
                unsigned long open()
                {
                    return 0;
                }

                // Start sending request.
                void send_request(request_context *request)
                {
                    http_request &msg = request->m_request;
                    winrt_request_context * winrt_context = static_cast<winrt_request_context *>(request);

                    if ( msg.method() == http::methods::TRCE )
                    {
                        // Not supported by WinInet. Generate a more specific exception than what WinInet does.
                        request->m_request_completion.set_exception(http_exception(U("TRACE is not supported")));
                        return;
                    }

                    // Start sending HTTP request.
                    IXMLHTTPRequest2 *xhr;
                    HRESULT hr = CoCreateInstance(
                        __uuidof(FreeThreadedXMLHTTP60), 
                        nullptr, 
                        CLSCTX_INPROC, 
                        __uuidof(IXMLHTTPRequest2), 
                        reinterpret_cast<void**>(&xhr));

                    if ( !SUCCEEDED(hr) ) 
                    {
                        request->report_error(hr, L"Failure to create IXMLHTTPRequest2 instance");
                        return;
                    }

                    winrt_context->m_hRequest = xhr;    // No AddRef(), because this will soon be the only copy.

                    auto callback = Make<details::HttpRequestCallback>(winrt_context);

                    utility::string_t encoded_resource = http::uri_builder(m_uri).append(msg.relative_uri()).to_string();

                    const utility::char_t* usernanme = nullptr;
                    const utility::char_t* password = nullptr;
                    const utility::char_t* proxy_usernanme = nullptr;
                    const utility::char_t* proxy_password = nullptr;

                    auto& config = client_config();

                    auto client_cred = config.credentials();
                    if(client_cred.is_set())
                    {
                        usernanme = client_cred.username().c_str();
                        password  = client_cred.password().c_str();
                    }

                    auto proxy = config.proxy();
                    if(!proxy.is_default())
                    {
                        request->report_exception(http_exception(U("Only a default proxy server is supported")));
                        return;
                    }

                    auto proxy_cred = proxy.credentials();
                    if(proxy_cred.is_set())
                    {
                        proxy_usernanme = proxy_cred.username().c_str();
                        proxy_password  = proxy_cred.password().c_str();
                    }

                    hr = xhr->Open(msg.method().c_str(), encoded_resource.c_str(), callback.Get(), usernanme, password, proxy_usernanme, proxy_password);

                    if ( !SUCCEEDED(hr) ) 
                    {
                        request->report_error(hr, L"Failure to open HTTP request");
                        return;
                    }

                    // Suppress automatic prompts for user credentials, since they are already provided.
                    hr = xhr->SetProperty(XHR_PROP_NO_CRED_PROMPT, TRUE);
                    if(!SUCCEEDED(hr))
                    {
                        request->report_error(hr, L"Failure to set no credentials prompt property");
                        return;
                    }

                    auto timeout = config.timeout();
                    int secs = static_cast<int>(timeout.count());
                    hr = xhr->SetProperty(XHR_PROP_TIMEOUT, secs * 1000);

                    if ( !SUCCEEDED(hr) ) 
                    {
                        request->report_error(hr, L"Failure to set HTTP request properties");
                        return;
                    }

                    if(!msg.headers().empty())
                    {
                        for ( auto hdr = msg.headers().begin(); hdr != msg.headers().end(); hdr++ )
                        {
                            xhr->SetRequestHeader(hdr->first.c_str(), hdr->second.c_str());
                        }
                    }

                    // Establish the ISequentialStream interface that the runtime will use to write the
                    // response data directly into the underlying response stream.

                    auto writebuf = winrt_context->_get_writebuffer();

                    winrt_context->m_stream_bridge = Make<ISequentialStream_bridge>(writebuf, request);

                    hr = xhr->SetCustomResponseStream(winrt_context->m_stream_bridge.Get());

                    if ( !SUCCEEDED(hr) ) 
                    {
                        request->report_error(hr, L"Failure to set HTTP response stream");
                        return;
                    }

                    size_t content_length = msg._get_impl()->_get_content_length();

                    if (content_length == std::numeric_limits<size_t>::max())
                    {
                        // IXHR2 does not allow transfer encoding chunked. So the user is expected to set the content length
                        request->report_exception(http_exception(U("Content length is not specified in the http headers")));
                        return;
                    }

                    if (content_length == 0)
                    {
                        hr = xhr->Send(nullptr, 0);
                    }
                    else
                    {
                        if ( msg.method() == http::methods::GET || msg.method() == http::methods::HEAD )
                        {
                            request->report_exception(http_exception(get_with_body));
                            return;
                        }

                        auto readbuf = msg.body().streambuf();
                        auto str = Make<ISequentialStream_bridge>(readbuf, request, content_length);

                        hr = xhr->Send(str.Get(), content_length);
                    }

                    if ( !SUCCEEDED(hr) )
                    {
                        request->report_error(hr, L"Failure to send HTTP request");
                        return;
                    }
                }

            private:

                // No copy or assignment.
                winrt_client(const winrt_client&);
                winrt_client &operator=(const winrt_client&);
            };

#elif defined(WIN32)

            // Helper function to query for the size of header values.
            static void query_header_length(HINTERNET request_handle, DWORD header, DWORD &length)
            {
                WinHttpQueryHeaders(
                    request_handle,
                    header,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    WINHTTP_NO_OUTPUT_BUFFER,
                    &length,
                    WINHTTP_NO_HEADER_INDEX);
            }

            // Helper function to get the status code from a WinHTTP response.
            static http::status_code parse_status_code(HINTERNET request_handle)
            {
                DWORD length = 0;
                query_header_length(request_handle, WINHTTP_QUERY_STATUS_CODE, length);
                utility::string_t buffer;
                buffer.resize(length);
                WinHttpQueryHeaders(
                    request_handle,
                    WINHTTP_QUERY_STATUS_CODE,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &buffer[0],
                    &length,
                    WINHTTP_NO_HEADER_INDEX);
                return (unsigned short)_wtoi(buffer.c_str());
            }

            // Helper function to get the reason phrase from a WinHTTP response.
            static utility::string_t parse_reason_phrase(HINTERNET request_handle)
            {
                utility::string_t phrase;
                DWORD length = 0;

                query_header_length(request_handle, WINHTTP_QUERY_STATUS_TEXT, length);
                phrase.resize(length);
                WinHttpQueryHeaders(
                    request_handle,
                    WINHTTP_QUERY_STATUS_TEXT,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &phrase[0],
                    &length,
                    WINHTTP_NO_HEADER_INDEX);
                // WinHTTP reports back the wrong length, trim any null characters.
                trim_nulls(phrase);
                return phrase;
            }

            /// <summary>
            /// Parses a string containing Http headers.
            /// </summary>
            static void parse_winhttp_headers(HINTERNET request_handle, _In_z_ utf16char *headersStr, http_response &response)
            {
                // Status code and reason phrase.
                response.set_status_code(parse_status_code(request_handle));
                response.set_reason_phrase(parse_reason_phrase(request_handle));

                parse_headers_string(headersStr, response.headers());
            }

            // Helper function to build an error message from a WinHTTP async result.
            static std::string build_callback_error_msg(_In_ WINHTTP_ASYNC_RESULT *error_result)
            {
                std::string error_msg("Error in: ");
                switch(error_result->dwResult)
                {
                case API_RECEIVE_RESPONSE:
                    error_msg.append("WinHttpReceiveResponse");
                    break;
                case API_QUERY_DATA_AVAILABLE:
                    error_msg.append("WinHttpQueryDataAvaliable");
                    break;
                case API_READ_DATA:
                    error_msg.append("WinHttpReadData");
                    break;
                case API_WRITE_DATA:
                    error_msg.append("WinHttpWriteData");
                    break;
                case API_SEND_REQUEST:
                    error_msg.append("WinHttpSendRequest");
                    break;
                default:
                    error_msg.append("Unknown WinHTTP Function");
                    break;
                }
                return error_msg;
            }

            class winhttp_client;

            struct winhttp_send_request_data
            {
                bool                       m_proxy_authentication_tried;
                bool                       m_server_authentication_tried;
            };

            class memory_holder
            {
                uint8_t* m_data;
                bool m_internally_allocated;
                size_t m_allocated_size;
                bool released;

                void release_memory()
                {
                    if (m_internally_allocated)
                    {
                        assert(m_data != nullptr);
                        delete[] m_data;
                        released = true;
                    }
                }
            public:
                memory_holder()
                    : m_data(nullptr), m_internally_allocated(false), m_allocated_size(0), released(false)
                {
                }

                ~memory_holder()
                {
                    assert(released == false);
                    release_memory();
                }

                memory_holder& operator=(memory_holder&& holder)
                {
                    assert(released == false);
                    release_memory();

                    // Copy the varialbes
                    m_data = holder.m_data;
                    m_internally_allocated = holder.m_internally_allocated;
                    m_allocated_size = holder.m_allocated_size;

                    // Null out the variables of holder
                    holder.m_data = nullptr;
                    holder.m_allocated_size = 0;
                    holder.m_internally_allocated = false;

                    released = false;

                    return *this;
                }

                void allocate_space(size_t length)
                {
                    // Do not allocate if you already hold a piece of memory which is not less than the one asked for
                    if (m_internally_allocated && length <= m_allocated_size)
                    {
                        return;
                    }

                    release_memory();
                    m_data = new uint8_t[length];
                    m_allocated_size = length;
                    m_internally_allocated = true;
                }

                inline void reassign_to(_In_opt_ uint8_t *block)
                {
                    assert(block != nullptr);
                    release_memory();
                    m_data = block;
                    m_internally_allocated = false;
                }

                inline bool is_internally_allocated()
                {
                    assert(released == false);
                    return m_internally_allocated;
                }

                inline uint8_t* get()
                {
                    assert(released == false);
                    return m_data;
                }
            };

            // Additional information necessary to track a WinHTTP request.
            class winhttp_request_context : public request_context
            {
            public:

                // Factory function to create requests on the heap.
                static request_context * create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
                {
                    return new winhttp_request_context(client, request);
                }

                ~winhttp_request_context()
                {
                    cleanup();
                }

                void allocate_request_space(_In_opt_ uint8_t *block, size_t length)
                {
                    if (block == nullptr)
                        request_data.allocate_space(length);
                    else
                        request_data.reassign_to(block);
                }

                void allocate_reply_space(_In_opt_ uint8_t *block, size_t length)
                {
                    if (block == nullptr)
                        reply_data.allocate_space(length);
                    else
                        reply_data.reassign_to(block);
                }

                bool is_externally_allocated()
                {
                    return transfered ? !reply_data.is_internally_allocated() : !request_data.is_internally_allocated();
                }

                void transfer_space()
                {
                    transfered = true;
                    reply_data = static_cast<memory_holder&&>(request_data);
                }

                HINTERNET m_request_handle;
                winhttp_send_request_data * m_request_data;

                bool m_need_to_chunk;
                bool m_too_large;
                bool transfered;
                size_t m_remaining_to_write;

                size_t m_chunksize;

                std::char_traits<uint8_t>::pos_type m_readbuf_pos;

                memory_holder request_data;
                memory_holder reply_data;

            protected:

                virtual void cleanup()
                {
                    if(m_request_handle != nullptr)
                    {
                        auto tmp_handle = m_request_handle;
                        m_request_handle = nullptr;
                        WinHttpCloseHandle(tmp_handle);
                    }
                }

            private:

                // Can only create on the heap using factory function.
                winhttp_request_context(std::shared_ptr<_http_client_communicator> &client, http_request request)
                    : request_context(client, request), 
                    m_request_handle(nullptr), 
                    m_need_to_chunk(false), 
                    m_too_large(false),
                    m_chunksize(client->client_config().chunksize()),
                    m_readbuf_pos(0),
                    request_data(),
                    reply_data(),
                    m_remaining_to_write(0),
                    transfered(false)
                {
                }
            };

            static DWORD ChooseAuthScheme( DWORD dwSupportedSchemes )
            {
                //  It is the server's responsibility only to accept 
                //  authentication schemes that provide a sufficient
                //  level of security to protect the servers resources.
                //
                //  The client is also obligated only to use an authentication
                //  scheme that adequately protects its username and password.
                //
                if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NEGOTIATE )
                    return WINHTTP_AUTH_SCHEME_NEGOTIATE;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NTLM )
                    return WINHTTP_AUTH_SCHEME_NTLM;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_PASSPORT )
                    return WINHTTP_AUTH_SCHEME_PASSPORT;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_DIGEST )
                    return WINHTTP_AUTH_SCHEME_DIGEST;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_BASIC )
                    return WINHTTP_AUTH_SCHEME_BASIC;
                else
                    return 0;
            }

            // WinHTTP client.
            class winhttp_client : public _http_client_communicator
            {
            public:
                winhttp_client(const http::uri &address, const http_client_config& client_config) 
                    : _http_client_communicator(address, client_config), m_secure(address.scheme() == U("https")), m_hSession(nullptr), m_hConnection(nullptr) { }

                // Closes session.
                ~winhttp_client()
                {
                    if(m_hConnection != nullptr)
                    {
                        if(WinHttpCloseHandle(m_hConnection) == NULL)
                        {
                            report_failure(U("Error closing connection"));
                        }
                    }

                    if(m_hSession != nullptr)
                    {
                        // Unregister the callback.
                        if(!WinHttpSetStatusCallback(
                            m_hSession,
                            nullptr,
                            WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS,
                            NULL))
                        {
                            report_failure(U("Error unregistering callback"));
                        }

                        if(WinHttpCloseHandle(m_hSession) == NULL)
                        {
                            report_failure(U("Error closing session"));
                        }
                    }
                }

            protected:

                unsigned long report_failure(const utility::string_t& errorMessage)
                {
                    // Should we log?
                    UNREFERENCED_PARAMETER(errorMessage);

                    return GetLastError();
                }

                // Open session and connection with the server.
                unsigned long open()
                {
                    DWORD access_type;
                    LPCWSTR proxy_name;
                    utility::string_t proxy_str;

                    auto& config = client_config();

                    if(config.proxy().is_disabled())
                    {
                        access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
                        proxy_name = WINHTTP_NO_PROXY_NAME;
                    }
                    else if(config.proxy().is_default() || config.proxy().is_auto_discovery())
                    {
                        access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
                        proxy_name = WINHTTP_NO_PROXY_NAME;
                    }
                    else
                    {
                        _ASSERTE(config.proxy().is_specified());
                        access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
                        // WinHttpOpen cannot handle trailing slash in the name, so here is some string gymnastics to keep WinHttpOpen happy
                        // proxy_str is intentionally declared at the function level to avoid poiinting to the string in the destructed object
                        auto uri = config.proxy().address();
                        if(uri.is_port_default())
                        {
                            proxy_name = uri.host().c_str();
                        }
                        else
                        {
                            if (uri.port() > 0)
                            {
                                utility::ostringstream_t ss;
                                ss << uri.host() << U(":") << uri.port();
                                proxy_str = ss.str();
                            }
                            else
                                proxy_str = uri.host();
                            proxy_name = proxy_str.c_str();
                        }
                    }

                    // Open session.
                    m_hSession = WinHttpOpen(
                        NULL,
                        access_type,
                        proxy_name,
                        WINHTTP_NO_PROXY_BYPASS,
                        WINHTTP_FLAG_ASYNC);
                    if(!m_hSession)
                    {
                        return report_failure(U("Error opening session"));
                    }

                    // Set timeouts.
                    auto timeout = config.timeout();
                    int secs = static_cast<int>(timeout.count());

                    if(!WinHttpSetTimeouts(m_hSession, 
                        60000,
                        60000,
                        secs * 1000,
                        secs * 1000))
                    {
                        return report_failure(U("Error setting timeouts"));
                    }

                    if(config.guarantee_order())
                    {
                        // Set max connection to use per server to 1.
                        DWORD maxConnections = 1;
                        if(!WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConnections, sizeof(maxConnections)))
                        {
                            return report_failure(U("Error setting options"));
                        }
                    }

#if 0 // Work in progress. Enable this to support server certrificate revocation check
                    if( m_secure )
                    {
                        DWORD dwEnableSSLRevocOpt = WINHTTP_ENABLE_SSL_REVOCATION;
                        if(!WinHttpSetOption(m_hSession, WINHTTP_OPTION_ENABLE_FEATURE, &dwEnableSSLRevocOpt, sizeof(dwEnableSSLRevocOpt)))
                        {
                            DWORD dwError = GetLastError(); dwError;
                            return report_failure(U("Error enabling SSL revocation check"));
                        }
                    }
#endif
                    // Register asynchronous callback.
                    if(WINHTTP_INVALID_STATUS_CALLBACK == WinHttpSetStatusCallback(
                        m_hSession,
                        &winhttp_client::completion_callback,
                        WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES,
                        0))
                    {
                        return report_failure(U("Error registering callback"));
                    }

                    // Open connection.
                    unsigned int port = m_uri.is_port_default() ? (m_secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT) : m_uri.port();
                    m_hConnection = WinHttpConnect(
                        m_hSession,
                        m_uri.host().c_str(),
                        (INTERNET_PORT)port,
                        0);

                    if(m_hConnection == nullptr)
                    {
                        return report_failure(U("Error opening connection"));
                    }

                    return S_OK;
                }

                // Start sending request.
                void send_request(_In_ request_context *request)
                {
                    http_request &msg = request->m_request;
                    winhttp_request_context * winhttp_context = static_cast<winhttp_request_context *>(request);

                    // Need to form uri path, query, and fragment for this request.
                    // Make sure to keep any path that was specified with the uri when the http_client was created.
                    utility::string_t encoded_resource = http::uri_builder(m_uri).append(msg.relative_uri()).to_uri().resource().to_string();

                    WINHTTP_PROXY_INFO info;
                    bool proxy_info_required = false;

                    if( client_config().proxy().is_auto_discovery() )
                    {
                        WINHTTP_AUTOPROXY_OPTIONS autoproxy_options;
                        memset( &autoproxy_options, 0, sizeof(WINHTTP_AUTOPROXY_OPTIONS) );
                        memset( &info, 0, sizeof(WINHTTP_PROXY_INFO) );

                        autoproxy_options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
                        autoproxy_options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
                        autoproxy_options.fAutoLogonIfChallenged = TRUE;

                        auto result = WinHttpGetProxyForUrl(
                            m_hSession,
                            m_uri.to_string().c_str(),
                            &autoproxy_options,
                            &info );
                        if(result)
                        {
                            proxy_info_required = true;
                        }
                        else
                        {
                            // Failure to download the auto-configuration script is not fatal. Fall back to the default proxy.
                        }
                    }

                    // Open the request.
                    winhttp_context->m_request_handle = WinHttpOpenRequest(
                        m_hConnection,
                        msg.method().c_str(),
                        encoded_resource.c_str(),
                        nullptr,
                        WINHTTP_NO_REFERER,
                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                        WINHTTP_FLAG_ESCAPE_DISABLE | (m_secure ? WINHTTP_FLAG_SECURE : 0));
                    if(winhttp_context->m_request_handle == nullptr)
                    {
                        request->report_error(U("Error opening request"));
                        return;
                    }

                    if( proxy_info_required )
                    {
                        auto result = WinHttpSetOption(
                            winhttp_context->m_request_handle,
                            WINHTTP_OPTION_PROXY,
                            &info, 
                            sizeof(WINHTTP_PROXY_INFO) );
                        if(!result)
                        {
                            request->report_error(U("Error setting http proxy option"));
                            return;
                        }
                    }

                    // If credentials are specified, use autologon policy: WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH
                    //    => default credentials are not used.
                    // Else, the default autologon policy WINHTTP_AUTOLOGON_SECURITY_LEVEL_MEDIUM will be used.
                    if ( !client_config().credentials().username().empty() )
                    {
                        DWORD data = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;

                        auto result = WinHttpSetOption(
                            winhttp_context->m_request_handle,
                            WINHTTP_OPTION_AUTOLOGON_POLICY,
                            &data, 
                            sizeof(data));
                        if(!result)
                        {
                            request->report_error(U("Error setting autologon policy to WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH."));
                            return;
                        }
                    }

                    size_t content_length = msg._get_impl()->_get_content_length();

                    if (content_length > 0)
                    {
                        if ( msg.method() == http::methods::GET || msg.method() == http::methods::HEAD )
                        {
                            request->report_exception(http_exception(get_with_body));
                            return;
                        }

                        // There is a request body that needs to be transferred.

                        if (content_length == std::numeric_limits<size_t>::max()) 
                        {
                            // The content length is unknown and the application set a stream. This is an 
                            // indication that we will need to chunk the data.
                            winhttp_context->m_need_to_chunk = true;
                        }
                        else
                        {
                            // While we won't be transfer-encoding the data, we will write it in portions.
                            winhttp_context->m_too_large = true;
                            winhttp_context->m_remaining_to_write = content_length;
                        }
                    }


                    // Add headers.
                    if(!msg.headers().empty())
                    {
                        utility::string_t flattened_headers = flatten_http_headers(msg.headers());
                        if(!WinHttpAddRequestHeaders(
                            winhttp_context->m_request_handle,
                            flattened_headers.c_str(),
                            (DWORD)flattened_headers.length(),
                            WINHTTP_ADDREQ_FLAG_ADD))
                        {
                            request->report_error(U("Error adding request headers"));
                            return;
                        }
                    }

                    m_request_data.m_proxy_authentication_tried = false;
                    m_request_data.m_server_authentication_tried = false;

                    winhttp_context->m_request_data = &m_request_data;

                    _start_request_send(winhttp_context, content_length);

                    return;
                }

            private:

                void _start_request_send(_In_ winhttp_request_context * winhttp_context, size_t content_length)
                {
                    if ( !winhttp_context->m_need_to_chunk && !winhttp_context->m_too_large )
                    {
                        // We have a message that is small enough to send in one chunk.

                        if(!WinHttpSendRequest(
                            winhttp_context->m_request_handle,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            nullptr,
                            0,
                            0,
                            (DWORD_PTR)winhttp_context))
                        {
                            winhttp_context->report_error(U("Error starting to send request"));
                        }

                        winhttp_context->m_readbuf_pos = 0;
                        return;
                    }

                    // Capure the current read position of the stream.

                    winhttp_context->m_readbuf_pos = winhttp_context->_get_readbuffer().seekoff(0, std::ios_base::cur, std::ios_base::in);

                    // If we find ourselves here, we either don't know how large the message
                    // body is, or it is larger than our threshold.

                    if(!WinHttpSendRequest(
                        winhttp_context->m_request_handle,
                        WINHTTP_NO_ADDITIONAL_HEADERS,
                        0,
                        nullptr,
                        0,
                        winhttp_context->m_too_large ? 
                        (DWORD)content_length : 
                    WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH,
                        (DWORD_PTR)winhttp_context))
                    {
                        winhttp_context->report_error(U("Error starting to send chunked request"));
                    }
                }

                static void _transfer_encoding_chunked_write_data(_In_ winhttp_request_context * p_request_context)
                {
                    const size_t chunk_size = p_request_context->m_chunksize;

                    p_request_context->allocate_request_space(nullptr, chunk_size+http::details::chunked_encoding::additional_encoding_space);

                    auto rbuf = p_request_context->_get_readbuffer();

                    rbuf.getn(&p_request_context->request_data.get()[http::details::chunked_encoding::data_offset], chunk_size).then(
                        [p_request_context, chunk_size](pplx::task<size_t> op)
                    {
                        size_t read;
                        try { read = op.get(); }
                        catch (...)
                        {
                            p_request_context->report_exception(std::current_exception());
                            return;
                        }

                        _ASSERTE(read != static_cast<size_t>(-1));

                        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(p_request_context->request_data.get(), chunk_size+http::details::chunked_encoding::additional_encoding_space, read);

                        if ( read == 0 )
                            p_request_context->m_need_to_chunk = false;

                        auto length = read+(http::details::chunked_encoding::additional_encoding_space-offset);

                        if(!WinHttpWriteData(
                            p_request_context->m_request_handle,
                            &p_request_context->request_data.get()[offset],
                            (DWORD)length,
                            nullptr))
                        {
                            p_request_context->report_error(U("Error writing data"));
                        }
                    });
                }

                static void _multiple_segment_write_data(_In_ winhttp_request_context * p_request_context)
                {
                    auto rbuf = p_request_context->_get_readbuffer();

                    msl::utilities::SafeInt<size_t> safeCount = p_request_context->m_remaining_to_write;

                    uint8_t*  block = nullptr; 
                    size_t length = 0;
                    if ( rbuf.acquire(block, length) )
                    {
                        if ( length == 0 )
                        {
                            // Unexpected end-of-stream.
                            if (!(rbuf.exception() == nullptr))
                                p_request_context->report_exception(rbuf.exception());
                            else
                                p_request_context->report_error(U("Error reading outgoing HTTP body from its stream."));
                            return;
                        }

                        p_request_context->allocate_request_space(block, length);

                        size_t to_write = safeCount.Min(length);

                        p_request_context->m_remaining_to_write -= to_write;

                        if ( p_request_context->m_remaining_to_write == 0 )
                        {
                            p_request_context->m_too_large = false;
                        }

                        if( !WinHttpWriteData(
                            p_request_context->m_request_handle,
                            p_request_context->request_data.get(),
                            (DWORD)to_write,
                            nullptr))
                        {
                            p_request_context->report_error(U("Error writing data"));
                        }      

                        return;
                    }

                    const size_t chunk_size = p_request_context->m_chunksize;

                    size_t next_chunk_size = safeCount.Min(chunk_size);

                    p_request_context->allocate_request_space(nullptr, next_chunk_size);

                    rbuf.getn(p_request_context->request_data.get(), next_chunk_size).then(
                        [p_request_context, next_chunk_size](pplx::task<size_t> op)
                    {
                        size_t read;
                        try { read = op.get(); } catch (...)
                        {
                            p_request_context->report_exception(std::current_exception());
                            read = 0;
                            p_request_context->m_remaining_to_write = 0;
                        }

                        _ASSERTE(read != static_cast<size_t>(-1));

                        p_request_context->m_remaining_to_write -= read;

                        if ( read == 0 || p_request_context->m_remaining_to_write == 0 )
                        {
                            p_request_context->m_too_large = false;
                        }

                        if( !WinHttpWriteData(
                            p_request_context->m_request_handle,
                            p_request_context->request_data.get(),
                            (DWORD)read,
                            nullptr))
                        {
                            p_request_context->report_error(U("Error writing data"));
                        }       
                    });
                }

                // Returns true if the request was completed, or false if a new request with credentials was issued
                static bool handle_authentication_failure(
                    HINTERNET hRequestHandle,
                    _In_ winhttp_request_context * p_request_context
                    )
                {
                    http_response & response = p_request_context->m_response;
                    http_request & request = p_request_context->m_request;

                    _ASSERTE(response.status_code() == status_codes::Unauthorized  || response.status_code() == status_codes::ProxyAuthRequired);

                    // If the application set a stream for the request body, we can only resend if the input stream supports
                    // seeking and we are also successful in seeking to the position we started at when the original request
                    // was sent.

                    bool can_resend = false;
                    bool got_credentials = false;
                    BOOL results;
                    DWORD dwSupportedSchemes;
                    DWORD dwFirstScheme;
                    DWORD dwTarget = 0;
                    DWORD dwSelectedScheme = 0;
                    string_t username;
                    string_t password;

                    if (request.body())
                    {
                        // Valid request stream => msg has a body that needs to be resend

                        auto rdpos = p_request_context->m_readbuf_pos;

                        // Check if the saved read position is valid
                        if (rdpos != (std::char_traits<uint8_t>::pos_type)-1)
                        {
                            auto rbuf = p_request_context->_get_readbuffer();

                            // Try to seek back to the saved read position
                            if ( rbuf.seekpos(rdpos, std::ios::ios_base::in) == rdpos )
                            {
                                // Success.
                                can_resend = true;
                            }
                        }
                    }
                    else
                    {
                        // There is no msg body
                        can_resend = true;
                    }

                    if (can_resend)
                    {
                        // The proxy requires authentication.  Sending credentials...
                        // Obtain the supported and preferred schemes.
                        results = WinHttpQueryAuthSchemes( hRequestHandle, 
                            &dwSupportedSchemes, 
                            &dwFirstScheme, 
                            &dwTarget );

                        if (!results)
                        {
                            // This will return the authentication failure to the user, without reporting fatal errors
                            return true;
                        }

                        dwSelectedScheme = ChooseAuthScheme( dwSupportedSchemes);
                        if( dwSelectedScheme == 0 )
                        {
                            // This will return the authentication failure to the user, without reporting fatal errors
                            return true;
                        }

                        if(response.status_code() == status_codes::ProxyAuthRequired /*407*/ && !p_request_context->m_request_data->m_proxy_authentication_tried)
                        {
                            // See if the credentials on the proxy were set. If not, there are no credentials to supply hence we cannot resend
                            web_proxy proxy = p_request_context->m_http_client->client_config().proxy();
                            // No need to check if proxy is disabled, because disabled proxies cannot have credentials set on them
                            credentials cred = proxy.credentials();
                            if(cred.is_set())
                            {
                                username = cred.username();
                                password = cred.password();
                                dwTarget = WINHTTP_AUTH_TARGET_PROXY;
                                got_credentials = !username.empty();
                                p_request_context->m_request_data->m_proxy_authentication_tried = true;
                            }
                        }
                        else if(response.status_code() == status_codes::Unauthorized /*401*/ && !p_request_context->m_request_data->m_server_authentication_tried)
                        {
                            username = p_request_context->m_http_client->client_config().credentials().username();
                            password = p_request_context->m_http_client->client_config().credentials().password();
                            dwTarget = WINHTTP_AUTH_TARGET_SERVER;
                            got_credentials = !username.empty();
                            p_request_context->m_request_data->m_server_authentication_tried = true;
                        }
                    }

                    if( !(can_resend && got_credentials) )
                    {
                        // Either we cannot resend, or the user did not provide non-empty credentials.
                        // Return the authentication failure to the user.
                        return true;
                    }

                    results = WinHttpSetCredentials( hRequestHandle,
                        dwTarget, 
                        dwSelectedScheme,
                        username.c_str(),
                        password.c_str(),
                        nullptr );
                    if(!results)
                    {
                        // This will return the authentication failure to the user, without reporting fatal errors
                        return true;
                    }

                    // Figure out how the data should be sent, if any
                    size_t content_length = request._get_impl()->_get_content_length();

                    if (content_length > 0)
                    {
                        // There is a request body that needs to be transferred.

                        if (content_length == std::numeric_limits<size_t>::max()) 
                        {
                            // The content length is unknown and the application set a stream. This is an 
                            // indication that we will need to chunk the data.
                            p_request_context->m_need_to_chunk = true;
                        }
                        else
                        {
                            // While we won't be transfer-encoding the data, we will write it in portions.
                            p_request_context->m_too_large = true;
                            p_request_context->m_remaining_to_write = content_length;
                        }
                    }

                    // We're good.
                    winhttp_client* winclnt = reinterpret_cast<winhttp_client*>(p_request_context->m_http_client.get());
                    winclnt->_start_request_send(p_request_context, content_length);

                    // We will not complete the request. Instead wait for the response to the request that was resent
                    return false;
                }

                // Callback used with WinHTTP to listen for async completions.
                static void CALLBACK completion_callback(
                    HINTERNET hRequestHandle,
                    DWORD_PTR context,
                    DWORD statusCode,
                    _In_ void* statusInfo,
                    DWORD statusInfoLength)
                {
                    UNREFERENCED_PARAMETER(statusInfoLength);

                    winhttp_request_context * p_request_context = reinterpret_cast<winhttp_request_context *>(context);

                    if(p_request_context != nullptr)
                    {
                        switch (statusCode)
                        {
                        case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR :
                            {
                                WINHTTP_ASYNC_RESULT *error_result = reinterpret_cast<WINHTTP_ASYNC_RESULT *>(statusInfo);

                                p_request_context->report_error(error_result->dwError, utility::conversions::to_string_t(build_callback_error_msg(error_result)));
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE :
                            {
                                if ( p_request_context->m_need_to_chunk )
                                {
                                    _transfer_encoding_chunked_write_data(p_request_context);
                                }
                                else if ( p_request_context->m_too_large )
                                {
                                    _multiple_segment_write_data(p_request_context);
                                }
                                else 
                                {
                                    if(!WinHttpReceiveResponse(hRequestHandle, nullptr))
                                    {
                                        p_request_context->report_error(U("Error receiving response"));
                                    }
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE :
                            {
                                if ( p_request_context->is_externally_allocated() )
                                {
                                    _ASSERTE(statusInfoLength == sizeof(DWORD));
                                    DWORD bytesWritten = *((DWORD *)statusInfo);
                                    p_request_context->_get_readbuffer().release(p_request_context->request_data.get(), bytesWritten);
                                }

                                if ( p_request_context->m_need_to_chunk )
                                {
                                    _transfer_encoding_chunked_write_data(p_request_context);
                                }
                                else if ( p_request_context->m_too_large )
                                {
                                    _multiple_segment_write_data(p_request_context);
                                }
                                else
                                {
                                    if(!WinHttpReceiveResponse(hRequestHandle, nullptr))
                                    {
                                        p_request_context->report_error(U("Error receiving response"));
                                    }
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE :
                            {
                                // Clear the space allocated for the request.
                                p_request_context->transfer_space();

                                // First need to query to see what the headers size is.
                                DWORD headerBufferLength = 0;
                                query_header_length(hRequestHandle, WINHTTP_QUERY_RAW_HEADERS_CRLF, headerBufferLength);

                                // Now allocate buffer for headers and query for them.
                                std::vector<unsigned char> header_raw_buffer;
                                header_raw_buffer.resize(headerBufferLength);
                                utf16char * header_buffer = reinterpret_cast<utf16char *>(&header_raw_buffer[0]);
                                if(!WinHttpQueryHeaders(
                                    hRequestHandle,
                                    WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    header_buffer,
                                    &headerBufferLength,
                                    WINHTTP_NO_HEADER_INDEX))
                                {
                                    p_request_context->report_error(U("Error receiving http headers"));
                                    return;
                                }

                                http_response & response = p_request_context->m_response;
                                parse_winhttp_headers(hRequestHandle, header_buffer, response);

                                if(response.status_code() == status_codes::Unauthorized /*401*/ ||
                                    response.status_code() == status_codes::ProxyAuthRequired /*407*/)
                                {
                                    bool completed = handle_authentication_failure(hRequestHandle, p_request_context);
                                    if( !completed )
                                    {
                                        // The request was not completed but resent with credentials. Wait until we get a new response
                                        return;
                                    }
                                }

                                // Signal that the headers are available.
                                p_request_context->complete_headers();

                                // If the method was 'HEAD,' the body of the message is by definition empty. No need to 
                                // read it. Any headers that suggest the presence of a body can safely be ignored.
                                if (p_request_context->m_request.method() == methods::HEAD )
                                {
                                    p_request_context->allocate_request_space(nullptr, 0);
                                    p_request_context->complete_request(0);
                                    return;
                                }

                                size_t content_length = 0;
                                utility::string_t transfer_encoding;

                                bool has_cnt_length = response.headers().match(header_names::content_length, content_length);
                                bool has_xfr_encode = response.headers().match(header_names::transfer_encoding, transfer_encoding);

                                // Determine if there is a msg body that needs to be read
                                bool hasMsgBody = (has_cnt_length && (content_length > 0)) || (has_xfr_encode);

                                if (!hasMsgBody)
                                {
                                    p_request_context->complete_request(0);
                                    return;
                                }

                                // HTTP Specification states:
                                // If a message is received with both a Transfer-Encoding header field 
                                // and a Content-Length header field, the latter MUST be ignored.

                                // At least one of them should be set
                                _ASSERTE(has_xfr_encode || has_cnt_length);

                                // So we check for transfer-encoding first
                                if (has_xfr_encode || (content_length >= p_request_context->m_chunksize))
                                {
                                    // Read in as chunks.
                                    if(!WinHttpQueryDataAvailable(hRequestHandle, nullptr))
                                    {
                                        p_request_context->report_error(U("Error querying for http body data"));
                                    }
                                }
                                else
                                {
                                    // If Content-Length is specified then we can read in all at once.
                                    _ASSERTE(content_length > 0);

                                    concurrency::streams::streambuf<uint8_t> writebuf = p_request_context->_get_writebuffer();

                                    p_request_context->allocate_reply_space(writebuf.alloc(content_length), content_length);

                                    // Read in body all at once.
                                    if(!WinHttpReadData(
                                        hRequestHandle,
                                        (LPVOID)p_request_context->reply_data.get(),
                                        (DWORD)content_length,
                                        nullptr))
                                    {
                                        p_request_context->report_error(U("Error receiving http body"));
                                    }
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE :
                            {
                                // Status information contains pointer to DWORD containing number of bytes avaliable.
                                DWORD num_bytes = *(PDWORD)statusInfo;

                                if(num_bytes > 0)
                                {
                                    auto writebuf = p_request_context->_get_writebuffer();
                                    p_request_context->allocate_reply_space(writebuf.alloc(num_bytes), num_bytes);

                                    // Read in body all at once.
                                    if(!WinHttpReadData(
                                        hRequestHandle,
                                        (LPVOID)p_request_context->reply_data.get(),
                                        (DWORD)num_bytes,
                                        nullptr))
                                    {
                                        p_request_context->report_error(U("Error receiving http body chunk"));
                                    }
                                }
                                else
                                {
                                    p_request_context->complete_request(p_request_context->m_total_response_size);
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_READ_COMPLETE :
                            {
                                // Status information length contains the number of bytes read.
                                // WinHTTP will always fill the whole buffer or read nothing.
                                // If number of bytes read is zero than we have reached the end.
                                if(statusInfoLength > 0)
                                {
                                    auto writebuf = p_request_context->_get_writebuffer();

                                    auto after_sync = 
                                        [hRequestHandle, p_request_context]
                                    (pplx::task<void> sync_op)
                                    {
                                        try { sync_op.wait(); } catch(...)
                                        {
                                            p_request_context->report_exception(std::current_exception());
                                            return;
                                        }

                                        // Look for more data
                                        if( !WinHttpQueryDataAvailable(hRequestHandle, nullptr))
                                        {
                                            p_request_context->report_error(U("Error querying for http body chunk"));
                                        }
                                    };

                                    if ( p_request_context->is_externally_allocated() )
                                    {
                                        p_request_context->m_total_response_size += statusInfoLength;
                                        writebuf.commit(statusInfoLength);

                                        writebuf.sync().then(after_sync);
                                    }
                                    else 
                                    {
                                        writebuf.putn(p_request_context->reply_data.get(), statusInfoLength).then(
                                            [hRequestHandle, p_request_context, statusInfoLength, after_sync]
                                        (pplx::task<size_t> op)
                                        {
                                            size_t written = 0;
                                            try { written = op.get(); } catch(...)
                                            {
                                                p_request_context->report_exception(std::current_exception());
                                                return;
                                            }

                                            p_request_context->m_total_response_size += written;

                                            // If we couldn't write everything, it's time to exit.
                                            if ( written != statusInfoLength ) 
                                            {
                                                p_request_context->report_exception(std::runtime_error("response stream unexpectedly failed to write the requested number of bytes"));
                                                return;
                                            }

                                            auto wbuf = p_request_context->_get_writebuffer();

                                            wbuf.sync().then(after_sync);
                                        });
                                    }
                                }
                                else
                                {
                                    // Done reading so set task completion event and close the request handle.
                                    p_request_context->complete_request(p_request_context->m_total_response_size);
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
                            {
                                // Handle must already be closed and nulled out in cleanup
                                assert(p_request_context->m_request_handle == nullptr);
                                delete p_request_context;
                                break;
                            }
                        }
                    }
                }

                // WinHTTP session and connection
                HINTERNET m_hSession;
                HINTERNET m_hConnection;
                winhttp_send_request_data m_request_data;
                bool      m_secure;

                // No copy or assignment.
                winhttp_client(const winhttp_client&);
                winhttp_client &operator=(const winhttp_client&);
            };

#else

            using boost::asio::ip::tcp;

            class linux_client;
            struct client;

            class linux_request_context : public request_context
            {
            public:
                static request_context * create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
                {
                    return new linux_request_context(client, request);
                }

                void report_error(const utility::string_t &scope, boost::system::error_code ec)
                {
                    request_context::report_error(0x8000000 | ec.value(), scope);
                }

                std::unique_ptr<tcp::socket> m_socket;
                uri m_what;
                size_t m_known_size;
                size_t m_current_size;
                bool m_needChunked;
                boost::asio::streambuf m_request_buf;
                boost::asio::streambuf m_response_buf;
                std::unique_ptr<boost::asio::deadline_timer> m_timer;

                ~linux_request_context()
                {
                    if (m_timer)
                    {
                        m_timer->cancel();
                        m_timer.reset();
                    }

                    if (m_socket)
                    {
                        boost::system::error_code ignore;
                        m_socket->shutdown(tcp::socket::shutdown_both, ignore);
                        m_socket->close();
                        m_socket.reset();
                    }
                }

                void cancel(const boost::system::error_code& ec)
                {
                    if(!ec)
                    {
                        auto sock = m_socket.get();
                        if (sock != nullptr)
                        {
                            sock->cancel();
                        }
                    }
                }

            private:
                linux_request_context(std::shared_ptr<_http_client_communicator> &client, http_request request) 
                    : request_context(client, request)
                    , m_known_size(0)
                    , m_needChunked(false)
                    , m_current_size(0)
                {
                }

            protected:
                virtual void cleanup()
                {
                    delete this;
                }
            };


            struct client
            {
                client(boost::asio::io_service& io_service, size_t chunk_size)
                    : m_resolver(io_service)
                    , m_io_service(io_service)
                    , m_chunksize(chunk_size) {}

                void send_request(linux_request_context* ctx, int timeout)
                {
                    auto what = ctx->m_what;
                    ctx->m_socket.reset(new tcp::socket(m_io_service));

                    auto resource = what.resource().to_string();
                    if (resource == "") resource = "/";

                    auto method = ctx->m_request.method();
                    // stop injection of headers via method
                    // resource should be ok, since it's been encoded
                    // and host won't resolve
                    if (std::find(method.begin(), method.end(), '\r') != method.end())
                        throw std::runtime_error("invalid method string");

                    auto host = what.host();
                    std::ostream request_stream(&ctx->m_request_buf);

                    request_stream << method << " "
                        << resource << " "
                        << "HTTP/1.1" << CRLF
                        << "Host: " << host;

                    if (what.port() != 0 && what.port() != 80)
                        request_stream << ":" << what.port();

                    request_stream << CRLF;

                    // Check user specified transfer-encoding
                    std::string transferencoding;
                    if (ctx->m_request.headers().match(header_names::transfer_encoding, transferencoding) && transferencoding == "chunked")
                    {
                        ctx->m_needChunked = true;
                    }

                    // Stream without content length is the signal of requiring transcoding.
                    if (!ctx->m_request.headers().match(header_names::content_length, ctx->m_known_size))
                    {
                        if (ctx->m_request.body())
                        {
                            ctx->m_needChunked = true;
                            ctx->m_request.headers()[header_names::transfer_encoding] = U("chunked");
                        }
                        else
                        {
                            ctx->m_request.headers()[header_names::content_length] = U("0");
                        }
                    }

                    request_stream << flatten_http_headers(ctx->m_request.headers());

                    request_stream << "Connection: close" << CRLF; // so we can just read to EOF
                    request_stream << CRLF;

                    tcp::resolver::query query(host, utility::conversions::print_string(what.port() == 0 ? 80 : what.port()));

                    ctx->m_timer.reset(new boost::asio::deadline_timer(m_io_service));
                    ctx->m_timer->expires_from_now(boost::posix_time::milliseconds(timeout));
                    ctx->m_timer->async_wait(boost::bind(&linux_request_context::cancel, ctx, boost::asio::placeholders::error));

                    m_resolver.async_resolve(query, boost::bind(&client::handle_resolve, this, boost::asio::placeholders::error, boost::asio::placeholders::iterator, ctx));
                }

            private:
                boost::asio::io_service& m_io_service;
                tcp::resolver m_resolver;
                size_t m_chunksize;

                void handle_resolve(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, linux_request_context* ctx)
                {
                    if (ec)
                    {
                        ctx->report_error("Error resolving address", ec);
                    }
                    else
                    {
                        auto endpoint = *endpoints;
                        ctx->m_socket->async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
                }

                void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        boost::asio::async_write(*ctx->m_socket, ctx->m_request_buf, boost::bind(&client::handle_write_request, this, boost::asio::placeholders::error, ctx));
                    }
                    else if (endpoints == tcp::resolver::iterator())
                    {
                        ctx->report_error("Failed to connect to any resolved endpoint", ec);
                    }
                    else
                    {
                        boost::system::error_code ignore;
                        ctx->m_socket->shutdown(tcp::socket::shutdown_both, ignore);
                        ctx->m_socket->close();
                        ctx->m_socket.reset(new tcp::socket(m_io_service));
                        auto endpoint = *endpoints;
                        ctx->m_socket->async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
                }

                void handle_write_chunked_body(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (ec)
                        return handle_write_body(ec, ctx);
                    auto readbuf = ctx->_get_readbuffer();
                    uint8_t *buf = boost::asio::buffer_cast<uint8_t *>(ctx->m_request_buf.prepare(m_chunksize + http::details::chunked_encoding::additional_encoding_space));
                    readbuf.getn(buf + http::details::chunked_encoding::data_offset, m_chunksize).then([=](size_t readSize) {
                        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buf, m_chunksize+http::details::chunked_encoding::additional_encoding_space, readSize);
                        ctx->m_request_buf.commit(readSize + http::details::chunked_encoding::additional_encoding_space);
                        ctx->m_request_buf.consume(offset);
                        ctx->m_current_size += readSize;
                        boost::asio::async_write(*ctx->m_socket, ctx->m_request_buf,
                            boost::bind(readSize != 0 ? &client::handle_write_chunked_body : &client::handle_write_body, this, boost::asio::placeholders::error, ctx));
                    });
                }

                void handle_write_large_body(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (ec || ctx->m_current_size >= ctx->m_known_size)
                    {
                        return handle_write_body(ec, ctx);
                    }

                    auto readbuf = ctx->_get_readbuffer();
                    size_t readSize = std::min(m_chunksize, ctx->m_known_size - ctx->m_current_size);

                    readbuf.getn(boost::asio::buffer_cast<uint8_t *>(ctx->m_request_buf.prepare(readSize)), readSize).then([=](size_t actualSize) {
                        ctx->m_current_size += actualSize;
                        ctx->m_request_buf.commit(actualSize);
                        boost::asio::async_write(*ctx->m_socket, ctx->m_request_buf,
                            boost::bind(&client::handle_write_large_body, this, boost::asio::placeholders::error, ctx));
                    });
                }

                void handle_write_request(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        ctx->m_current_size = 0;

                        if (ctx->m_needChunked)
                            handle_write_chunked_body(ec, ctx);
                        else
                            handle_write_large_body(ec, ctx);
                    }
                    else
                    {
                        ctx->report_error("Failed to write request headers", ec);
                    }
                }

                void handle_write_body(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        // Read until the end of entire headers
                        boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF+CRLF,
                            boost::bind(&client::handle_status_line, this, boost::asio::placeholders::error, ctx));
                    }
                    else
                    {
                        ctx->report_error("Failed to write request body", ec);
                    }
                }

                void handle_status_line(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        std::istream response_stream(&ctx->m_response_buf);
                        std::string http_version;
                        response_stream >> http_version;
                        status_code status_code;
                        response_stream >> status_code;

                        std::string status_message;
                        std::getline(response_stream, status_message);

                        ctx->m_response.set_status_code(status_code);

                        trim_whitespace(status_message);
                        ctx->m_response.set_reason_phrase(status_message);

                        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
                        {
                            ctx->report_error("Invalid HTTP status line", ec);
                            return;
                        }

                        read_headers(ctx);
                    }
                    else
                    {
                        ctx->report_error("Failed to read HTTP status line", ec);
                    }
                }

                void read_headers(linux_request_context* ctx)
                {
                    ctx->m_needChunked = false;
                    std::istream response_stream(&ctx->m_response_buf);
                    std::string header;
                    while (std::getline(response_stream, header) && header != "\r")
                    {
                        auto colon = header.find(':');
                        if (colon != std::string::npos)
                        {
                            auto name = header.substr(0, colon);
                            auto value = header.substr(colon+2, header.size()-(colon+3)); // also exclude '\r'
                            ctx->m_response.headers()[name] = value;

                            if (boost::iequals(name, header_names::transfer_encoding))
                            {
                                ctx->m_needChunked = boost::iequals(value, U("chunked"));
                            }

                        }
                    }
                    ctx->complete_headers();

                    ctx->m_known_size = 0;
                    ctx->m_response.headers().match(header_names::content_length, ctx->m_known_size);
                    // note: need to check for 'chunked' here as well, azure storage sends both
                    // transfer-encoding:chunked and content-length:0 (although HTTP says not to)
                    if (ctx->m_request.method() == U("HEAD") || (!ctx->m_needChunked && ctx->m_known_size == 0))
                    {
                        // we can stop early - no body
                        ctx->complete_request(0);
                    }
                    else
                    {
                        ctx->m_current_size = 0;
                        if (!ctx->m_needChunked)
                            async_read_until_buffersize(std::min(ctx->m_known_size, m_chunksize),
                            boost::bind(&client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
                        else
                            boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF,
                            boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                    }
                }

                template <typename ReadHandler>
                void async_read_until_buffersize(size_t size, ReadHandler handler, linux_request_context* ctx)
                {
                    if (ctx->m_response_buf.size() >= size)
                        boost::asio::async_read(*ctx->m_socket, ctx->m_response_buf, boost::asio::transfer_at_least(0), handler);
                    else
                        boost::asio::async_read(*ctx->m_socket, ctx->m_response_buf, boost::asio::transfer_at_least(size - ctx->m_response_buf.size()), handler);
                }

                void handle_chunk_header(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        std::istream response_stream(&ctx->m_response_buf);
                        std::string line;
                        std::getline(response_stream, line);

                        std::istringstream octetLine(line);    
                        int octets = 0;
                        octetLine >> std::hex >> octets;

                        if (octetLine.fail())
                        {
                            ctx->report_error("Invalid chunked response header", boost::system::error_code());
                        }
                        else
                            async_read_until_buffersize(octets + CRLF.size(), // +2 for crlf
                            boost::bind(&client::handle_chunk, this, boost::asio::placeholders::error, octets, ctx), ctx);
                    }
                    else
                    {
                        ctx->report_error("Retrieving message chunk header", ec);
                    }
                }

                void handle_chunk(const boost::system::error_code& ec, int to_read, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        ctx->m_current_size += to_read;
                        if (to_read == 0)
                        {
                            ctx->m_response_buf.consume(CRLF.size());
                            ctx->_get_writebuffer().close(std::ios_base::out).get();
                            ctx->complete_request(ctx->m_current_size);
                        }
                        else
                        {
                            auto writeBuffer = ctx->_get_writebuffer();
                            writeBuffer.putn(boost::asio::buffer_cast<const uint8_t *>(ctx->m_response_buf.data()), to_read).then([=](size_t) {
                                ctx->m_response_buf.consume(to_read + CRLF.size()); // consume crlf
                                boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF,
                                    boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                            });
                        }
                    }
                    else
                    {
                        ctx->report_error("Failed to read chunked response part", ec);
                    }
                }

                void handle_read_content(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    auto writeBuffer = ctx->_get_writebuffer();

                    if (ec)
                        ctx->report_error("Failed to read response body", ec);
                    else if (ctx->m_current_size < ctx->m_known_size)
                    {
                        // more data need to be read
                        writeBuffer.putn(boost::asio::buffer_cast<const uint8_t *>(ctx->m_response_buf.data()), std::min(ctx->m_response_buf.size(), ctx->m_known_size - ctx->m_current_size)).then([=](size_t writtenSize) {
                            ctx->m_current_size += writtenSize;
                            ctx->m_response_buf.consume(writtenSize);
                            async_read_until_buffersize(std::min(m_chunksize, ctx->m_known_size - ctx->m_current_size),
                                boost::bind(&client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
                        });
                    }
                    else
                    {
                        writeBuffer.close(std::ios_base::out).get();
                        ctx->complete_request(ctx->m_current_size);
                    }
                }
            };
            class linux_client : public _http_client_communicator
            {
            private:
                std::unique_ptr<client> m_client;
                http::uri m_address;

            public:
                linux_client(const http::uri &address, const http_client_config& client_config) 
                    : _http_client_communicator(address, client_config)
                    , m_address(address) {}

                unsigned long open()
                {
                    m_client.reset(new client(crossplat::threadpool::shared_instance().service(), client_config().chunksize()));
                    return 0;
                }

                void send_request(request_context* request_ctx)
                {
                    auto linux_ctx = static_cast<linux_request_context*>(request_ctx);

                    auto encoded_resource = uri_builder(m_address).append(linux_ctx->m_request.relative_uri()).to_uri();

                    linux_ctx->m_what = encoded_resource;

                    auto& config = client_config();

                    auto timeout = config.timeout();
                    int secs = static_cast<int>(timeout.count());

                    m_client->send_request(linux_ctx, secs * 1000);
                }
            };
#endif

        } // namespace details

        // Helper function to check to make sure the uri is valid.
        static void verify_uri(const uri &uri)
        {
            // Somethings like proper URI schema are verified by the URI class.
            // We only need to check certain things specific to HTTP.
            if( uri.scheme() != U("http") && uri.scheme() != U("https") )
            {
                throw std::invalid_argument("URI scheme must be 'http' or 'https'");
            }

            if(uri.host().empty())
            {
                throw std::invalid_argument("URI must contain a hostname.");
            }
        }

        class http_network_handler : public http_pipeline_stage
        {
        public:

            http_network_handler(const uri &base_uri, const http_client_config& client_config) :
#if defined(__cplusplus_winrt)
                m_http_client_impl(std::make_shared<details::winrt_client>(base_uri, client_config))
#elif defined(WIN32)
                m_http_client_impl(std::make_shared<details::winhttp_client>(base_uri, client_config))
#else // LINUX
                m_http_client_impl(std::make_shared<details::linux_client>(base_uri, client_config))
#endif
            {
            }

            virtual pplx::task<http_response> propagate(http_request request)
            {

#if defined(__cplusplus_winrt)
                details::request_context * context = details::winrt_request_context::create_request_context(m_http_client_impl, request);
#elif defined(WIN32)
                details::request_context * context = details::winhttp_request_context::create_request_context(m_http_client_impl, request);
#else // LINUX
                details::request_context * context = details::linux_request_context::create_request_context(m_http_client_impl, request);
#endif

                // Use a task to externally signal the final result and completion of the task.
                auto result_task = pplx::create_task(context->m_request_completion);

                // Asynchronously send the response with the HTTP client implementation.
                m_http_client_impl->async_send_request(context);

                return result_task;
            }

            const std::shared_ptr<details::_http_client_communicator>& http_client_impl() const
            {
                return m_http_client_impl;
            }

        private:
            std::shared_ptr<details::_http_client_communicator> m_http_client_impl;
        };

        http_client::http_client(const uri &base_uri)
        {
            build_pipeline(base_uri, http_client_config());
        }

        http_client::http_client(const uri &base_uri, const http_client_config& client_config)
        {
            build_pipeline(base_uri, client_config);
        }

        void http_client::build_pipeline(const uri &base_uri, const http_client_config& client_config)
        {
            verify_uri(base_uri);
            m_pipeline = ::web::http::http_pipeline::create_pipeline(std::make_shared<http_network_handler>(base_uri, client_config));
        }

        pplx::task<http_response> http_client::request(http_request request)
        {
            return m_pipeline->propagate(request);
        }

        const http_client_config& http_client::client_config() const
        {
            http_network_handler* ph = static_cast<http_network_handler*>(m_pipeline->last_stage().get());
            return ph->http_client_impl()->client_config();
        }

    } // namespace client
}} // namespace casablanca::http
