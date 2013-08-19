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
* http_windows_server.cpp
*
* HTTP Library: implementation of HTTP server API built on Windows HTTP Server APIs.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/http_server.h"
#include "cpprest/http_server_api.h"
#include "cpprest/http_windows_server.h"
#include "cpprest/rawptrstream.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace utility::conversions;
using namespace http::details;
using namespace http::experimental::listener;
using namespace http::experimental::details;
using namespace utility::experimental;
using namespace logging;
using namespace logging::log;


namespace web { namespace http
{
namespace experimental {
namespace details
{

/// <summary>
/// String values for all HTTP Server API known headers.
/// NOTE: the order here is important it is from the _HTTP_HEADER_ID enum.
/// </summary>
static utility::string_t HttpServerAPIKnownHeaders[] =
{
    U("Cache-Control"),
    U("Connection"),
    U("Data"),
    U("Keep-Alive"),
    U("Pragma"),
    U("Trailer"),
    U("Transfer-Encoding"),
    U("Upgrade"),
    U("Via"),
    U("Warning"),
    U("Allow"),
    U("Content-Length"),
    U("Content-Type"),
    U("Content-Encoding"),
    U("Content-Language"),
    U("Content-Location"),
    U("Content-Md5"),
    U("Content-Range"),
    U("Expires"),
    U("Last-Modified"),
    U("Accept"),
    U("Accept-Charset"),
    U("Accept-Encoding"),
    U("Accept-Language"),
    U("Accept-Authorization"),
    U("Cookie"),
    U("Expect"),
    U("From"),
    U("Host"),
    U("If-Match"),
    U("If-Modified-Since"),
    U("If-None-Match"),
    U("If-Range"),
    U("If-Unmodified-Since"),
    U("Max-Forwards"),
    U("Proxy-Authorization"),
    U("Referer"),
    U("Range"),
    U("Te"),
    U("Translate"),
    U("User-Agent"),
    U("Request-Maximum"),
    U("Accept-Ranges"),
    U("Age"),
    U("Etag"),
    U("Location"),
    U("Proxy-Authenticate"),
    U("Retry-After"),
    U("Server"),
    U("Set-Cookie"),
    U("Vary"),
    U("Www-Authenticate"),
    U("Response-Maximum")
};

static void char_to_wstring(utf16string &dest, const char * src)
{
    dest = utility::conversions::to_utf16string(std::string(src));
}

http::method parse_request_method(const HTTP_REQUEST *p_request)
{
    http::method method;

    switch(p_request->Verb)
    {
    case HttpVerbGET:
        method = methods::GET;
        break;
    case HttpVerbPOST:
        method = methods::POST;
        break;
    case HttpVerbPUT:
        method = methods::PUT;
        break;
    case HttpVerbDELETE:
        method = methods::DEL;
        break;
    case HttpVerbHEAD:
        method = methods::HEAD;
        break;
    case HttpVerbOPTIONS:
        method = methods::OPTIONS;
        break;
    case HttpVerbTRACE:
        method = methods::TRCE;
        break;
    case HttpVerbCONNECT:
        method = methods::CONNECT;
        break;
    case HttpVerbUnknown:
        char_to_wstring(method, p_request->pUnknownVerb);
        break;
    case HttpVerbMOVE:
        method = _XPLATSTR("MOVE");
        break;
    case HttpVerbCOPY:
        method = _XPLATSTR("COPY");
        break;
    case HttpVerbPROPFIND:
        method = _XPLATSTR("PROPFIND");
        break;
    case HttpVerbPROPPATCH:
        method = _XPLATSTR("PROPPATCH");
        break;
    case HttpVerbMKCOL:
        method = _XPLATSTR("MKCOL");
        break;
    case HttpVerbLOCK:
        method = _XPLATSTR("LOCK");
        break;
    case HttpVerbUNLOCK:
        method = _XPLATSTR("UNLOCK");
        break;
    case HttpVerbSEARCH:
        method = _XPLATSTR("SEARCH");
        break;
    default:
        break;
    }
    return method;
}

void parse_http_headers(const HTTP_REQUEST_HEADERS &headers, http::http_headers & msgHeaders)
{
    //
    // This is weird for the 'KnownHeaders' but there is no way I can find with the HTTP Server API
    // to get all the raw headers. The known ones are stored in an array index by a HTTP Server API
    // enumeration.
    //
    // TFS 354587 As a perf optimization we could parse the headers from Windows on demand in the
    //      http_header class itself.
    for(USHORT i = 0; i < headers.UnknownHeaderCount; ++i)
    {
        utf16string unknown_header_name;
        char_to_wstring(unknown_header_name, headers.pUnknownHeaders[i].pName);

        // header value can be empty
        if(headers.pUnknownHeaders[i].RawValueLength > 0)
        {
            char_to_wstring(msgHeaders[unknown_header_name], headers.pUnknownHeaders[i].pRawValue);
        }
        else
        {
            msgHeaders[unknown_header_name] = U("");
        }
    }
    for(int i = 0; i < HttpHeaderMaximum; ++i)
    {
        if(headers.KnownHeaders[i].RawValueLength > 0)
        {
            char_to_wstring(msgHeaders[HttpServerAPIKnownHeaders[i]], headers.KnownHeaders[i].pRawValue);
        }
    }
}

//
// Chunk size to use to read request body in.
// 64KB seems a good number based on previous measurements.
//
#define CHUNK_SIZE ((size_t)64*1024)

http_windows_server::http_windows_server()
    : m_connectionCount(0)
{
    HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
    HttpInitialize(httpApiVersion, HTTP_INITIALIZE_SERVER, NULL);
}

http_windows_server::~http_windows_server()
{
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
}

void http_windows_server::connection_reference()
{
    _InterlockedIncrement(&m_connectionCount);
}

void http_windows_server::connection_release()
{
    auto count = _InterlockedDecrement(&m_connectionCount);
    _ASSERTE(count >= 0);
    if (count == 0)
    {
        m_connections_tce.set();
    }
}

pplx::task<void> http_windows_server::register_listener(_In_ http_listener *pListener)
{
    unsigned long errorCode;

    // Add listener registration.
    {
        pplx::extensibility::scoped_rw_lock_t lock(_M_listenersLock);
        if(_M_registeredListeners.find(pListener) != _M_registeredListeners.end())
        {
            throw std::invalid_argument("Error: http_listener is already registered");
        }
        _M_registeredListeners[pListener] = std::unique_ptr<pplx::extensibility::reader_writer_lock_t>(new pplx::extensibility::reader_writer_lock_t());
    }

    http::uri u = pListener->uri();
    if (u.is_port_default())
    {
        // Windows HTTP Server API has issues when the port isn't set to 80 here -- it expects a url prefix string
        // which always includes the port number
        // http://msdn.microsoft.com/en-us/library/windows/desktop/aa364698(v=vs.85).aspx
        http::uri_builder builder(u);
        builder.set_port(80);
        u = builder.to_uri();
    }

    // Windows HTTP Server API will not accept a uri with an empty path, it must have a '/'.
    // Windows HTTP Server API will only accept decoded uri strings.
    utility::string_t host_uri = http::uri::decode(u.to_string());
    if(host_uri.back() != U('/') && u.query().empty() && u.fragment().empty())
    {
        host_uri.append(U("/"));
    }

    // inside here we check for and log a few specific error types that know about
    // there may be more possibilities for windows to return a different error
    errorCode = HttpAddUrlToUrlGroup(m_urlGroupId, host_uri.c_str(), (HTTP_URL_CONTEXT)pListener, 0);
    if(errorCode)
    {
        utility::stringstream_t os;

        if(errorCode == ERROR_ALREADY_EXISTS || errorCode == ERROR_SHARING_VIOLATION)
        {
            os << _XPLATSTR("Address '") << pListener->uri().to_string() << _XPLATSTR("' is already in use");
            return pplx::task_from_exception<void>(http_exception(errorCode, os.str()));
        }
        else if (errorCode == ERROR_ACCESS_DENIED)
        {
            os << _XPLATSTR("Access denied: attempting to add Address '") << pListener->uri().to_string() << _XPLATSTR("'. ");
            os << _XPLATSTR("Run as administrator to listen on an hostname other than localhost, or to listen on port 80.");
            return pplx::task_from_exception<void>(http_exception(errorCode, os.str()));
        }
        else
        {
            return pplx::task_from_exception<void>(http_exception(errorCode, _XPLATSTR("Error adding url to url group")));
        }
    }

    return pplx::task_from_result();
}

pplx::task<void> http_windows_server::unregister_listener(_In_ http_listener *pListener)
{
    // Windows HTTP Server API will not accept a uri with an empty path, it must have a '/'.
    const http::uri listener_uri = pListener->uri();
    utility::string_t host_uri = web::http::uri::decode(listener_uri.to_string());
    if(listener_uri.is_path_empty() && host_uri[host_uri.length() - 1] != '/' && listener_uri.query().empty() && listener_uri.fragment().empty())
    {
        host_uri.append(U("/"));
    }
    const unsigned long error_code = HttpRemoveUrlFromUrlGroup(m_urlGroupId, host_uri.c_str(), 0);

    // First remove listener registration.
    std::unique_ptr<pplx::extensibility::reader_writer_lock_t> pListenerLock;
    {
        pplx::extensibility::scoped_rw_lock_t lock(_M_listenersLock);
        pListenerLock = std::move(_M_registeredListeners[pListener]);
        _M_registeredListeners[pListener] = nullptr;
        _M_registeredListeners.erase(pListener);
    }

    // Then take the listener write lock to make sure there are no calls into the listener's
    // request handler.
    if ( pListenerLock )
    {
        pplx::extensibility::scoped_rw_lock_t lock(*pListenerLock);
    }

    if ( error_code != NO_ERROR )
        return pplx::task_from_exception<void>(http_exception(error_code));
    else
        return pplx::task_from_result();
}

pplx::task<void> http_windows_server::start()
{
    // Initialize data.
    m_serverSessionId = 0; 
    m_urlGroupId = 0;
    m_hRequestQueue = nullptr;
    m_threadpool_io = nullptr;
    m_shuttingDown = 0;

    // Take a connection reference such that its release in stop() would
    // indicate if all connections has been released.
    connection_reference();

    // Open server session.
    HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
    ULONG errorCode = HttpCreateServerSession(httpApiVersion, &m_serverSessionId, 0);
    if(errorCode)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Create Url group.
    errorCode = HttpCreateUrlGroup(m_serverSessionId, &m_urlGroupId, 0);
    if(errorCode)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Create request queue.
    errorCode = HttpCreateRequestQueue(httpApiVersion, NULL, NULL, NULL, &m_hRequestQueue);
    if(errorCode)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Create and start ThreadPool I/O so we can process asynchronous I/O.
    m_threadpool_io = CreateThreadpoolIo(m_hRequestQueue, &http_overlapped::io_completion_callback, NULL, NULL);
    if(m_threadpool_io == nullptr)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Associate Url group with request queue.
    HTTP_BINDING_INFO bindingInfo;
    bindingInfo.RequestQueueHandle = m_hRequestQueue;
    bindingInfo.Flags.Present = 1;
    errorCode = HttpSetUrlGroupProperty(
        m_urlGroupId,
        HttpServerBindingProperty,
        &bindingInfo,
        sizeof(HTTP_BINDING_INFO));
    if(errorCode)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    m_receivingTask = pplx::create_task([this]() { receive_requests(); });
    return pplx::task_from_result();
}

pplx::task<void> http_windows_server::stop()
{
    // Prevent new connections
    if (_InterlockedExchange(&m_shuttingDown, 1) == 1)
        return pplx::task_from_result();

    // Close existing connections.
    close_connections();

    // Shutdown request queue.
    if(m_hRequestQueue != nullptr)
    {
        HttpShutdownRequestQueue(m_hRequestQueue);
        m_receivingTask.wait();
        HttpCloseRequestQueue(m_hRequestQueue);

        // Now that the receiving task is shutdown, no new connections can be made.
        // There shall be no outstanding conncections at this point
        _ASSERTE(m_connections.size() == 0);
    }

    // Release resources.
    if(m_urlGroupId != 0) 
    {
        HttpCloseUrlGroup(m_urlGroupId);
    }
    if(m_serverSessionId != 0) 
    {
        HttpCloseServerSession(m_serverSessionId);
    }
    if(m_threadpool_io != nullptr)
    {
        CloseThreadpoolIo(m_threadpool_io);
        m_threadpool_io = nullptr;
    }

    return pplx::task_from_result();
}

void http_windows_server::close_connections()
{
    {
        // Take the exclusive lock and remove all the connections
        pplx::extensibility::scoped_rw_lock_t lock(m_connections_lock);

        // Reset the TCE. The connections are actually closed asynchronously. When
        // all the connections closed it will set the task_completion_event
        m_connections_tce = pplx::task_completion_event<void>();

        // Remove all the connections from the map
        m_connections.clear();

        // Drop the reference the server keeps (in the constructor).
        // This reference ensures that the right TCE is in place before
        // the reference goes down to 0.
        connection_release();
    }

    // Wait on the TCE after the lock is released
    {
        pplx::create_task(m_connections_tce).wait();
    }
}

void http_windows_server::connection_closed_completion::http_io_completion(DWORD error_code, DWORD)
{
    if ( error_code == NO_ERROR || (error_code == ERROR_OPERATION_ABORTED && m_p_server->m_shuttingDown) )
    {
        m_p_server->remove_connection(m_connection_id);
    }
    else
    {
        report_error(logging::LOG_ERROR, error_code, U("Error: listening for HTTP connection close"));
    }
}

///<summary>
/// Creates a new connection and add it to the map
///</summary>
std::shared_ptr<connection> http_windows_server::add_connection(HTTP_CONNECTION_ID connection_id)
{
    std::shared_ptr<connection> pConnection;

    if (!m_shuttingDown)
    {
        // Insert new connection into the map before we register completion notification
        pplx::extensibility::scoped_rw_lock_t lock(m_connections_lock);

        // Make the check again holding the lock to resolve the race with server::stop.
        // The stop() routine sets the flag and then acquires the lock
        if (!m_shuttingDown)
        {
            pConnection = std::make_shared<connection>(this);
            m_connections[connection_id] = pConnection;
        }
    }

    if (pConnection)
    {

        // Register for completion notification when connection closes.
        http_overlapped *p_overlapped = new http_overlapped(new http_windows_server::connection_closed_completion(this, connection_id));
        StartThreadpoolIo(m_threadpool_io);
        auto error_code = HttpWaitForDisconnect(m_hRequestQueue, connection_id, p_overlapped);
        if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
        {
            CancelThreadpoolIo(m_threadpool_io);
            delete p_overlapped;
            report_error(logging::LOG_ERROR, error_code, U("Error registering for HTTP server disconnect."));

            // Failed to register for completion. Remove the connection
            remove_connection(connection_id);
            pConnection = nullptr;
        }
    }

    return pConnection;
}

///<summary>
/// Retrieves an existing connection if any from the map. 
///</summary>
std::shared_ptr<connection> http_windows_server::find_connection(HTTP_CONNECTION_ID connection_id)
{
    // Return an existing connection.

    std::shared_ptr<connection> pConnection;

    pplx::extensibility::scoped_read_lock_t lock(m_connections_lock);
            
    
    auto iter = m_connections.find(connection_id);
    if(iter != m_connections.end())
    {
        pConnection = iter->second;
    }

    return pConnection;
}

///<summary>
/// Removes the connection from the map
///</summary>
void http_windows_server::remove_connection(HTTP_CONNECTION_ID connection_id)
{
    pplx::extensibility::scoped_rw_lock_t lock(m_connections_lock);
    m_connections.erase(connection_id);
}

void http_windows_server::receive_requests()
{
    HTTP_REQUEST p_request;
    ULONG bytes_received;
    for(;;)
    {
        unsigned long error_code = HttpReceiveHttpRequest(
            m_hRequestQueue,
            HTTP_NULL_ID,
            0,
            &p_request,
            sizeof(HTTP_REQUEST),
            &bytes_received,
            0);

        if(error_code != NO_ERROR && error_code != ERROR_MORE_DATA)
        {
            if(!m_shuttingDown)
            {
                report_error(logging::LOG_ERROR, error_code, U("Error receiving HTTP request."));
            }
            break;
        }

        // Handle existing connection.
        auto pConnection = find_connection(p_request.ConnectionId);

        if (!pConnection)
        {
            // Handle new connection
            pConnection = add_connection(p_request.ConnectionId);
        }

        if (pConnection)
        {
            // Process the request
            http::http_request msg = http::http_request::_create_request(std::unique_ptr<_http_server_context>(new windows_request_context(pConnection)));
            pConnection->async_process_request(p_request.RequestId, msg, bytes_received);
        }
        else
        {
            // Failed to process the request. Cancel and continue
            HttpCancelHttpRequest(m_hRequestQueue, p_request.RequestId, NULL);            
        }
    }
}

pplx::task<void> http_windows_server::respond(http::http_response response)
{
    windows_request_context * p_context = static_cast<windows_request_context *>(response._get_server_context());
    return pplx::create_task(p_context->m_response_completed);
}

connection::connection(_In_ http_windows_server *p_server) 
    : m_p_server(p_server)
{
    // Indicate that there is an open connection
    m_p_server->connection_reference();
}

connection::~connection()
{
    // Indicates that the connection has been closed/deleted
    m_p_server->connection_release();
}

void connection::async_process_request(HTTP_REQUEST_ID request_id, http_request msg, const unsigned long headers_size)
{
    windows_request_context * p_context = static_cast<windows_request_context *>(msg._get_server_context());
    p_context->m_request_id = request_id;

    // Asynchronously read in request headers.
    auto p_io_completion = new read_headers_io_completion(msg, headers_size);
    http_overlapped *p_overlapped = new http_overlapped(p_io_completion);

    StartThreadpoolIo(m_p_server->m_threadpool_io);

    const unsigned long error_code = HttpReceiveHttpRequest(
            m_p_server->m_hRequestQueue,
            p_context->m_request_id,
            0,
            p_io_completion->m_request,
            headers_size,
            NULL,
            p_overlapped);
    
    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(m_p_server->m_threadpool_io);
        delete p_overlapped;
        report_error(logging::LOG_ERROR, error_code, U("Error receiving HTTP headers (HttpReceiveHttpRequest)"));

        msg.reply(status_codes::InternalError);
    }
}

void connection::read_headers_io_completion::http_io_completion(DWORD error_code, DWORD)
{
    if(error_code != NO_ERROR)
    {
        report_error(logging::LOG_ERROR, error_code, U("Error receiving HTTP headers (HttpReceiveHttpRequest)"));
        m_msg.reply(status_codes::InternalError);
    }
    else
    {
        // Parse headers.
        // We have no way of getting at the full raw encoded URI that was sent across the wire
        // so we have to access a already decoded version and re-encode it. But since we are dealing
        // with a full URI we won't handle encoding all possible cases. I don't see any way around this
        // right now.
        // TFS # 392606
        uri encoded_uri = uri::encode_uri(m_request->CookedUrl.pFullUrl);
        m_msg.set_request_uri(encoded_uri);
        m_msg.set_method(parse_request_method(m_request));
        parse_http_headers(m_request->Headers, m_msg.headers());

        // Read in body from the network
        m_msg._get_impl()->_prepare_to_receive_data();

        auto requestContext = static_cast<windows_request_context *>(m_msg._get_server_context());
        requestContext->m_p_connection->read_request_body_chunk(m_request, m_msg);
        // Dispatch request to the http_listener.
        requestContext->m_p_connection->dispatch_request_to_listener(m_msg, (http_listener *)m_request->UrlContext);
    }
}

void connection::read_request_body_chunk(_In_ HTTP_REQUEST *p_request, http_request &msg)
{
    read_body_io_completion *p_read_body_io_completion = new read_body_io_completion(*p_request, msg);
    http_overlapped *p_overlapped = new http_overlapped(p_read_body_io_completion);

    auto request_body_buf = msg._get_impl()->outstream().streambuf();
    auto body = request_body_buf.alloc(CHUNK_SIZE);

    // Once we allow users to set the output stream the following assert could fail.
    // At that time we would need compensation code that would allocate a buffer from the heap instead.
    _ASSERTE(body != nullptr);

    StartThreadpoolIo(m_p_server->m_threadpool_io);
    const ULONG error_code = HttpReceiveRequestEntityBody(
        m_p_server->m_hRequestQueue,
        p_request->RequestId,
        HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER,
        (PVOID)body,
        CHUNK_SIZE,
        NULL,
        p_overlapped);

    if(error_code != ERROR_IO_PENDING && error_code != NO_ERROR)
    {
        // There was no more data to read.
        CancelThreadpoolIo(m_p_server->m_threadpool_io);
        delete p_overlapped;

        if(error_code == ERROR_HANDLE_EOF)
        {
            request_body_buf.commit(0);
            msg._get_impl()->_complete(request_body_buf.in_avail());
        } 
        else
        {
            report_error(logging::LOG_ERROR, error_code, U("Error receiving HTTP request body (HttpReceiveRequestEntityBody)"));
            msg.reply(status_codes::InternalError);
        }
    }
}

void connection::read_body_io_completion::http_io_completion(DWORD error_code, DWORD bytes_read)
{
    if (error_code == NO_ERROR || error_code == ERROR_HANDLE_EOF)
    {
        auto request_body_buf = m_msg._get_impl()->outstream().streambuf();
        request_body_buf.commit(bytes_read);

        auto requestContext = static_cast<windows_request_context *>(m_msg._get_server_context());
        requestContext->m_p_connection->read_request_body_chunk(&m_request, m_msg);
    }
    else
    {
        // Signal that this message should be dropped in the DispatchRequest and send a response
        // to the client.
        report_error(logging::LOG_ERROR, error_code, U("Error receiving HTTP request body (HttpReceiveRequestEntityBody)"));
        m_msg.reply(status_codes::InternalError);
    }
}

void connection::dispatch_request_to_listener(http_request &request, _In_ http_listener *pListener)
{
    request._set_listener_path(pListener->uri().path());

    // Don't let an exception from sending the response bring down the server.
    pplx::task<http_response> response_task = request.get_response();
    response_task.then(
        [request](pplx::task<http::http_response> r_task) mutable
        {
            http::http_response response;
            try
            {
                response = r_task.get();
            }
            catch(const std::exception &e)
            {
                utility::ostringstream_t str_stream;
                str_stream << U("Error: a std::exception was encountered sending the HTTP response: ") << e.what();
                logging::log::post(logging::LOG_ERROR, 0, str_stream.str(), request);
                response = http::http_response(status_codes::InternalError);
            }
            catch(...)
            {
                logging::log::post(logging::LOG_FATAL, 0, U("Fatal Error: an unknown exception was encountered sending the HTTP response."), request);
                response = http::http_response(status_codes::InternalError);
            }
            // Workaround the Dev10 Bug on nested lambda
            struct LambdaDoProcessResponse
            {
                http::http_response response;
                LambdaDoProcessResponse (const http::http_response & response) : response(response) { }
                void operator ()(pplx::task<http_request>) 
                {
                    // Wait until the content download finished before replying.
                    windows_request_context * p_context = static_cast<windows_request_context *>(response._get_server_context());
                    p_context->m_p_connection->async_process_response(response);
                }
            };
            request.content_ready().then(LambdaDoProcessResponse(response));
        });

    // Look up the lock for the http_listener.
    pplx::extensibility::reader_writer_lock_t *pListenerLock;
    {
        pplx::extensibility::scoped_read_lock_t lock(m_p_server->_M_listenersLock);

        // It is possible the listener could have unregistered.
        if(m_p_server->_M_registeredListeners.find(pListener) == m_p_server->_M_registeredListeners.end())
        {
            request.reply(status_codes::NotFound);
            return;
        }
        pListenerLock = m_p_server->_M_registeredListeners[pListener].get();

        // We need to acquire the listener's lock before releasing the registered listeners lock.
        // But we don't need to hold the registered listeners lock when calling into the user's code.
        pListenerLock->lock_read();
    }

    try
    {
        pListener->handle_request(request).then(
            [request](pplx::task<http_response> resp)
            {
                try
                {
                    resp.wait();
                }
                catch(...)
                {
                    auto r = request;
                    r._reply_if_not_already(status_codes::InternalError);
                }
            });

        pListenerLock->unlock();
    } 
    catch(const std::exception &e)
    {
        utility::ostringstream_t str_stream;
        str_stream << U("Error: a std::exception was thrown out of http_listener handle: ") << e.what();
        logging::log::post(logging::LOG_ERROR, 0, str_stream.str(), request);
        pListenerLock->unlock();

        request._reply_if_not_already(status_codes::InternalError);
    }
    catch(...)
    {
        logging::log::post(logging::LOG_ERROR, 0, U("Fatal Error: an unknown exception was thrown out of http_listener handler."), request);
        pListenerLock->unlock();

        request._reply_if_not_already(status_codes::InternalError);
    }
}

void connection::async_process_response(http_response &response)
{
    HTTP_RESPONSE win_api_response;
    ZeroMemory(&win_api_response, sizeof(win_api_response));
    win_api_response.StatusCode = response.status_code();
    const std::string reason = utf16_to_utf8(response.reason_phrase());
    win_api_response.pReason = reason.c_str();
    win_api_response.ReasonLength = (USHORT)reason.size();

    size_t content_length = response._get_impl()->_get_content_length();

    // Create headers.
    // TFS 354587. This isn't great but since we don't store the headers as char * or std::string any more we
    // need to do this conversion instead what we should do is store internally in the http_response
    // the and convert on demand as requested from the headers. 
    send_response_io_completion *p_send_response_overlapped = new send_response_io_completion(response);
    win_api_response.Headers.UnknownHeaderCount = (USHORT)response.headers().size();
    win_api_response.Headers.pUnknownHeaders = p_send_response_overlapped->m_headers.get();
    int headerIndex = 0;
    for(auto iter = response.headers().begin(); iter != response.headers().end(); ++iter, ++headerIndex)
    {
        p_send_response_overlapped->m_headers_buffer[headerIndex * 2] = utf16_to_utf8(iter->first);
        p_send_response_overlapped->m_headers_buffer[headerIndex * 2 + 1] = utf16_to_utf8(iter->second);
        win_api_response.Headers.pUnknownHeaders[headerIndex].NameLength = (USHORT)p_send_response_overlapped->m_headers_buffer[headerIndex * 2].size();
        win_api_response.Headers.pUnknownHeaders[headerIndex].pName = p_send_response_overlapped->m_headers_buffer[headerIndex * 2].c_str();
        win_api_response.Headers.pUnknownHeaders[headerIndex].RawValueLength = (USHORT)p_send_response_overlapped->m_headers_buffer[headerIndex * 2 + 1].size();
        win_api_response.Headers.pUnknownHeaders[headerIndex].pRawValue = p_send_response_overlapped->m_headers_buffer[headerIndex * 2 + 1].c_str();
    }

    // Setup any data.
    windows_request_context * p_context = static_cast<windows_request_context *>(response._get_server_context());
    http_overlapped *p_overlapped = new http_overlapped(p_send_response_overlapped);

    p_context->m_remaining_to_write = content_length;

    // Figure out how to send the entity body of the message.

    if (content_length == 0)
    {
        // There's no data. This is easy!

        StartThreadpoolIo(m_p_server->m_threadpool_io);
        const unsigned long error_code = HttpSendHttpResponse(
            m_p_server->m_hRequestQueue,
            p_context->m_request_id,
            NULL,
            &win_api_response,
            NULL,
            NULL,
            NULL,
            NULL,
            p_overlapped,
            NULL);

        if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
        {
            CancelThreadpoolIo(m_p_server->m_threadpool_io);
            delete p_overlapped;
            report_error(logging::LOG_ERROR, error_code, U("Error sending http response (HttpSendHttpResponse)"));
            p_context->m_response_completed.set_exception(http_exception(error_code));
        }

        return;
    }

    // OK, so we need to chunk it up.
    _ASSERTE(content_length > 0);
    p_context->m_sending_in_chunks = (content_length != std::numeric_limits<size_t>::max());
    p_context->m_transfer_encoding = (content_length == std::numeric_limits<size_t>::max());

    StartThreadpoolIo(m_p_server->m_threadpool_io);
    const unsigned long error_code = HttpSendHttpResponse(
        m_p_server->m_hRequestQueue,
        p_context->m_request_id,
        HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
        &win_api_response,
        NULL,
        NULL,
        NULL,
        NULL,
        p_overlapped,
        NULL);

    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(m_p_server->m_threadpool_io);
        delete p_overlapped;
        report_error(logging::LOG_ERROR, error_code, U("Error sending http response (HttpSendHttpResponse)"));
        p_context->m_response_completed.set_exception(http_exception(error_code));
    }
}

void connection::send_response_io_completion::http_io_completion(DWORD error_code, DWORD)
{
    windows_request_context * p_context = static_cast<windows_request_context *>(m_response._get_server_context());

    if(error_code != NO_ERROR)
    {
        report_error(logging::LOG_ERROR, error_code, U("Error sending http response (HttpSendHttpResponse)"));
        p_context->m_response_completed.set_exception(http_exception(error_code));
        return;
    }

    p_context->transmit_body(m_response);
}

void send_response_body_io_completion::http_io_completion(DWORD error_code, DWORD)
{
    windows_request_context * p_context = static_cast<windows_request_context *>(m_response._get_server_context());

    if(error_code != NO_ERROR)
    {
        report_error(logging::LOG_ERROR, error_code, U("Error sending http response (HttpSendResponseEntityBody)"));
        p_context->m_response_completed.set_exception(http_exception(error_code));
        return;
    }

    p_context->transmit_body(m_response);
}

void cancel_request_io_completion::http_io_completion(DWORD, DWORD)
{
    // Already in an error path so ignore any other error code, we don't want
    // to mask the original underlying error.
    windows_request_context * p_context = static_cast<windows_request_context *>(m_response._get_server_context());
    p_context->m_response_completed.set_exception(m_except_ptr);
}

// Transmit the response body to the network
void windows_request_context::transmit_body(http::http_response response)
{
    if ( !m_sending_in_chunks && !m_transfer_encoding )
    {
        // We are done sending data
        m_response_completed.set();
        return;
    }
    
    // In both cases here we could perform optimizations to try and use acquire on the streams to avoid an extra copy.
    if ( m_sending_in_chunks )
    {
        msl::utilities::SafeInt<size_t> safeCount = m_remaining_to_write;
        size_t next_chunk_size = safeCount.Min(CHUNK_SIZE);
        m_body_data.resize(CHUNK_SIZE);

        streams::rawptr_buffer<unsigned char> buf(&m_body_data[0], next_chunk_size);

        response.body().read(buf, next_chunk_size).then([this, response](pplx::task<size_t> op)
        {
            size_t bytes_read = 0;
            
            // If an exception occurs surface the error to user on the server side
            // and cancel the request so the client sees the error.
            try { bytes_read = op.get(); } catch (...)
            {
                cancel_request(response, std::current_exception());
                return;
            }
            if ( bytes_read == 0 )
            {
                cancel_request(response, std::make_exception_ptr(http_exception(_XPLATSTR("Error unexpectedly encountered the end of the response stream early"))));
                return;
            }

            // Check whether this is the last one to send...
            m_remaining_to_write = m_remaining_to_write-bytes_read;
            m_sending_in_chunks = (m_remaining_to_write > 0);

            send_entity_body(response, &m_body_data[0], bytes_read);
        });
    }
    else
    {
        // We're transfer-encoding...
        const size_t body_data_length = CHUNK_SIZE+http::details::chunked_encoding::additional_encoding_space;
        m_body_data.resize(body_data_length);

        streams::rawptr_buffer<unsigned char> buf(&m_body_data[http::details::chunked_encoding::data_offset], body_data_length);

        auto t1 = response.body().read(buf, CHUNK_SIZE).then([this, response, body_data_length](pplx::task<size_t> op)
        {
            size_t bytes_read = 0;
            
            // If an exception occurs surface the error to user on the server side
            // and cancel the request so the client sees the error.
            try 
            { 
                bytes_read = op.get();
            } catch (...)
            {
                cancel_request(response, std::current_exception());
                return;
            }

            // Check whether this is the last one to send...
            m_transfer_encoding = (bytes_read > 0);
            size_t offset = http::details::chunked_encoding::add_chunked_delimiters(&m_body_data[0], body_data_length, bytes_read);

            auto data_length = bytes_read + (http::details::chunked_encoding::additional_encoding_space-offset);
            send_entity_body(response, &m_body_data[offset], data_length);
        });
    }
}

// Send the body through HTTP.sys
void windows_request_context::send_entity_body(http::http_response response, _In_reads_(data_length) unsigned char * data, _In_ size_t data_length)
{
    HTTP_DATA_CHUNK dataChunk;
    memset(&dataChunk, 0, sizeof(dataChunk));
    dataChunk.DataChunkType = HttpDataChunkFromMemory;
    dataChunk.FromMemory.pBuffer = data;
    dataChunk.FromMemory.BufferLength = (ULONG)data_length;
    const bool this_is_the_last_chunk = !m_transfer_encoding && !m_sending_in_chunks;

    // Send response.
    http_overlapped *p_overlapped = new http_overlapped(new send_response_body_io_completion(response));
    StartThreadpoolIo(m_p_connection->m_p_server->m_threadpool_io);
    auto error_code = HttpSendResponseEntityBody(
        m_p_connection->m_p_server->m_hRequestQueue,
        m_request_id,
        this_is_the_last_chunk ? NULL : HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
        1,
        &dataChunk,
        NULL,
        NULL,
        NULL,
        p_overlapped,
        NULL);

    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(m_p_connection->m_p_server->m_threadpool_io);
        delete p_overlapped;
        report_error(logging::LOG_ERROR, error_code, U("Error sending http response (HttpSendResponseEntityBody)"));
        m_response_completed.set_exception(http_exception(error_code));
    }
}

void windows_request_context::cancel_request(http_response response, std::exception_ptr except_ptr)
{
    http_overlapped *p_overlapped = new http_overlapped(new cancel_request_io_completion(response, except_ptr));
    StartThreadpoolIo(m_p_connection->m_p_server->m_threadpool_io);

    auto error_code = HttpCancelHttpRequest(
        m_p_connection->m_p_server->m_hRequestQueue,
        m_request_id, 
        p_overlapped);
    
    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(m_p_connection->m_p_server->m_threadpool_io);
        delete p_overlapped;
        // An error already occurred so don't report here, because it will just hide the initial error.
    }
}

} // namespace details
} // namespace experimental
}} // namespace web::http
