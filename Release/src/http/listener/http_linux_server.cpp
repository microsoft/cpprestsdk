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
*/
#include "stdafx.h"
#include <boost/algorithm/string/find.hpp>
#include "cpprest/http_helpers.h"
#include "cpprest/http_server_api.h"
#include "cpprest/http_server.h"
#include "cpprest/http_linux_server.h"
#include "cpprest/producerconsumerstream.h"
#include "cpprest/log.h"
#include "cpprest/filelog.h"
#define CRLF std::string("\r\n")

using namespace utility::experimental::logging;
namespace web
{
namespace http
{
namespace experimental 
{
namespace listener
{

    const size_t ChunkSize = 4 * 1024;

namespace details
{

void hostport_listener::start()
{
    // resolve the endpoint address
    auto& service = crossplat::threadpool::shared_instance().service();
    tcp::resolver resolver(service);
    tcp::resolver::query query(m_host, m_port);
    tcp::endpoint endpoint = *resolver.resolve(query);

    m_acceptor.reset(new tcp::acceptor(service, endpoint));
    m_acceptor->set_option(tcp::acceptor::reuse_address(true));

    auto socket = new ip::tcp::socket(service);
    m_acceptor->async_accept(*socket, boost::bind(&hostport_listener::on_accept, this, socket, placeholders::error));
}

void connection::close()
{
    m_close = true;
    auto sock = m_socket.get();
    if (sock != nullptr)
    {
        boost::system::error_code ec;
        sock->cancel(ec);
        sock->shutdown(tcp::socket::shutdown_both, ec);
        sock->close();
    }
    m_request._reply_if_not_already(status_codes::InternalError);
}

void connection::start_request_response()
{
    m_read_size = 0; m_read = 0;
    m_request_buf.consume(m_request_buf.size()); // clear the buffer
    async_read_until(*m_socket, m_request_buf, CRLF + CRLF, boost::bind(&connection::handle_http_line, this, placeholders::error));
}

void hostport_listener::on_accept(ip::tcp::socket* socket, const boost::system::error_code& ec)
{
    if (ec)
    {
        delete socket;
    }
    else
    {
        {
            pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_connections_lock);
            m_connections.insert(new connection(std::unique_ptr<tcp::socket>(std::move(socket)), m_p_server, this));
            m_all_connections_complete.reset();
            
            if (m_acceptor)
            {
                // spin off another async accept
                auto newSocket = new ip::tcp::socket(crossplat::threadpool::shared_instance().service());
                m_acceptor->async_accept(*newSocket, boost::bind(&hostport_listener::on_accept, this, newSocket, placeholders::error));
            }
        }

    }
}

void connection::handle_http_line(const boost::system::error_code& ec)
{
    m_request = http_request::_create_request(std::unique_ptr<http::details::_http_server_context>(new linux_request_context()));
    if (ec)
    {
        // client closed connection
        if (ec == boost::asio::error::eof)
        {
            finish_request_response();
        }
        else
        {
            m_request.reply(status_codes::InternalError);
            do_response();
        }
    }
    else
    {
        // read http status line

        std::istream request_stream(&m_request_buf);
        std::skipws(request_stream);

        std::string http_verb;
        request_stream >> http_verb;

        if (boost::iequals(http_verb, http::methods::GET))          http_verb = http::methods::GET;
        else if (boost::iequals(http_verb, http::methods::POST))    http_verb = http::methods::POST;
        else if (boost::iequals(http_verb, http::methods::PUT))     http_verb = http::methods::PUT;
        else if (boost::iequals(http_verb, http::methods::DEL))     http_verb = http::methods::DEL;
        else if (boost::iequals(http_verb, http::methods::HEAD))    http_verb = http::methods::HEAD;
        else if (boost::iequals(http_verb, http::methods::TRCE))    http_verb = http::methods::TRCE;
        else if (boost::iequals(http_verb, http::methods::CONNECT)) http_verb = http::methods::CONNECT;
        else if (boost::iequals(http_verb, http::methods::OPTIONS)) http_verb = http::methods::OPTIONS;
        m_request.set_method(http_verb);

        std::string http_path_and_version;
        std::getline(request_stream, http_path_and_version);
        const size_t VersionPortionSize = sizeof(" HTTP/1.1\r") - 1;

        // Get the host part of the address. This is a bit of a hack, like the Windows one...
        std::string host_part = "http://" + m_p_parent->m_host + ":" + m_p_parent->m_port;
        uri request_uri = uri::encode_uri(host_part);
        // Get the path - remove the version portion and prefix space
        request_uri = uri_builder(request_uri).append(http_path_and_version.substr(1, http_path_and_version.size() - VersionPortionSize - 1)).to_uri();
        m_request.set_request_uri(request_uri);

        // Get the version
        std::string http_version = http_path_and_version.substr(http_path_and_version.size() - VersionPortionSize + 1, VersionPortionSize - 2);
        // if HTTP version is 1.0 then disable pipelining
        if (http_version == "HTTP/1.0")
        {
            m_close = true;
        }

        handle_headers();
    }
}

void connection::handle_headers()
{
    std::istream request_stream(&m_request_buf);
    std::string header;
    while (std::getline(request_stream, header) && header != "\r")
    {
        auto colon = header.find(':');
        if (colon != std::string::npos)
        {
            auto name = header.substr(0, colon);
            auto value = header.substr(colon+1, header.length() - (colon+1)); // also exclude '\r'
            http::details::trim_whitespace(name);
            http::details::trim_whitespace(value);

            auto& currentValue = m_request.headers()[name];
            if (currentValue.empty() || boost::iequals(name, U("content-length"))) // (content-length is already set)
            {
                currentValue = value;
            }
            else currentValue += U(", ") + value;
        }
        else
        {
            m_request.reply(status_codes::BadRequest);
            do_response();
            return;
        }
    }

    m_close = m_chunked = false;
    utility::string_t name;
    // check if the client has requested we close the connection
    if (m_request.headers().match(U("connection"), name))
    {
        m_close = boost::iequals(name, U("close"));
    }

    if (m_request.headers().match(U("transfer-encoding"), name))
    {
        m_chunked = boost::ifind_first(name, U("chunked"));
    }

    Concurrency::streams::producer_consumer_buffer<uint8_t> buf;
    m_request._get_impl()->set_instream(buf.create_istream());
    m_request._get_impl()->set_outstream(buf.create_ostream(), false);
    if (m_chunked)
    {
        boost::asio::async_read_until(*m_socket, m_request_buf, CRLF, boost::bind(&connection::handle_chunked_header, this, placeholders::error));
        dispatch_request_to_listener();
        return;
    }

    if (!m_request.headers().match(U("content-length"), m_read_size))
        m_read_size = 0;

        if (m_read_size == 0)
        {
            request_data_avail( 0);
        }
        else // need to read the sent data
        {
            m_read = 0;
            async_read_until_buffersize(std::min(ChunkSize, m_read_size), boost::bind(&connection::handle_body, this, placeholders::error));
        }

        dispatch_request_to_listener();
    }

void connection::handle_chunked_header(const boost::system::error_code& ec)
{
    if (!ec)
    {
        std::istream is(&m_request_buf);
        int len;
        is >> std::hex >> len;
        m_request_buf.consume(CRLF.size());
        m_read += len;
        if (len == 0)
            request_data_avail(m_read);
        else
            async_read_until_buffersize(len + 2, boost::bind(&connection::handle_chunked_body, this, boost::asio::placeholders::error, len));
    }
    else
    {
        m_request._reply_if_not_already(status_codes::BadRequest);
    }
}

void connection::handle_chunked_body(const boost::system::error_code& ec, int toWrite)
{
    if (!ec)
    {
        auto writebuf = m_request._get_impl()->outstream().streambuf();
        writebuf.putn(buffer_cast<const uint8_t *>(m_request_buf.data()), toWrite).then([=](size_t) {
            m_request_buf.consume(2 + toWrite);

            boost::asio::async_read_until(*m_socket, m_request_buf, CRLF, 
                    boost::bind(&connection::handle_chunked_header, this, placeholders::error));
        });
    }
    else
    {
        m_request._reply_if_not_already(status_codes::BadRequest);
    }
}

void connection::handle_body(const boost::system::error_code& ec)
{
    // read body
    if (ec)
    {
        m_request._reply_if_not_already(status_codes::BadRequest);
    }
    else if (m_read < m_read_size)  // there is more to read
    {
        auto writebuf = m_request._get_impl()->outstream().streambuf();
        writebuf.putn(boost::asio::buffer_cast<const uint8_t*>(m_request_buf.data()), std::min(m_request_buf.size(), m_read_size - m_read)).then([=](size_t writtenSize) {
            m_read += writtenSize;
            m_request_buf.consume(writtenSize);
            async_read_until_buffersize(std::min(ChunkSize, m_read_size - m_read), boost::bind(&connection::handle_body, this, placeholders::error));
        });
    }
    else  // have read request body
    {
        request_data_avail(m_read);
    }
}


template <typename ReadHandler>
void connection::async_read_until_buffersize(size_t size, ReadHandler handler)
{
    auto bufsize = m_request_buf.size();
    if (bufsize >= size)
        boost::asio::async_read(*m_socket, m_request_buf, transfer_at_least(0), handler);
    else
        boost::asio::async_read(*m_socket, m_request_buf, transfer_at_least(size - bufsize), handler);
}

void connection::dispatch_request_to_listener()
{
    // locate the listener:
    http_listener* pListener = nullptr;
    {
        auto path_segments = uri::split_path(uri::decode(m_request.relative_uri().path()));
        for (auto i = static_cast<long>(path_segments.size()); i >= 0; --i)
        {
            std::string path = "";
            for (size_t j = 0; j < static_cast<size_t>(i); ++j)
            {
                path += "/" + path_segments[j];
            }
            path += "/";

            pplx::extensibility::scoped_read_lock_t lock(m_p_parent->m_listeners_lock);
            auto it = m_p_parent->m_listeners.find(path);
            if (it != m_p_parent->m_listeners.end())
            {
                pListener = it->second;
                break;
            }
        }
    }

    if (pListener == nullptr)
    {
        m_request.reply(status_codes::NotFound);
        do_response();
    }
    else
    {
        m_request._set_listener_path(pListener->uri().path());
        do_response();
        
        // Look up the lock for the http_listener.
        pplx::extensibility::reader_writer_lock_t *pListenerLock;
        {
            pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_p_server->m_listeners_lock);

            // It is possible the listener could have unregistered.
            if(m_p_server->m_registered_listeners.find(pListener) == m_p_server->m_registered_listeners.end())
            {
                m_request.reply(status_codes::NotFound);
                return;
            }
            pListenerLock = m_p_server->m_registered_listeners[pListener].get();

            // We need to acquire the listener's lock before releasing the registered listeners lock.
            // But we don't need to hold the registered listeners lock when calling into the user's code.
            pListenerLock->lock_read();
        }

        try
        {
            pListener->handle_request(m_request);
            pListenerLock->unlock();
        } 
        catch(const std::exception &e)
        {
            pListenerLock->unlock();
            std::ostringstream str_stream;
            str_stream << "Error: a std::exception was thrown out of http_listener handle: " << e.what();
            utility::experimental::logging::log::post(utility::experimental::logging::LOG_ERROR, 0, str_stream.str(), m_request);

            m_request._reply_if_not_already(status_codes::InternalError);
        }
        catch(...)
        {
            pListenerLock->unlock();
            log::post(LOG_FATAL, 0, "Fatal Error: an unknown exception was thrown out of http_listener handler.", m_request);

            m_request._reply_if_not_already(status_codes::InternalError);
        }
    }

    if (--m_refs == 0)
        delete this;
}

void connection::request_data_avail(size_t size)
{
    // closing writing data
    m_request._get_impl()->_complete(size);
}

void connection::do_response()
{
    ++m_refs;
    pplx::task<http_response> response_task = m_request.get_response();

    response_task.then(
        [=](pplx::task<http::http_response> r_task)
        {
            http::http_response response;
            try
            {
                response = r_task.get();
            }
            catch(const std::exception& ex)
            {
                std::ostringstream str_stream;
                str_stream << "Error in listener: " << ex.what() << std::endl;
                log::post(LOG_ERROR, 0, str_stream.str(), m_request);
                response = http::http_response(status_codes::InternalError);
            }
            catch(...)
            {
                log::post(LOG_FATAL, 0, "Fatal Error: an unknown exception was thrown out of http_listener handler.", m_request);
                response = http::http_response(status_codes::InternalError);
            }
            // before sending response, the full incoming message need to be processed.
            m_request.content_ready().then([=](pplx::task<http::http_request>) {
                async_process_response(response);
            });
        });
}

void connection::async_process_response(http_response response)
{
    m_response_buf.consume(m_response_buf.size()); // clear the buffer
    std::ostream os(&m_response_buf);

    os << "HTTP/1.1 " << response.status_code() << " " 
        << response.reason_phrase()
        << CRLF;

    m_chunked = false;
    m_write = m_write_size = 0;

    std::string transferencoding;
    if (response.headers().match(header_names::transfer_encoding, transferencoding) && transferencoding == "chunked")
    {
        m_chunked  = true;
    }
    if (!response.headers().match(header_names::content_length, m_write_size) && response.body())
    {
        m_chunked = true;
        response.headers()[header_names::transfer_encoding] = U("chunked");
    }

    for (auto it = response.headers().begin();
            it != response.headers().end(); ++it)
    {
        // check if the responder has requested we close the connection
        if (boost::iequals(it->first, U("connection"))) {
            if (boost::iequals(it->second, U("close")))
            {
                m_close = true;
            }
        }
        os << it->first << ": "
           << it->second << CRLF;
    }
    os << CRLF;

    boost::asio::async_write(*m_socket, m_response_buf, boost::bind(&connection::handle_headers_written, this, response, placeholders::error));
}


void connection::handle_write_chunked_response(http_response response, const boost::system::error_code& ec)
{
    if (ec)
        return handle_response_written(response, ec);

    auto readbuf = response._get_impl()->instream().streambuf();
    auto membuf = m_response_buf.prepare(ChunkSize + http::details::chunked_encoding::additional_encoding_space);

    readbuf.getn(buffer_cast<uint8_t *>(membuf) + http::details::chunked_encoding::data_offset, ChunkSize).then([=](size_t actualSize) {
        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buffer_cast<uint8_t *>(membuf), ChunkSize+http::details::chunked_encoding::additional_encoding_space, actualSize);
        m_response_buf.commit(actualSize + http::details::chunked_encoding::additional_encoding_space);
        m_response_buf.consume(offset);
        boost::asio::async_write(*m_socket, m_response_buf,
            boost::bind(actualSize == 0 ? &connection::handle_response_written : &connection::handle_write_chunked_response, this, response, placeholders::error));
    });
}


void connection::handle_write_large_response(http_response response, const boost::system::error_code& ec)
{
    if (ec || m_write == m_write_size)
        return handle_response_written(response, ec);

    auto readbuf = response._get_impl()->instream().streambuf();
    size_t readBytes = std::min(ChunkSize, m_write_size - m_write);
    readbuf.getn(buffer_cast<uint8_t *>(m_response_buf.prepare(readBytes)), readBytes).then([=](size_t actualSize) {
        m_write += actualSize;
        m_response_buf.commit(actualSize);
        boost::asio::async_write(*m_socket, m_response_buf, boost::bind(&connection::handle_write_large_response, this, response, placeholders::error));
    });
}

void connection::handle_headers_written(http_response response, const boost::system::error_code& ec)
{
    if (ec)
    {
        auto * context = static_cast<linux_request_context*>(response._get_server_context());
        context->m_response_completed.set_exception(std::runtime_error("error writing headers"));
        finish_request_response();
    }
    else
    {
        if (m_chunked)
            handle_write_chunked_response(response, ec);
        else
            handle_write_large_response(response, ec);
    }
}

void connection::handle_response_written(http_response response, const boost::system::error_code& ec)
{
    auto * context = static_cast<linux_request_context*>(response._get_server_context());
    if (ec)
    {
        //printf("-> boost::system::error_code %d in handle_response_written\n", ec.value());
        context->m_response_completed.set_exception(std::runtime_error("error writing response"));
        finish_request_response();
    }
    else
    {
        context->m_response_completed.set();
        if (!m_close)
        {
            start_request_response();
        }
        else
        {
            finish_request_response();
        }
    }
}

void connection::finish_request_response()
{
    // kill the connection
    {
        pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_p_parent->m_connections_lock);
        m_p_parent->m_connections.erase(this);
        if (m_p_parent->m_connections.size() == 0)
            m_p_parent->m_all_connections_complete.set();
    }

    close();
    if (--m_refs == 0)
    delete this;
}

void hostport_listener::stop()
{

    // halt existing connections
    {
        pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_connections_lock);
        m_acceptor.reset();
        for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
        {
            (*it)->close();
        }
    }

    m_all_connections_complete.wait();
}

void hostport_listener::add_listener(const std::string& path, http_listener* listener)
{
    pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);

    if (!m_listeners.insert(std::map<std::string,http_listener*>::value_type(path, listener)).second)
        throw std::invalid_argument("Error: http_listener is already registered for this path");
}

void hostport_listener::remove_listener(const std::string& path, http_listener*)
{
    pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);

    if (m_listeners.erase(path) != 1)
        throw std::invalid_argument("Error: no http_listener found for this path");

}

}

pplx::task<void> http_linux_server::start()
{
    pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_listeners_lock);

    auto it = m_listeners.begin();
    try
    {
        for (; it != m_listeners.end(); ++it)
        {
            it->second->start();
        }
    }
    catch (...)
    {
        --it;
        while (it != m_listeners.begin())
        {
            it->second->stop();
            --it;
        }
        it->second->stop();
        return pplx::task_from_exception<void>(std::current_exception());
    }

    m_started = true;
    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::stop()
{
    pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_listeners_lock);

    m_started = false;

    for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
    {
        it->second->stop();
    }

    return pplx::task_from_result();
}

std::pair<std::string,std::string> canonical_parts(const http::uri& uri)
{
    auto host = uri.host();
    auto port = uri.port();
    std::ostringstream endpoint;
    endpoint << uri::decode(host) << ":" << port;

    auto path = uri::decode(uri.path());

    if (path.size() > 1 && path[path.size()-1] != '/')
    {
        path += "/"; // ensure the end slash is present
    }

    return std::make_pair(endpoint.str(), path);
}

pplx::task<void> http_linux_server::register_listener(http_listener* listener)
{
    auto parts = canonical_parts(listener->uri());
    auto hostport = parts.first;
    auto path = parts.second;

    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);
        if (m_registered_listeners.find(listener) != m_registered_listeners.end())
            throw std::invalid_argument("listener already registered");

        m_registered_listeners[listener] = std::unique_ptr<pplx::extensibility::reader_writer_lock_t>(new pplx::extensibility::reader_writer_lock_t());

        auto found_hostport_listener = m_listeners.find(hostport);
        if (found_hostport_listener == m_listeners.end())
        {
            found_hostport_listener = m_listeners.insert(
                std::make_pair(hostport, std::unique_ptr<hostport_listener>(new hostport_listener(this, hostport)))).first;

            if (m_started)
                found_hostport_listener->second->start();
        }

        found_hostport_listener->second->add_listener(path, listener);
    }

    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::unregister_listener(http_listener* listener)
{
    auto parts = canonical_parts(listener->uri());
    auto hostport = parts.first;
    auto path = parts.second;
    // First remove the listener from hostport listener
    {
        pplx::extensibility::scoped_read_lock_t lock(m_listeners_lock);
        auto itr = m_listeners.find(hostport);
        if (itr == m_listeners.end())
        {
            throw std::invalid_argument("Error: no listener registered for that host");
        }

        itr->second->remove_listener(path, listener);
    }

    // Second remove the listener form listener collection
    std::unique_ptr<pplx::extensibility::reader_writer_lock_t> pListenerLock = nullptr;
    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);
        pListenerLock = std::move(m_registered_listeners[listener]);
        m_registered_listeners[listener] = nullptr;
        m_registered_listeners.erase(listener);
    }

    // Then take the listener write lock to make sure there are no calls into the listener's
    // request handler.
    if (pListenerLock != nullptr)
    {
        pplx::extensibility::scoped_rw_lock_t lock(*pListenerLock);
    }

    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::respond(http::http_response response)
{
    linux_request_context * p_context = static_cast<linux_request_context*>(response._get_server_context());
    return pplx::create_task(p_context->m_response_completed);
}

}
}
}
}

