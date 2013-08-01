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
* http_win8.cpp
*
* HTTP Library: Client-side APIs.
* 
* This file contains the implementation for Windows 8 App store apps
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "cpprest/http_client_impl.h"

namespace web { namespace http
{
    namespace client
    {
        namespace details
        {
            class _http_client_communicator;

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
                    m_bytes_read(0),
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

                                auto progress = m_request->m_request._get_impl()->_progress_handler();
                                if ( progress && count > 0 )
                                {
                                    m_request->m_uploaded += count;
                                    (*progress)(message_direction::upload, m_request->m_uploaded);
                                }
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

                virtual HRESULT STDMETHODCALLTYPE Write( _In_reads_bytes_(cb) const void *pv, _In_ ULONG cb, _Out_opt_ ULONG *pcbWritten )
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

                        auto progress = m_request->m_request._get_impl()->_progress_handler();
                        if ( progress && count > 0 )
                        {
                            m_request->m_downloaded += count;
                            (*progress)(message_direction::download, m_request->m_downloaded);
                        }

                        return S_OK;
                    }
                    catch(...)
                    {
                        m_request->m_exceptionPtr = std::current_exception();
                        return (HRESULT)STG_E_CANTSAVE;
                    }
                }

                size_t total_bytes() const
                {
                    return m_bytes_written;
                }

            private:
                concurrency::streams::streambuf<uint8_t> m_buffer;
                
                request_context *m_request;

                // Total count of bytes read
                size_t m_bytes_read;

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

                ~winrt_request_context()
                {
                    cleanup();
                }

                virtual void cleanup()
                {
                    // Unlike WinHTTP, we can delete the object right away
                    if ( m_hRequest != nullptr )
                        m_hRequest->Release();
                }

            private:

                // Request contexts must be created through factory function.
                winrt_request_context(std::shared_ptr<_http_client_communicator> client, http_request &request) 
                    : request_context(client, request), m_hRequest(nullptr) 
                {
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
                HRESULT STDMETHODCALLTYPE OnRedirect(_In_opt_ IXMLHTTPRequest2*, __RPC__in_string const WCHAR*) {
                    return S_OK;
                }

                // Called when HTTP headers have been received and processed.
                HRESULT STDMETHODCALLTYPE OnHeadersAvailable(_In_ IXMLHTTPRequest2* xmlReq, DWORD dw, __RPC__in_string const WCHAR* phrase)
                {
                    http_response response = m_request->m_response;
                    response.set_status_code((http::status_code)dw);
                    response.set_reason_phrase(phrase);

                    utf16char *hdrStr = nullptr;
                    HRESULT hr = xmlReq->GetAllResponseHeaders(&hdrStr);

                    if ( hr == S_OK )
                    {
                        auto progress = m_request->m_request._get_impl()->_progress_handler();
                        if ( progress && m_request->m_uploaded == 0)
                        {
                            (*progress)(message_direction::upload, 0);
                        }

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
                        if ( req != nullptr ) req->AddRef();

                        auto progress = m_request->m_request._get_impl()->_progress_handler();
                        if ( progress && m_request->m_downloaded == 0)
                        {
                            (*progress)(message_direction::download, 0);
                        }

                        if (m_request->m_exceptionPtr != nullptr)
                            m_request->report_exception(m_request->m_exceptionPtr);
                        else    
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
                        if ( req != nullptr ) req->AddRef();
                            
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

            private:
                static bool _check_streambuf(winrt_request_context * winhttp_context, concurrency::streams::streambuf<uint8_t> rdbuf, const utility::char_t* msg) 
                {
                    if ( !rdbuf.is_open() )
                    {
                        auto eptr = rdbuf.exception();
                        if ( eptr )
                            winhttp_context->report_exception(eptr);
                        else
                            winhttp_context->report_error(msg);
                    }
                    return rdbuf.is_open();
                }

            protected:

                // Method to open client.
                unsigned long open()
                {
                    return 0;
                }

                // Start sending request.
                void send_request(_In_ request_context *request)
                {
                    http_request &msg = request->m_request;
                    winrt_request_context * winrt_context = static_cast<winrt_request_context *>(request);

                    if (!validate_method(msg.method()))
                    {
                        request->report_error(L"The method string is invalid.");
                        return;
                    }
                    
                    if ( msg.method() == http::methods::TRCE )
                    {
                        // Not supported by WinInet. Generate a more specific exception than what WinInet does.
                        request->report_error(L"TRACE is not supported");
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

                    xhr->AddRef();
                    winrt_context->m_hRequest = xhr;

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
                        request->report_error(L"Only a default proxy server is supported");
                        xhr->Release();
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
                        xhr->Release();
                        return;
                    }

                    // Suppress automatic prompts for user credentials, since they are already provided.
                    hr = xhr->SetProperty(XHR_PROP_NO_CRED_PROMPT, TRUE);
                    if(!SUCCEEDED(hr))
                    {
                        request->report_error(hr, L"Failure to set no credentials prompt property");
                        xhr->Release();
                        return;
                    }

                    auto timeout = config.timeout();
                    int secs = static_cast<int>(timeout.count());
                    hr = xhr->SetProperty(XHR_PROP_TIMEOUT, secs * 1000);

                    if ( !SUCCEEDED(hr) ) 
                    {
                        request->report_error(hr, L"Failure to set HTTP request properties");
                        xhr->Release();
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
                    if ( !_check_streambuf(winrt_context, writebuf, L"Output stream is not open") )
                    {
                        xhr->Release();
                        return;
                    }

                    winrt_context->m_stream_bridge = Make<ISequentialStream_bridge>(writebuf, request);

                    hr = xhr->SetCustomResponseStream(winrt_context->m_stream_bridge.Get());

                    if ( !SUCCEEDED(hr) ) 
                    {
                        request->report_error(hr, L"Failure to set HTTP response stream");
                        xhr->Release();
                        return;
                    }

                    size_t content_length = msg._get_impl()->_get_content_length();

                    if (content_length == std::numeric_limits<size_t>::max())
                    {
                        // IXHR2 does not allow transfer encoding chunked. So the user is expected to set the content length
                        request->report_error(L"Content length is not specified in the http headers");
                        xhr->Release();
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
                            xhr->Release();
                            return;
                        }

                        auto readbuf = winrt_context->_get_readbuffer();
                        if ( !_check_streambuf(winrt_context, readbuf, L"Input stream is not open") )
                        {
                            xhr->Release();
                            return;
                        }
                        auto str = Make<ISequentialStream_bridge>(readbuf, request, content_length);

                        hr = xhr->Send(str.Get(), content_length);
                    }

                    if ( !SUCCEEDED(hr) )
                    {
                        request->report_error(hr, L"Failure to send HTTP request");
                        xhr->Release();
                        return;
                    }

                    xhr->Release();
                }

            private:

                // No copy or assignment.
                winrt_client(const winrt_client&);
                winrt_client &operator=(const winrt_client&);
            };


        } // namespace details

        // Helper function to check to make sure the uri is valid.
        static void verify_uri(const uri &uri)
        {
            // Somethings like proper URI schema are verified by the URI class.
            // We only need to check certain things specific to HTTP.
            if( uri.scheme() != L"http" && uri.scheme() != L"https" )
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
                m_http_client_impl(std::make_shared<details::winrt_client>(base_uri, client_config))
            {
            }

            virtual pplx::task<http_response> propagate(http_request request)
            {
                details::request_context * context = details::winrt_request_context::create_request_context(m_http_client_impl, request);

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
            :_base_uri(base_uri)
        {
            build_pipeline(base_uri, http_client_config());
        }

        http_client::http_client(const uri &base_uri, const http_client_config& client_config)
            :_base_uri(base_uri)
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
