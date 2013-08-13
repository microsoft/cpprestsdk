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
* http_windows_server.h
*
* HTTP Library: implementation of HTTP server API built on Windows HTTP Server APIs.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _MS_WINDOWS
#error This file is Windows-specific
#endif

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
#error "Error: http server APIs are not supported in XP"
#endif //_WIN32_WINNT < _WIN32_WINNT_VISTA

// Windows Sockets are not code analysis clean.
#pragma warning(push)
#pragma warning(disable : 6386)
#include <http.h>
#pragma warning(pop)

#include <safeint.h>

#include "cpprest/http_server.h"

namespace web { namespace http
{
namespace experimental {

namespace details
{

class http_windows_server;

/// <summary>
/// Interface for asynchronous HTTP I/O completions.
/// </summary>
class http_overlapped_io_completion
{
public:
    virtual ~http_overlapped_io_completion() {}
    virtual void http_io_completion(DWORD error_code, DWORD num_bytes) = 0;
};

/// <summary>
/// Class used to wrap OVERLAPPED I/O with any HTTP I/O.
/// </summary>
class http_overlapped : public OVERLAPPED
{
public:
    // Caller beware:
    // the ctor takes ownership of the object that is deleted at the end of http_overlapped lifetime
    http_overlapped(_In_ http_overlapped_io_completion* completion) : m_completion(completion) 
    {
        ZeroMemory(this, sizeof(OVERLAPPED)); 
    }

    ~http_overlapped()
    {
        delete m_completion;
    }

    /// <summary>
    /// Callback for all I/O completions.
    /// </summary>
    static void CALLBACK io_completion_callback(
        PTP_CALLBACK_INSTANCE instance,
        PVOID context,
        PVOID pOverlapped,
        ULONG result,
        ULONG_PTR numberOfBytesTransferred,
        PTP_IO io)
    {
        UNREFERENCED_PARAMETER(io);
        UNREFERENCED_PARAMETER(context);
        UNREFERENCED_PARAMETER(instance);

        http_overlapped *p_http_overlapped = (http_overlapped *)pOverlapped;
        p_http_overlapped->m_completion->http_io_completion(result, (DWORD)numberOfBytesTransferred);
        delete p_http_overlapped;
    }

private:
    http_overlapped_io_completion* m_completion;
};

/// <summary>
/// Structure which represents a connection and handles
/// the processing of incoming requests and outgoing responses.
/// </summary>
class connection
{
public:
    connection(_In_ http_windows_server *p_server);

    ~connection();

    // Asynchronously starts processing the current request.
    void async_process_request(HTTP_REQUEST_ID request_id, http::http_request msg, const unsigned long headers_size);

    // Handles reading in request headers using overlapped I/O.
    class read_headers_io_completion : public http_overlapped_io_completion
    {
    public:
        read_headers_io_completion(const http::http_request &msg, const unsigned long headers_size) 
            : m_msg(msg), m_request_buffer(new unsigned char[msl::utilities::SafeInt<unsigned long>(headers_size)]) 
        {
            m_request = (HTTP_REQUEST *)m_request_buffer.get();
        }
        void http_io_completion(DWORD error_code, DWORD bytes_read);

        HTTP_REQUEST *m_request;
    private:
        http::http_request m_msg;
        std::unique_ptr<unsigned char[]> m_request_buffer;
    };

    // Helper function to asynchronously read in a request body chunk.
    // Returns true if the entire body has been read in.
    void read_request_body_chunk(_In_ HTTP_REQUEST *p_request, http::http_request &msg);

    // Handles reading request body using overlapped I/O.
    class read_body_io_completion : public http_overlapped_io_completion
    {
    public:
        read_body_io_completion(HTTP_REQUEST request, const http::http_request &msg)
            : m_request(request), m_msg(msg), m_total_body_size(0) {}

        void http_io_completion(DWORD error_code, DWORD bytes_read);

        std::vector<unsigned char> m_body_data;

        size_t m_total_body_size;

    private:
        HTTP_REQUEST m_request;
        http::http_request m_msg;
    };

    // Handles actual dispatching of requests to http_listener.
    void dispatch_request_to_listener(http::http_request &request, _In_ web::http::experimental::listener::http_listener *pListener);

    // Asychrounously sends response.
    void async_process_response(http::http_response &response);

    // Handles sending response using overlapped I/O.
    class send_response_io_completion : public http_overlapped_io_completion
    {
    public:
        send_response_io_completion(http::http_response &response)
            : m_response(response), 
              m_headers(new HTTP_UNKNOWN_HEADER[msl::utilities::SafeInt<size_t>(response.headers().size())]) 
        {
            m_headers_buffer.resize(msl::utilities::SafeInt<size_t>(response.headers().size()) * 2);
        }
        void http_io_completion(DWORD error_code, DWORD bytes_read);

        std::unique_ptr<HTTP_UNKNOWN_HEADER []> m_headers;
        std::vector<std::string> m_headers_buffer;

    private:

        http::http_response m_response;
    };

    // http server this connection belongs to.
    http::experimental::details::http_windows_server *m_p_server;
};

// Handles sending response using overlapped I/O.
class send_response_body_io_completion : public http_overlapped_io_completion
{
public:
    send_response_body_io_completion(http::http_response &response)
        : m_response(response) 
    {
    }

    void http_io_completion(DWORD error_code, DWORD bytes_read);

private:

    http::http_response m_response;
};

// Handles canceling a request using overlapped I/O.
class cancel_request_io_completion : public http_overlapped_io_completion
{
public:
    cancel_request_io_completion(http::http_response response, std::exception_ptr except_ptr)
        : m_response(std::move(response)), m_except_ptr(std::move(except_ptr))
    {
    }
    
    void http_io_completion(DWORD error_code, DWORD bytes_read);

private:
    http::http_response m_response;
    std::exception_ptr m_except_ptr;
};

/// <summary>
/// Context for http request through Windows HTTP Server API.
/// </summary>
struct windows_request_context : http::details::_http_server_context
{
    windows_request_context(std::shared_ptr<connection> p_connection) 
        : m_p_connection(p_connection),
          m_sending_in_chunks(false),
          m_transfer_encoding(false),
          m_remaining_to_write(0)
    {
    }

    void transmit_body(http::http_response response);

    // The connection being used
    std::shared_ptr<connection> m_p_connection;

    // TCE that indicates the completion of response
    pplx::task_completion_event<void> m_response_completed;

    // Id of the currently processed request on this connection.
    HTTP_REQUEST_ID m_request_id;

    bool m_sending_in_chunks;
    bool m_transfer_encoding;

    size_t m_remaining_to_write;

private:
    windows_request_context(const windows_request_context &);
    windows_request_context& operator=(const windows_request_context &);

    // Sends entity body chunk.
    void send_entity_body(http::http_response response, _In_reads_(data_length) unsigned char * data, _In_ size_t data_length);
    
    // Cancels this request.
    void cancel_request(http::http_response response, std::exception_ptr except_ptr);

    std::vector<unsigned char> m_body_data;
};

/// <summary>
/// Class to implement HTTP server API on Windows.
/// </summary>
class http_windows_server : public http_server
{
public:

    /// <summary>
    /// Constructs a http_windows_server.
    /// </summary>
    http_windows_server();

    /// <summary>
    /// Releases resources held.
    /// </summary>
    ~http_windows_server();

    /// <summary>
    /// Start listening for incoming requests.
    /// </summary>
    virtual pplx::task<void> start();

    /// <summary>
    /// Registers an http listener.
    /// </summary>
    virtual pplx::task<void> register_listener(_In_ web::http::experimental::listener::http_listener *pListener);

    /// <summary>
    /// Unregisters an http listener.
    /// </summary>
    virtual pplx::task<void> unregister_listener(_In_ web::http::experimental::listener::http_listener *pListener);

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
    friend class details::connection;
    friend struct details::windows_request_context;

    // Creates a new connection and adds it to m_connections map
    std::shared_ptr<details::connection> add_connection(HTTP_CONNECTION_ID connection_id);

    // Get an existing connection from the m_connections map
    std::shared_ptr<details::connection> find_connection(HTTP_CONNECTION_ID connection_id);

    // Removes a connection from the m_connections map
    void remove_connection(HTTP_CONNECTION_ID connection_id);

    // Indicates that there is an outstanding connection
    void connection_reference();

    // Releases the reference for a connection
    void connection_release();

    // Closes all connections
    void close_connections();

    // Completion callback to handle a connection being closed.
    class connection_closed_completion : public details::http_overlapped_io_completion
    {
    public:
        connection_closed_completion(_In_ http_windows_server *p_server, HTTP_CONNECTION_ID connection_id)
            : m_p_server(p_server), m_connection_id(connection_id) {}
        void http_io_completion(DWORD error_code, DWORD num_bytes);
    private:
        http_windows_server *m_p_server;
        HTTP_CONNECTION_ID m_connection_id;
    };

    // Maps connection to correct connection structure.
    pplx::extensibility::reader_writer_lock_t m_connections_lock;
    std::unordered_map<HTTP_CONNECTION_ID, std::shared_ptr<details::connection>> m_connections;

    // Reference held by each connection
    volatile long m_connectionCount;
    pplx::task_completion_event<void> m_connections_tce;

    // Registered listeners
    pplx::extensibility::reader_writer_lock_t _M_listenersLock;
    std::unordered_map<web::http::experimental::listener::http_listener *, std::unique_ptr<pplx::extensibility::reader_writer_lock_t>> _M_registeredListeners;

    // HTTP Server API server session id.
    HTTP_SERVER_SESSION_ID m_serverSessionId;

    // HTTP Server API url group id.
    HTTP_URL_GROUP_ID m_urlGroupId;

    // Flag used to signal the HTTP server is being shutdown.
    volatile long m_shuttingDown;

    // Handle to HTTP Server API request queue.
    HANDLE m_hRequestQueue;

    // Threadpool I/O structure for overlapped I/O.
    TP_IO * m_threadpool_io;
    
    // Task which actually handles receiving requests from HTTP Server API request queue.
    pplx::task<void> m_receivingTask;
    void receive_requests();
};

} // namespace details;
} // namespace experimental
}} // namespace web::http
