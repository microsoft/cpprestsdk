/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: HTTP listener (server-side) APIs
*
* This file contains implementation built on Windows HTTP Server APIs.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if _WIN32_WINNT >= _WIN32_WINNT_VISTA

#pragma comment(lib, "Ws2_32")

#include "http_server_named_pipe.h"
#include "http_server_impl.h"
#include "../common/internal_http_helpers.h"
#include <boost/algorithm/string/predicate.hpp>
#include <sddl.h>

#undef min
#undef max

using namespace web;
using namespace utility;
using namespace concurrency;
using namespace utility::conversions;
using namespace http::details;
using namespace http::experimental::listener;
using namespace http::experimental::details;

#define CHUNK_SIZE 64 * 1024
#define CRLF std::string("\r\n")
#define CRLFCRLF std::string("\r\n\r\n")

// Maximum number of concurrent named pipe instances (1 instance per request).
const DWORD MAX_NUMBER_OF_CONCURRENT_REQUESTS = 128;

namespace web
{
namespace http
{
namespace experimental
{
namespace details
{

/***************************************************************************************************

                                named_pipe_request_context

***************************************************************************************************/

named_pipe_request_context::named_pipe_request_context(web::http::experimental::listener::details::http_listener_impl* listener, HANDLE pipeHandle, TP_IO* threadpool_io)
    : 
    m_listener(listener),
    m_pipeHandle(pipeHandle),
    m_threadpool_io(threadpool_io)
{
    auto *pServer = static_cast<named_pipe_server *>(http_server_api::server_api());
    if (++pServer->m_numOutstandingRequests == 1)
    {
        pServer->m_zeroOutstandingRequests.reset();
    }
}

named_pipe_request_context::~named_pipe_request_context()
{
    // Unfortunately have to work around a ppl task_completion_event bug that can cause AVs.
    // Bug is that task_completion_event accesses internal state after setting.
    // Workaround is to use a lock incurring additional synchronization, if can acquire
    // the lock then setting of the event has completed.
    std::unique_lock<std::mutex> lock(m_responseCompletedLock);

    // Add a task-based continuation so no exceptions thrown from the task go 'unobserved'.
    pplx::create_task(m_response_completed).then([](pplx::task<void> t)
    {
        try { t.wait(); }
        catch (...) {}
    });

    auto *pServer = static_cast<named_pipe_server *>(http_server_api::server_api());
    if (--pServer->m_numOutstandingRequests == 0)
    {
        pServer->m_zeroOutstandingRequests.set();
    }

    if (m_threadpool_io != nullptr) {
        CloseThreadpoolIo(m_threadpool_io);
    }

    if (m_pipeHandle != nullptr)
    {
        // TODO: Need to FlushFileBuffers before disconnecting (https://msdn.microsoft.com/en-us/library/windows/desktop/aa365598(v=vs.85).aspx)
        DisconnectNamedPipe(m_pipeHandle);
        CloseHandle(m_pipeHandle);
    }
}

void named_pipe_request_context::async_process_request(http_request msg)
{
    m_msg = msg;

    m_request_buffer = std::unique_ptr<unsigned char[]>(new unsigned char[msl::safeint3::SafeInt<unsigned long>(named_pipe_listener::PIPE_BUFFER_SIZE)]); 

    m_overlapped.set_http_io_completion([this](DWORD error, DWORD bytes_read) {
        on_request_received(error, bytes_read);
    });

    StartThreadpoolIo(m_threadpool_io);

    DWORD bytes_read = 0;

    auto success = ReadFile(m_pipeHandle, m_request_buffer.get(), named_pipe_listener::PIPE_BUFFER_SIZE, &bytes_read, &m_overlapped);

    if (!success && GetLastError() != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(m_threadpool_io);
        m_msg.reply(status_codes::InternalError);
        init_response_callbacks(ShouldWaitForBody::DontWait);
    }
}

/// <summary>
///  The read request completion callback function.
/// </summary>
void named_pipe_request_context::on_request_received(DWORD error_code, DWORD bytes_read)
{
    if (error_code != NO_ERROR) {
        m_msg.reply(status_codes::InternalError);
        init_response_callbacks(ShouldWaitForBody::DontWait);
        return;
    }

    // TODO: Need better buffer management
    std::string buffer(reinterpret_cast<char*>(m_request_buffer.get()), bytes_read);
    std::istringstream request_stream(buffer);
    request_stream.imbue(std::locale::classic());
    std::skipws(request_stream);

    // parse the request
    // TODO: Support Connection header?
    // TODO: Support Transfer-Encoding: chunked
    try {
        m_msg.set_method(parse_request_method(&request_stream));

        std::string path;
        web::http::http_version version;
        parse_request_path_and_version(&request_stream, &path, &version);

        m_msg.set_request_uri(utility::conversions::to_string_t(path));
        m_msg._get_impl()->_set_http_version(version);

        m_msg._get_impl()->_set_remote_address(U("127.0.0.1"));

        parse_http_headers(&request_stream, m_msg.headers());
    }
    catch (const std::exception& e) {
        m_msg.reply(status_codes::BadRequest, e.what());
        init_response_callbacks(ShouldWaitForBody::DontWait);
        return;
    }

    // read the body 
    m_msg._get_impl()->_prepare_to_receive_data();

    auto bodySize = request_stream.rdbuf()->in_avail();
    auto request_body_buf = m_msg._get_impl()->outstream().streambuf();
    auto body = request_body_buf.alloc(bodySize);

    std::string tmp;
    request_stream >> tmp;
    memcpy(body, tmp.c_str(), bodySize);

    request_body_buf.commit(bodySize);

    m_msg._get_impl()->_complete(bodySize);

    dispatch_request_to_listener(m_listener);
}

http::method named_pipe_request_context::parse_request_method(std::istream* request_stream)
{
    web::http::method method;

#ifndef _UTF16_STRINGS
    request_stream >> method;
#else
    {
        std::string tmp;
        (*request_stream) >> tmp;
        method = utility::conversions::latin1_to_utf16(tmp);
    }
#endif

    if (boost::iequals(method, methods::GET))          method = methods::GET;
    else if (boost::iequals(method, methods::POST))    method = methods::POST;
    else if (boost::iequals(method, methods::PUT))     method = methods::PUT;
    else if (boost::iequals(method, methods::DEL))     method = methods::DEL;
    else if (boost::iequals(method, methods::HEAD))    method = methods::HEAD;
    else if (boost::iequals(method, methods::TRCE))    method = methods::TRCE;
    else if (boost::iequals(method, methods::CONNECT)) method = methods::CONNECT;
    else if (boost::iequals(method, methods::OPTIONS)) method = methods::OPTIONS;

    return method;
}

void named_pipe_request_context::parse_request_path_and_version(std::istream* request_stream, std::string* path, web::http::http_version* version)
{
    std::string http_path_and_version;
    std::getline(*request_stream, http_path_and_version);
    const size_t VersionPortionSize = sizeof(" HTTP/1.1\r") - 1;

    // Make sure path and version is long enough to contain the HTTP version
    if (http_path_and_version.size() < VersionPortionSize + 2) {
        throw http_exception(status_codes::BadRequest);
    }

    // Get the path - remove the version portion and prefix space
    *path = http_path_and_version.substr(1, http_path_and_version.size() - VersionPortionSize - 1);

    // Get the version
    std::string versionString = http_path_and_version.substr(http_path_and_version.size() - VersionPortionSize + 1, VersionPortionSize - 2);
    *version = web::http::http_version::from_string(versionString);
}

void named_pipe_request_context::parse_http_headers(std::istream* request_stream, http::http_headers& headers)
{
    std::string oneHeader;

    while (std::getline(*request_stream, oneHeader) && oneHeader != "\r")
    {
        if (oneHeader.length() == 0 || oneHeader[0] == '\0') // TODO: Check extra '\0' from client
            continue;

        auto colon = oneHeader.find(':');
        if (colon != std::string::npos && colon != 0)
        {
            auto name = utility::conversions::to_string_t(oneHeader.substr(0, colon));
            auto value = utility::conversions::to_string_t(oneHeader.substr(colon + 1, oneHeader.length() - (colon + 1))); // also exclude '\r'
            web::http::details::trim_whitespace(name);
            web::http::details::trim_whitespace(value);

            if (boost::iequals(name, header_names::content_length))
            {
                headers[header_names::content_length] = value;
            }
            else
            {
                headers.add(name, value);
            }
        }
        else
        {
            throw http_exception(status_codes::BadRequest);
        }
    }
}

void named_pipe_request_context::dispatch_request_to_listener(_In_ web::http::experimental::listener::details::http_listener_impl* pListener)
{
    m_msg._set_listener_path(pListener->uri().path());

    // Save http_request copy to dispatch to user's handler in case content_ready() completes before.
    http_request request = m_msg;

    init_response_callbacks(ShouldWaitForBody::Wait);

    // Look up the lock for the http_listener.
    pplx::extensibility::reader_writer_lock_t *pListenerLock;

    {
        auto *pServer = static_cast<named_pipe_server*>(http_server_api::server_api());

        pplx::extensibility::scoped_read_lock_t lock(pServer->m_listenersLock);

        // It is possible the listener could have unregistered.
        if (pServer->m_registeredListeners.find(pListener) == pServer->m_registeredListeners.end())
        {
            request.reply(status_codes::NotFound);
            return;
        }
        pListenerLock = &pServer->m_registeredListeners[pListener]->m_requestHandlerLock;

        // We need to acquire the listener's lock before releasing the registered listeners lock.
        // But we don't need to hold the registered listeners lock when calling into the user's code.
        pListenerLock->lock_read();
    }

    try
    {
        pListener->handle_request(request);
        pListenerLock->unlock();
    }
    catch (...)
    {
        pListenerLock->unlock();
        request._reply_if_not_already(status_codes::InternalError);
    }
}


void named_pipe_request_context::init_response_callbacks(ShouldWaitForBody shouldWait)
{
    // Use a proxy event so we're not causing a circular reference between the http_request and the response task
    pplx::task_completion_event<void> proxy_content_ready;

    auto content_ready_task = m_msg.content_ready();
    auto get_response_task = m_msg.get_response();

    content_ready_task.then([this, proxy_content_ready](pplx::task<http_request> requestBody)
    {
        // If an exception occurred while processing the body then there is no reason
        // to even try sending the response, just re-surface the same exception.
        try
        {
            requestBody.wait();
        }
        catch (...)
        {
            // Copy the request reference in case it's the last
            http_request request = m_msg;
            m_msg = http_request();
            auto exc = std::current_exception();
            proxy_content_ready.set_exception(exc);
            cancel_request(exc);
            return;
        }

        // At this point the user entirely controls the lifetime of the http_request.
        m_msg = http_request();
        proxy_content_ready.set();
    });

    get_response_task.then([this, proxy_content_ready](pplx::task<http::http_response> responseTask)
    {
        // Don't let an exception from sending the response bring down the server.
        try
        {
            m_response = responseTask.get();
        }
        catch (const pplx::task_canceled &)
        {
            // This means the user didn't respond to the request, allowing the
            // http_request instance to be destroyed. There is nothing to do then
            // so don't send a response.
            // Avoid unobserved exception handler
            pplx::create_task(proxy_content_ready).then([](pplx::task<void> t)
            {
                try { t.wait(); }
                catch (...) {}
            });
            return;
        }
        catch (...)
        {
            // Should never get here, if we do there's a chance that a circular reference will cause leaks,
            // or worse, undefined behaviour as we don't know who owns 'this' anymore 
            _ASSERTE(false);
            m_response = http::http_response(status_codes::InternalError);
        }

        pplx::create_task(m_response_completed).then([this](pplx::task<void> t)
        {
            // After response is sent, break circular reference between http_response and the request context.
            // Otherwise http_listener::close() can hang.
            m_response._get_impl()->_set_server_context(nullptr);
        });

        // Wait until the content download finished before replying because m_overlapped is reused,
        // and we don't want to delete 'this' if the body is still downloading
        pplx::create_task(proxy_content_ready).then([this](pplx::task<void> t)
        {
            try
            {
                t.wait();
                async_process_response();
            }
            catch (...)
            {
            }
        }).wait();
    });

    if (shouldWait == ShouldWaitForBody::DontWait)
    {
        // Fake a body completion so the content_ready() task doesn't keep the http_request alive forever
        m_msg._get_impl()->_complete(0);
    }
}

void named_pipe_request_context::async_process_response()
{
    std::ostringstream os;
    os.imbue(std::locale::classic());

    os << "HTTP/1.1 " << m_response.status_code() << " "
        << utility::conversions::to_utf8string(m_response.reason_phrase())
        << CRLF;

    if (!m_response.body())
    {
        m_response.headers().add(header_names::content_length, 0);
    }

    for (const auto &header : m_response.headers())
    {
        os << utility::conversions::to_utf8string(header.first) << ": " << utility::conversions::to_utf8string(header.second) << CRLF;
    }
    os << CRLF;

    if (m_response.body()) {
        std::vector<unsigned char> body_data;

        body_data.resize(named_pipe_listener::PIPE_BUFFER_SIZE);

        streams::rawptr_buffer<unsigned char> buf(&body_data[0], named_pipe_listener::PIPE_BUFFER_SIZE);

        auto bytes_read = m_response.body().read(buf, named_pipe_listener::PIPE_BUFFER_SIZE).then([this](pplx::task<size_t> op) -> size_t
        {
            size_t bytes_read = 0;

            // If an exception occurs surface the error to user on the server side
            // and cancel the request so the client sees the error.
            try {
                bytes_read = op.get();
            }
            catch (...)
            {
                cancel_request(std::current_exception());
            }
            if (bytes_read == 0)
            {
                cancel_request(std::make_exception_ptr(http_exception(_XPLATSTR("Error unexpectedly encountered the end of the response stream early"))));
            }
            return bytes_read;
        }).get();

        for (size_t i = 0; i < bytes_read; i++)
            os << static_cast<char>(body_data[i]);
    }

    m_overlapped.set_http_io_completion([this](DWORD error, DWORD nBytes) {
        on_response_written(error, nBytes);
    });

    StartThreadpoolIo(m_threadpool_io);

    DWORD bytesWritten = 0;

    auto success = WriteFile(m_pipeHandle, os.str().c_str(), static_cast<DWORD>(os.str().length()), &bytesWritten, &m_overlapped);

    if (!success && GetLastError() != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(m_threadpool_io);
        m_msg.reply(status_codes::InternalError);
        init_response_callbacks(ShouldWaitForBody::DontWait);
    }
}

/// <summary>
///  The send response body completion callback function.
/// </summary>
void named_pipe_request_context::on_response_written(DWORD error_code, DWORD)
{
    if (error_code != NO_ERROR)
    {
        cancel_request(std::make_exception_ptr(http_exception(error_code)));
    }
    else
    {
        std::unique_lock<std::mutex> lock(m_responseCompletedLock);
        m_response_completed.set();
    }
}

void named_pipe_request_context::cancel_request(std::exception_ptr except_ptr)
{
    // TODO 
}

/// <summary>
///  The cancel request completion callback function.
/// </summary>
void named_pipe_request_context::on_request_canceled(DWORD, DWORD)
{
    std::unique_lock<std::mutex> lock(m_responseCompletedLock);
    m_response_completed.set_exception(m_except_ptr);
}


/***************************************************************************************************

                                named_pipe_listener

***************************************************************************************************/

named_pipe_listener::named_pipe_listener(web::http::experimental::listener::details::http_listener_impl* listener)
    : m_listener(listener)
{}

void named_pipe_listener::start() {
    async_receive_request();
}

void named_pipe_listener::async_receive_request()
{
    // TODO: validate uri
    auto path = uri::split_path(m_listener->uri().path());

    utility::string_t pipe_name = U("\\\\.\\pipe\\") + path[0];

    //
    // We use the following ACL for the named pipe:
    //
    auto acl = L"D:(A;;GA;;;BA)(D;;GA;;;NU)";
    //
    // The format of the string is described in https://msdn.microsoft.com/en-us/library/windows/desktop/aa379570(v=vs.85).aspx
    //
    //   D - Discretionary access control list
    //   (A;;GA;;;BA)
    //       A  - Type: SDDL_ACCESS_ALLOWED
    //       GA - Rights: SDDL_GENERIC_ALL
    //       BA - Account SID: SDDL_BUILTIN_ADMINISTRATORS
    //
    //  (D;;GA;;;NU)    
    //       D  - Type: SDDL_ACCESS_DENIED
    //       GA - Rights: SDDL_GENERIC_ALL
    //       NU - Account SID: SDDL_NETWORK
    // 
    // TODO: the ACL (if any) on the named pipe should be controlled by the client instead of hardcoding it here.
    //
    PSECURITY_DESCRIPTOR security_descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl, SECURITY_DESCRIPTOR_REVISION, &security_descriptor, NULL))
    {
        throw http_exception(GetLastError(), "Failed to create security descriptor for the named pipe");
    }

    SECURITY_ATTRIBUTES security_attributes = { 0 };
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.lpSecurityDescriptor = security_descriptor;
    security_attributes.bInheritHandle = false;

    // TODO: add exception-safe cleanup for pipe and thread pool
    auto pipeHandle = CreateNamedPipeW(
        pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        MAX_NUMBER_OF_CONCURRENT_REQUESTS,
        PIPE_BUFFER_SIZE,
        PIPE_BUFFER_SIZE,
        0,
        &security_attributes);

    if (pipeHandle == INVALID_HANDLE_VALUE)
    {
        throw http_exception(GetLastError(), "Failed to create named pipe");
    }

    // TODO: exception-safe cleanup
    LocalFree(security_descriptor);

    auto threadpool_io = CreateThreadpoolIo(pipeHandle, &http_overlapped::io_completion_callback, NULL, NULL);
    if (threadpool_io == nullptr)
    {
        throw http_exception(GetLastError(), "Failed to thread pool for named pipe");
    }

    m_overlapped.set_http_io_completion([this, pipeHandle, threadpool_io](DWORD error, DWORD) {
        on_request_received(pipeHandle, threadpool_io, error);
    });

    StartThreadpoolIo(threadpool_io);

    auto connected = ConnectNamedPipe(pipeHandle, &m_overlapped);

    if (!connected) {
        auto error_code = GetLastError();

        if (error_code != ERROR_IO_PENDING) {
            if (error_code != ERROR_PIPE_CONNECTED) {
                CancelThreadpoolIo(threadpool_io);
                throw http_exception(error_code, "Failed to connect to the named pipe");
            }

            on_request_received(pipeHandle, threadpool_io, NO_ERROR);
        }
    }
}

void named_pipe_listener::on_request_received(HANDLE pipeHandle, TP_IO* threadpool_io, DWORD error_code)
{
    if (error_code == NO_ERROR)
    {
        auto request_context = new named_pipe_request_context(m_listener, pipeHandle, threadpool_io);
        http_request msg = http_request::_create_request(std::unique_ptr<_http_server_context>(request_context));
        request_context->async_process_request(msg);
        async_receive_request();
    }
}

void named_pipe_listener::stop() {
    // TODO: need to sync with async ConnectNamedPipe & outstanding requests
}

/***************************************************************************************************

                                named_pipe_server

***************************************************************************************************/

named_pipe_server::named_pipe_server()
    :
    m_started(false),
    m_listenersLock(),
    m_registeredListeners(),
    m_numOutstandingRequests(0),
    m_zeroOutstandingRequests()
{}

named_pipe_server::~named_pipe_server()
{}

pplx::task<void> named_pipe_server::register_listener(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)
{
    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listenersLock);

        if(m_registeredListeners.find(pListener) != m_registeredListeners.end())
        {
            throw std::invalid_argument("Error: http_listener is already registered");
        }
        m_registeredListeners[pListener] = std::unique_ptr<named_pipe_listener>(new named_pipe_listener(pListener));

        if (m_started)
        {
            m_registeredListeners[pListener]->start();
        }

    }

    return pplx::task_from_result();
}

pplx::task<void> named_pipe_server::unregister_listener(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)
{
    return pplx::create_task([=]()
    {
        // First remove listener registration.
        std::unique_ptr<named_pipe_listener> listener;
        {
            pplx::extensibility::scoped_rw_lock_t lock(m_listenersLock);

            listener = std::move(m_registeredListeners[pListener]);
            m_registeredListeners[pListener] = nullptr;
            m_registeredListeners.erase(pListener);
        }

        // Then take the listener write lock to make sure there are no calls into the listener's
        // request handler.
        {
            pplx::extensibility::scoped_rw_lock_t lock(listener->m_requestHandlerLock);
        }
    });
}

pplx::task<void> named_pipe_server::start()
{
    m_numOutstandingRequests = 0;

    m_zeroOutstandingRequests.set();

    // Start all the registered listeners.
    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listenersLock);

        auto it = m_registeredListeners.begin();
        try
        {
            for (; it != m_registeredListeners.end(); ++it)
            {
                it->second->start();
            }
        }
        catch (...)
        {
            while (it != m_registeredListeners.begin())
            {
                --it;
                it->second->stop();
            }
            return pplx::task_from_exception<void>(std::current_exception());
        }
    }

    m_started = true;

    return pplx::task_from_result();
}

pplx::task<void> named_pipe_server::stop()
{
    // Wait for all requests to be finished processing.
    m_zeroOutstandingRequests.wait();

    // Stop all the registered listeners.
    {
        m_started = false;

        pplx::extensibility::scoped_rw_lock_t lock(m_listenersLock);

        for (auto& listener : m_registeredListeners)
        {
            listener.second->stop();
        }
    }

    return pplx::task_from_result();
}

pplx::task<void> named_pipe_server::respond(http::http_response response)
{
    named_pipe_request_context* p_context = static_cast<named_pipe_request_context *>(response._get_server_context());
    return pplx::create_task(p_context->m_response_completed);
}

std::unique_ptr<http_server> make_http_named_pipe_server()
{
    return std::make_unique<named_pipe_server>();
}

}}}}

#endif
