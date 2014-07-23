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

namespace web 
{
namespace http
{
namespace client
{
namespace details
{

// Additional information necessary to track a WinRT request.
class winrt_request_context : public request_context
{
public:

    // Factory function to create requests on the heap.
    static std::shared_ptr<request_context> create_request_context(std::shared_ptr<_http_client_communicator> client, http_request &request)
    {
        return std::make_shared<winrt_request_context>(client, request);
    }

    Microsoft::WRL::ComPtr<IXMLHTTPRequest2> m_hRequest;

    // Request contexts must be created through factory function.
    // But constructor needs to be public for make_shared to access.
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
    HttpRequestCallback(std::shared_ptr<winrt_request_context> request)
        : m_request(request)
    {
    }

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
        if(hr != S_OK)
        {
            return hr;
        }

        auto progress = m_request->m_request._get_impl()->_progress_handler();
        if ( progress && m_request->m_uploaded == 0)
        {
            try { (*progress)(message_direction::upload, 0); } catch(...)
            {
                m_request->m_exceptionPtr = std::current_exception();
                return ERROR_UNHANDLED_EXCEPTION;
            }
        }

        parse_headers_string(hdrStr, response.headers());
        m_request->complete_headers();

        return S_OK;
    }

    // Called when a portion of the entity body has been received.
    HRESULT STDMETHODCALLTYPE OnDataAvailable(_In_opt_ IXMLHTTPRequest2*, _In_opt_ ISequentialStream* ) {
        return S_OK;
    }

    // Called when the entire entity response has been received.
    HRESULT STDMETHODCALLTYPE OnResponseReceived(_In_opt_ IXMLHTTPRequest2*, _In_opt_ ISequentialStream* )
    {
        auto progress = m_request->m_request._get_impl()->_progress_handler();
        if ( progress && m_request->m_downloaded == 0)
        {
            try { (*progress)(message_direction::download, 0); } catch(...)
            {
                m_request->m_exceptionPtr = std::current_exception();
            }
        }

        if (m_request->m_exceptionPtr != nullptr)
            m_request->report_exception(m_request->m_exceptionPtr);
        else    
            m_request->complete_request(m_request->m_downloaded);

        // Break the circular reference loop.
        //     - winrt_request_context holds a reference to IXmlHttpRequest2
        //     - IXmlHttpRequest2 holds a reference to HttpRequestCallback
        //     - HttpRequestCallback holds a reference to winrt_request_context
        // 
        // Not releasing the winrt_request_context below previously worked due to the 
        // implementation of IXmlHttpRequest2, after calling OnError/OnResponseReceived 
        // it would immediately release its reference to HttpRequestCallback. However
        // it since has been discovered on Xbox that the implementation is different, 
        // the reference to HttpRequestCallback is NOT immediately released and is only
        // done at destruction of IXmlHttpRequest2.
        // 
        // To be safe we now will break the circular reference.
        m_request.reset();

        return S_OK;
    }

    // Called when an error occurs during the HTTP request.
    HRESULT STDMETHODCALLTYPE OnError(_In_opt_ IXMLHTTPRequest2*, HRESULT hrError)
    {
        if (m_request->m_exceptionPtr != nullptr)
            m_request->report_exception(m_request->m_exceptionPtr);
        else    
            m_request->report_error(hrError, L"Error in IXMLHttpRequest2Callback");
        
        // Break the circular reference loop.
        // See full explaination in OnResponseReceived
        m_request.reset();
        
        return S_OK;
    }

private:
    std::shared_ptr<winrt_request_context> m_request;
};

/// <summary>
/// This class acts as a bridge for the underlying request stream.
/// </summary>
/// <remarks>
/// These operations are completely synchronous, so it's important to block on both
/// read and write operations. The I/O will be done off the UI thread, so there is no risk
/// of causing the UI to become unresponsive.
/// </remarks>
class IRequestStream
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<ClassicCom>, ISequentialStream>
{
public:
    IRequestStream(std::weak_ptr<winrt_request_context> context, size_t read_length = std::numeric_limits<size_t>::max())
        : m_context(context),
        m_read_length(read_length)
    {
        // read_length is the initial length of the ISequentialStream that is avaiable for read
        // This is required because IXHR2 attempts to read more data that what is specified by
        // the content_length. (Specifically, it appears to be reading 128K chunks regardless of
        // the content_length specified).
    }

    virtual HRESULT STDMETHODCALLTYPE Read(_Out_writes_ (cb) void *pv, _In_ ULONG cb, _Out_ ULONG *pcbRead)
    {
        auto context = m_context.lock();
        if(context == nullptr)
        {
            // OnError has already been called so just error out
            return STG_E_READFAULT;
        }

        try
        {
            auto buffer = context->_get_readbuffer();
            
            // Do not read more than the specified read_length
            SafeSize safe_count = static_cast<size_t>(cb);
            size_t size_to_read = safe_count.Min(m_read_length);

            const size_t count = buffer.getn((uint8_t *)pv, size_to_read).get();
            *pcbRead = (ULONG)count;
            if (count == 0 && size_to_read != 0)
            {
                return STG_E_READFAULT;
            }

            _ASSERTE(count != static_cast<size_t>(-1));
            _ASSERTE(m_read_length >= count);
            m_read_length -= count;

            auto progress = context->m_request._get_impl()->_progress_handler();
            if ( progress && count > 0 )
            {
                context->m_uploaded += count;
                try { (*progress)(message_direction::upload, context->m_uploaded); } catch(...)
                {
                    context->m_exceptionPtr = std::current_exception();
                    return STG_E_READFAULT;
                }
            }

            return S_OK;
        }
        catch(...)
        {
            context->m_exceptionPtr = std::current_exception();
            return STG_E_READFAULT;
        }
    }

    virtual HRESULT STDMETHODCALLTYPE Write(const void *, _In_ ULONG, _Out_opt_ ULONG *)
    {
        return E_NOTIMPL;
    }

private:

    // The request context controls the lifetime of this class so we only hold a weak_ptr.
    std::weak_ptr<winrt_request_context> m_context;

    // Length of the ISequentialStream for reads. This is equivalent
    // to the amount of data that the ISequentialStream is allowed
    // to read from the underlying streambuffer.
    size_t m_read_length;
};

/// <summary>
/// This class acts as a bridge for the underlying response stream.
/// </summary>
/// <remarks>
/// These operations are completely synchronous, so it's important to block on both
/// read and write operations. The I/O will be done off the UI thread, so there is no risk
/// of causing the UI to become unresponsive.
/// </remarks>
class IResponseStream
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<ClassicCom>, ISequentialStream>
{
public:
    IResponseStream(std::weak_ptr<request_context> context)
        : m_context(context)
    { }

    virtual HRESULT STDMETHODCALLTYPE Write(_In_reads_bytes_(cb) const void *pv, _In_ ULONG cb, _Out_opt_ ULONG *pcbWritten)
    {
        auto context = m_context.lock();
        if(context == nullptr)
        {
            // OnError has already been called so just error out
            return STG_E_CANTSAVE;
        }

        if (pcbWritten != nullptr)
        {
            *pcbWritten = 0;
        }

        try
        {
            auto buffer = context->_get_writebuffer();
            if ( cb == 0 )
            {
                buffer.sync().wait();
                return S_OK;
            }

            const size_t count = buffer.putn((const uint8_t *)pv, (size_t)cb).get();

            _ASSERTE(count != static_cast<size_t>(-1));
            _ASSERTE(count <= static_cast<size_t>(ULONG_MAX));
            if (pcbWritten != nullptr)
            {
                *pcbWritten = (ULONG) count;
            }
            context->m_downloaded += count;

            auto progress = context->m_request._get_impl()->_progress_handler();
            if ( progress && count > 0 )
            {
                try { (*progress)(message_direction::download, context->m_downloaded); } catch(...)
                {
                    context->m_exceptionPtr = std::current_exception();
                    return STG_E_CANTSAVE;
                }
            }

            return S_OK;
        }
        catch(...)
        {
            context->m_exceptionPtr = std::current_exception();
            return STG_E_CANTSAVE;
        }
    }

    virtual HRESULT STDMETHODCALLTYPE Read(void *, _In_ ULONG, _Out_ ULONG *)
    {
        return E_NOTIMPL;
    }

private:
    
    // The request context controls the lifetime of this class so we only hold a weak_ptr.
    std::weak_ptr<request_context> m_context;
};

// WinRT client.
class winrt_client : public _http_client_communicator
{
public:
    winrt_client(http::uri address, http_client_config client_config)
        : _http_client_communicator(std::move(address), std::move(client_config)) { }

protected:

    // Method to open client.
    unsigned long open()
    {
        return 0;
    }

    // Start sending request.
    void send_request(_In_ std::shared_ptr<request_context> request)
    {
        http_request &msg = request->m_request;
        auto winrt_context = std::static_pointer_cast<winrt_request_context>(request);

        if (!validate_method(msg.method()))
        {
            request->report_exception(http_exception(L"The method string is invalid."));
            return;
        }

        if ( msg.method() == http::methods::TRCE )
        {
            // Not supported by WinInet. Generate a more specific exception than what WinInet does.
            request->report_exception(http_exception(L"TRACE is not supported"));
            return;
        }

        const size_t content_length = msg._get_impl()->_get_content_length();
        if (content_length == std::numeric_limits<size_t>::max())
        {
            // IXHR2 does not allow transfer encoding chunked. So the user is expected to set the content length
            request->report_exception(http_exception(L"Content length is not specified in the http headers"));
            return;
        }

        // Start sending HTTP request.
        HRESULT hr = CoCreateInstance(
            __uuidof(FreeThreadedXMLHTTP60), 
            nullptr, 
            CLSCTX_INPROC, 
            __uuidof(IXMLHTTPRequest2), 
            reinterpret_cast<void**>(winrt_context->m_hRequest.GetAddressOf()));
        if ( FAILED(hr) ) 
        {
            request->report_error(hr, L"Failure to create IXMLHTTPRequest2 instance");
            return;
        }

        utility::string_t encoded_resource = http::uri_builder(m_uri).append(msg.relative_uri()).to_string();

        const utility::char_t* usernanme = nullptr;
        const utility::char_t* password = nullptr;
        const utility::char_t* proxy_usernanme = nullptr;
        const utility::char_t* proxy_password = nullptr;

        const auto &config = client_config();
        const auto &client_cred = config.credentials();
        if(client_cred.is_set())
        {
            usernanme = client_cred.username().c_str();
            password  = client_cred.password().c_str();
        }

        const auto &proxy = config.proxy();
        if(!proxy.is_default())
        {
            request->report_exception(http_exception(L"Only a default proxy server is supported"));
            return;
        }

        const auto &proxy_cred = proxy.credentials();
        if(proxy_cred.is_set())
        {
            proxy_usernanme = proxy_cred.username().c_str();
            proxy_password  = proxy_cred.password().c_str();
        }

        hr = winrt_context->m_hRequest->Open(
            msg.method().c_str(), 
            encoded_resource.c_str(), 
            Make<HttpRequestCallback>(winrt_context).Get(), 
            usernanme, 
            password, 
            proxy_usernanme, 
            proxy_password);
        if ( FAILED(hr) ) 
        {
            request->report_error(hr, L"Failure to open HTTP request");
            return;
        }

        // Suppress automatic prompts for user credentials, since they are already provided.
        hr = winrt_context->m_hRequest->SetProperty(XHR_PROP_NO_CRED_PROMPT, TRUE);
        if(FAILED(hr))
        {
            request->report_error(hr, L"Failure to set no credentials prompt property");
            return;
        }

        const auto timeout = config.timeout();
        const int secs = static_cast<int>(timeout.count());
        hr = winrt_context->m_hRequest->SetProperty(XHR_PROP_TIMEOUT, secs * 1000);
        if ( FAILED(hr) ) 
        {
            request->report_error(hr, L"Failure to set HTTP request properties");
            return;
        }

        // Add headers.
        for ( auto hdr = msg.headers().begin(); hdr != msg.headers().end(); ++hdr )
        {
            winrt_context->m_hRequest->SetRequestHeader(hdr->first.c_str(), hdr->second.c_str());
        }

        // Set response stream.
        hr = winrt_context->m_hRequest->SetCustomResponseStream(Make<IResponseStream>(request).Get());
        if ( FAILED(hr) ) 
        {
            request->report_error(hr, L"Failure to set HTTP response stream");
            return;
        }

        // Call the callback function of user customized options
        try
        {
            config.call_user_nativehandle_options(winrt_context->m_hRequest.Get());
        }
        catch (...)
        {
            request->report_exception(std::current_exception());
            return;
        }

        if (content_length == 0)
        {
            hr = winrt_context->m_hRequest->Send(nullptr, 0);
        }
        else
        {
            if ( msg.method() == http::methods::GET || msg.method() == http::methods::HEAD )
            {
                request->report_exception(http_exception(get_with_body));
                return;
            }

            hr = winrt_context->m_hRequest->Send(Make<IRequestStream>(winrt_context, content_length).Get(), content_length);
        }

        if ( FAILED(hr) )
        {
            request->report_error(hr, L"Failure to send HTTP request");
            return;
        }

        // Register for notification on cancellation to abort this request.
        if(msg._cancellation_token() != pplx::cancellation_token::none())
        {
            auto requestHandle = winrt_context->m_hRequest;

            // cancellation callback is unregistered when request is completed.
            winrt_context->m_cancellationRegistration = msg._cancellation_token().register_callback([requestHandle]()
            {
                requestHandle->Abort();
            });
        }
    }

private:

    // No copy or assignment.
    winrt_client(const winrt_client&);
    winrt_client &operator=(const winrt_client&);
};

http_network_handler::http_network_handler(uri base_uri, http_client_config client_config) :
    m_http_client_impl(std::make_shared<details::winrt_client>(std::move(base_uri), std::move(client_config)))
{
}

pplx::task<http_response> http_network_handler::propagate(http_request request)
{
    auto context = details::winrt_request_context::create_request_context(m_http_client_impl, request);

    // Use a task to externally signal the final result and completion of the task.
    auto result_task = pplx::create_task(context->m_request_completion);

    // Asynchronously send the response with the HTTP client implementation.
    m_http_client_impl->async_send_request(context);

    return result_task;
}

}}}} // namespaces
