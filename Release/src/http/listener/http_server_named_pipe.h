/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Implementation of HTTP server API built on Windows named pipes APIs.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <atomic>
#include <mutex>

#include "cpprest/details/http_server.h"
#include "http_overlapped.h"

namespace web
{
namespace http
{
namespace experimental
{
namespace details
{

class named_pipe_server;
class named_pipe_listener;
class named_pipe_request_context;

/***************************************************************************************************

                                named_pipe_request_context

***************************************************************************************************/

/// <summary>
/// Context for http request through Windows HTTP Server API.
/// </summary>
class named_pipe_request_context : public http::details::_http_server_context
{
public:
    named_pipe_request_context(web::http::experimental::listener::details::http_listener_impl* listener, HANDLE pipeHandle, TP_IO* threadpool_io);

    virtual ~named_pipe_request_context();

    // Start processing the current request
    void async_process_request(http::http_request msg);
private:
    friend class named_pipe_server; // needs access to the response completed event

    named_pipe_request_context(const named_pipe_request_context &) = delete;
    named_pipe_request_context& operator=(const named_pipe_request_context &) = delete;

    // Dispatch request to the provided http_listener.
    void dispatch_request_to_listener(_In_ web::http::experimental::listener::details::http_listener_impl *pListener);

    enum class ShouldWaitForBody
    {
        Wait,
        DontWait
    };
    // Initialise the response task callbacks. If the body has been requested, we should wait for it to avoid race conditions.
    void init_response_callbacks(ShouldWaitForBody shouldWait);

    // Start processing the response.
    void async_process_response();

    // Read request headers io completion callback function .
    void on_request_received(DWORD error_code, DWORD bytes_read);

    // Send response io completion callback function .
    void on_response_written(DWORD error_code, DWORD bytes_read);

    // Cancels this request.
    void cancel_request(std::exception_ptr except_ptr);

    // Cancel request io completion callback function.
    void on_request_canceled(DWORD error_code, DWORD bytes_read);

    static http::method parse_request_method(std::istream* request_stream);
    static void parse_request_path_and_version(std::istream* request_stream, std::string* path, web::http::http_version* version);
    static void parse_http_headers(std::istream* request_stream, http::http_headers& headers);

    web::http::experimental::listener::details::http_listener_impl* m_listener;

    HANDLE m_pipeHandle;
    TP_IO* m_threadpool_io;
    http_overlapped m_overlapped;

    std::unique_ptr<unsigned char[]> m_request_buffer;

    http_request m_msg;
    http_response m_response;

    std::exception_ptr m_except_ptr;

    // TCE that indicates the completion of response
    pplx::task_completion_event<void> m_response_completed;
    // Lock used as a workaround for ppl task_completion_event bug.
    std::mutex m_responseCompletedLock;
};

/***************************************************************************************************

                                named_pipe_listener

***************************************************************************************************/

class named_pipe_listener
{
public:
    static const DWORD PIPE_BUFFER_SIZE = 64 * 1024;

    named_pipe_listener(web::http::experimental::listener::details::http_listener_impl* listener);

    void start();

    void stop();

private:
    friend class named_pipe_request_context; // needs access to the request handler lock
    friend class named_pipe_server; // needs access to the request handler lock

    named_pipe_listener(const named_pipe_listener &) = delete;
    named_pipe_listener& operator=(const named_pipe_listener &) = delete;

    web::http::experimental::listener::details::http_listener_impl* m_listener;

    http_overlapped m_overlapped;

    // Request handler lock to guard against removing a listener while in user code.
    pplx::extensibility::reader_writer_lock_t m_requestHandlerLock;

    void async_receive_request();
    void on_request_received(HANDLE pipeHandle, TP_IO* threadpool_io, DWORD error_code);
};

/***************************************************************************************************

                                named_pipe_server

***************************************************************************************************/

/// <summary>
/// Class to implement HTTP server API on Windows.
/// </summary>
class named_pipe_server : public http_server
{
public:

    /// <summary>
    /// Constructs a named_pipe_server.
    /// </summary>
    named_pipe_server();

    /// <summary>
    /// Releases resources held.
    /// </summary>
    ~named_pipe_server();

    /// <summary>
    /// Start listening for incoming requests.
    /// </summary>
    virtual pplx::task<void> start();

    /// <summary>
    /// Registers an http listener.
    /// </summary>
    virtual pplx::task<void> register_listener(_In_ web::http::experimental::listener::details::http_listener_impl* pListener);

    /// <summary>
    /// Unregisters an http listener.
    /// </summary>
    virtual pplx::task<void> unregister_listener(_In_ web::http::experimental::listener::details::http_listener_impl* pListener);

    /// <summary>
    /// Stop processing and listening for incoming requests.
    /// </summary>
    virtual pplx::task<void> stop();

    /// <summary>
    /// Asynchronously sends the specified http response.
    /// </summary>
    /// <param name="response">The http_response to send.</param>
    /// <returns>A operation which is completed once the response has been sent.</returns>
    virtual pplx::task<void> respond(http::http_response response);

private:
    friend class details::named_pipe_request_context; // needs access to the synchronization objects

    named_pipe_server(const named_pipe_server &) = delete;
    named_pipe_server& operator=(const named_pipe_server &) = delete;

    bool m_started;

    // Registered listeners
    pplx::extensibility::reader_writer_lock_t m_listenersLock;
    std::unordered_map<web::http::experimental::listener::details::http_listener_impl*, std::unique_ptr<named_pipe_listener>> m_registeredListeners;

    // Tracks the number of outstanding requests being processed.
    std::atomic<int> m_numOutstandingRequests;
    pplx::extensibility::event_t m_zeroOutstandingRequests;
};

} // namespace details;
} // namespace experimental
}} // namespace web::http
