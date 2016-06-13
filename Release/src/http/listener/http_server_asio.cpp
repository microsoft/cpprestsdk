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
* HTTP Library: HTTP listener (server-side) APIs

* This file contains a cross platform implementation based on Boost.ASIO.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
*/
#include "stdafx.h"
#include <boost/algorithm/string/find.hpp>
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

using namespace boost::asio;
using namespace boost::asio::ip;

#define CRLF std::string("\r\n")
#define CRLFCRLF std::string("\r\n\r\n")

namespace web
{
namespace http
{
namespace experimental
{
namespace listener
{
namespace details
{
/// This class replaces the regex "\r\n\r\n|[\x00-\x1F]|[\x80-\xFF]"
// It was added due to issues with regex on Android, however since
// regex was rather overkill for such a simple parse it makes sense
// to use it on all *nix platforms.
//
// This is used as part of the async_read_until call below; see the
// following for more details:
// http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/async_read_until/overload4.html
struct crlfcrlf_nonascii_searcher_t
{
    enum class State
    {
        none = 0,   // ".\r\n\r\n$"
        cr = 1,     // "\r.\n\r\n$"
                    // "  .\r\n\r\n$"
        crlf = 2,   // "\r\n.\r\n$"
                    // "    .\r\n\r\n$"
        crlfcr = 3  // "\r\n\r.\n$"
                    // "    \r.\n\r\n$"
                    // "      .\r\n\r\n$"
    };

    // This function implements the searcher which "consumes" a certain amount of the input
    // and returns whether or not there was a match (see above).

    // From the Boost documentation:
    // "The first member of the return value is an iterator marking one-past-the-end of the
    //  bytes that have been consumed by the match function. This iterator is used to
    //  calculate the begin parameter for any subsequent invocation of the match condition.
    //  The second member of the return value is true if a match has been found, false
    //  otherwise."
    template<typename Iter>
    std::pair<Iter, bool> operator()(const Iter begin, const Iter end) const
    {
        // In the case that we end inside a partially parsed match (like abcd\r\n\r),
        // we need to signal the matcher to give us the partial match back again (\r\n\r).
        // We use the excluded variable to keep track of this sequence point (abcd.\r\n\r
        // in the previous example).
        Iter excluded = begin;
        Iter cur = begin;
        State state = State::none;
        while (cur != end)
        {
            char c = *cur;
            if (c == '\r')
            {
                if (state == State::crlf)
                {
                    state = State::crlfcr;
                }
                else
                {
                    // In the case of State::cr or State::crlfcr, setting the state here
                    // "skips" a none state and therefore fails to move up the excluded
                    // counter.
                    excluded = cur;
                    state = State::cr;
                }
            }
            else if (c == '\n')
            {
                if (state == State::cr)
                {
                    state = State::crlf;
                }
                else if (state == State::crlfcr)
                {
                    ++cur;
                    return std::make_pair(cur, true);
                }
                else
                {
                    state = State::none;
                }
            }
            else if (c <= '\x1F' && c >= '\x00')
            {
                ++cur;
                return std::make_pair(cur, true);
            }
            else if (c <= '\xFF' && c >= '\x80')
            {
                ++cur;
                return std::make_pair(cur, true);
            }
            else
            {
                state = State::none;
            }
            ++cur;
            if (state == State::none)
                excluded = cur;
        }
        return std::make_pair(excluded, false);
    }
} crlfcrlf_nonascii_searcher;
}}}}}

namespace boost
{
namespace asio
{
template <> struct is_match_condition<web::http::experimental::listener::details::crlfcrlf_nonascii_searcher_t> : public boost::true_type {};
}}

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
        sock->close(ec);
    }
    m_request._reply_if_not_already(status_codes::InternalError);
}

void connection::start_request_response()
{
    m_read_size = 0;
    m_read = 0;
    m_request_buf.consume(m_request_buf.size()); // clear the buffer

    if (m_ssl_stream)
    {
        boost::asio::async_read_until(*m_ssl_stream, m_request_buf, CRLFCRLF, [this](const boost::system::error_code& ec, std::size_t)
        {
            this->handle_http_line(ec);
        });
    }
    else
    {
        boost::asio::async_read_until(*m_socket, m_request_buf, crlfcrlf_nonascii_searcher, [this](const boost::system::error_code& ec, std::size_t)
        {
            this->handle_http_line(ec);
        });
    }
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
            m_connections.insert(new connection(std::unique_ptr<tcp::socket>(std::move(socket)), m_p_server, this, m_is_https, m_ssl_context_callback));
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
        if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted)
        {
            finish_request_response();
        }
        else
        {
            m_request._reply_if_not_already(status_codes::BadRequest);
            m_close = true;
            do_response(true);
        }
    }
    else
    {
        // read http status line
        std::istream request_stream(&m_request_buf);
        request_stream.imbue(std::locale::classic());
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

        // Check to see if there is not allowed character on the input
        if (!web::http::details::validate_method(http_verb))
        {
            m_request.reply(status_codes::BadRequest);
            m_close = true;
            do_response(true);
            return;
        }

        m_request.set_method(http_verb);

        std::string http_path_and_version;
        std::getline(request_stream, http_path_and_version);
        const size_t VersionPortionSize = sizeof(" HTTP/1.1\r") - 1;

        // Make sure path and version is long enough to contain the HTTP version
        if(http_path_and_version.size() < VersionPortionSize + 2)
        {
            m_request.reply(status_codes::BadRequest);
            m_close = true;
            do_response(true);
            return;
        }

        // Get the path - remove the version portion and prefix space
        try
        {
            m_request.set_request_uri(http_path_and_version.substr(1, http_path_and_version.size() - VersionPortionSize - 1));
        }
        catch(const uri_exception &e)
        {
            m_request.reply(status_codes::BadRequest, e.what());
            m_close = true;
            do_response(true);
            return;
        }

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
    request_stream.imbue(std::locale::classic());
    std::string header;
    while (std::getline(request_stream, header) && header != "\r")
    {
        auto colon = header.find(':');
        if (colon != std::string::npos && colon != 0)
        {
            auto name = header.substr(0, colon);
            auto value = header.substr(colon + 1, header.length() - (colon + 1)); // also exclude '\r'
            http::details::trim_whitespace(name);
            http::details::trim_whitespace(value);

            auto& currentValue = m_request.headers()[name];
            if (currentValue.empty() || boost::iequals(name, header_names::content_length)) // (content-length is already set)
            {
                currentValue = value;
            }
            else
            {
                currentValue += U(", ") + value;
            }
        }
        else
        {
            m_request.reply(status_codes::BadRequest);
            m_close = true;
            do_response(true);
            return;
        }
    }

    m_close = m_chunked = false;
    utility::string_t name;
    // check if the client has requested we close the connection
    if (m_request.headers().match(header_names::connection, name))
    {
        m_close = boost::iequals(name, U("close"));
    }

    if (m_request.headers().match(header_names::transfer_encoding, name))
    {
        m_chunked = boost::ifind_first(name, U("chunked"));
    }

    m_request._get_impl()->_prepare_to_receive_data();
    if (m_chunked)
    {
        async_read_until();
        dispatch_request_to_listener();
        return;
    }

    if (!m_request.headers().match(header_names::content_length, m_read_size))
    {
        m_read_size = 0;
    }

    if (m_read_size == 0)
    {
        m_request._get_impl()->_complete(0);
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
    if (ec)
    {
        m_request._get_impl()->_complete(0, std::make_exception_ptr(http_exception(ec.value())));
    }
    else
    {
        std::istream is(&m_request_buf);
        is.imbue(std::locale::classic());
        int len;
        is >> std::hex >> len;
        m_request_buf.consume(CRLF.size());
        m_read += len;
        if (len == 0)
            m_request._get_impl()->_complete(m_read);
        else
            async_read_until_buffersize(len + 2, boost::bind(&connection::handle_chunked_body, this, boost::asio::placeholders::error, len));
    }
}

void connection::handle_chunked_body(const boost::system::error_code& ec, int toWrite)
{
    if (ec)
    {
        m_request._get_impl()->_complete(0, std::make_exception_ptr(http_exception(ec.value())));
    }
    else
    {
        auto writebuf = m_request._get_impl()->outstream().streambuf();
        writebuf.putn_nocopy(buffer_cast<const uint8_t *>(m_request_buf.data()), toWrite).then([=](pplx::task<size_t> writeChunkTask)
        {
            try
            {
                writeChunkTask.get();
            }
            catch (...)
            {
                m_request._get_impl()->_complete(0, std::current_exception());
                return;
            }

            m_request_buf.consume(2 + toWrite);
            async_read_until();
        });
    }
}

void connection::handle_body(const boost::system::error_code& ec)
{
    // read body
    if (ec)
    {
        m_request._get_impl()->_complete(0, std::make_exception_ptr(http_exception(ec.value())));
    }
    else if (m_read < m_read_size)  // there is more to read
    {
        auto writebuf = m_request._get_impl()->outstream().streambuf();
        writebuf.putn_nocopy(boost::asio::buffer_cast<const uint8_t*>(m_request_buf.data()), std::min(m_request_buf.size(), m_read_size - m_read)).then([=](pplx::task<size_t> writtenSizeTask)
        {
            size_t writtenSize = 0;
            try
            {
                writtenSize = writtenSizeTask.get();
            }
            catch (...)
            {
                m_request._get_impl()->_complete(0, std::current_exception());
                return;
            }
            m_read += writtenSize;
            m_request_buf.consume(writtenSize);
            async_read_until_buffersize(std::min(ChunkSize, m_read_size - m_read), boost::bind(&connection::handle_body, this, placeholders::error));
        });
    }
    else  // have read request body
    {
        m_request._get_impl()->_complete(m_read);
    }
}

void connection::async_write(ResponseFuncPtr response_func_ptr, const http_response &response)
{
    if (m_ssl_stream)
    {
        boost::asio::async_write(*m_ssl_stream, m_response_buf, [=] (const boost::system::error_code& ec, std::size_t)
        {
            (this->*response_func_ptr)(response, ec);
        });
    }
    else
    {
        boost::asio::async_write(*m_socket, m_response_buf, [=] (const boost::system::error_code& ec, std::size_t)
        {
            (this->*response_func_ptr)(response, ec);
        });
    }
}

template <typename CompletionCondition, typename Handler>
void connection::async_read(CompletionCondition &&condition, Handler &&read_handler)
{
    if (m_ssl_stream)
    {
        boost::asio::async_read(*m_ssl_stream, m_request_buf, std::forward<CompletionCondition>(condition), std::forward<Handler>(read_handler));
    }
    else
    {
        boost::asio::async_read(*m_socket, m_request_buf, std::forward<CompletionCondition>(condition), std::forward<Handler>(read_handler));
    }
}

void connection::async_read_until()
{
    if (m_ssl_stream)
    {
        boost::asio::async_read_until(*m_ssl_stream, m_request_buf, CRLF, boost::bind(&connection::handle_chunked_header, this, placeholders::error));
    }
    else
    {
        boost::asio::async_read_until(*m_socket, m_request_buf, CRLF, boost::bind(&connection::handle_chunked_header, this, placeholders::error));
    }
}

template <typename ReadHandler>
void connection::async_read_until_buffersize(size_t size, const ReadHandler &handler)
{
    auto bufsize = m_request_buf.size();
    if (bufsize >= size)
    {
        async_read(transfer_at_least(0), handler);
    }
    else
    {
        async_read(transfer_at_least(size - bufsize), handler);
    }
}

void connection::dispatch_request_to_listener()
{
    // locate the listener:
    web::http::experimental::listener::details::http_listener_impl* pListener = nullptr;
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
        do_response(false);
    }
    else
    {
        m_request._set_listener_path(pListener->uri().path());
        do_response(false);

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
        catch(...)
        {
            pListenerLock->unlock();
            m_request._reply_if_not_already(status_codes::InternalError);
        }
    }
    
    if (--m_refs == 0) delete this;
}

void connection::do_response(bool bad_request)
{
    ++m_refs;
    m_request.get_response().then([=](pplx::task<http::http_response> r_task)
    {
        http::http_response response;
        try
        {
            response = r_task.get();
        }
        catch(...)
        {
            response = http::http_response(status_codes::InternalError);
        }

        // before sending response, the full incoming message need to be processed.
        if (bad_request)
        {
            async_process_response(response);
        }
        else
        {
            m_request.content_ready().then([=](pplx::task<http::http_request>)
            {
                async_process_response(response);
            });
        }
    });
}

void connection::async_process_response(http_response response)
{
    m_response_buf.consume(m_response_buf.size()); // clear the buffer
    std::ostream os(&m_response_buf);
    os.imbue(std::locale::classic());

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
    if (!response.body())
    {
        response.headers().add(header_names::content_length,0);
    }

    for(const auto & header : response.headers())
    {
        // check if the responder has requested we close the connection
        if (boost::iequals(header.first, U("connection")))
        {
            if (boost::iequals(header.second, U("close")))
            {
                m_close = true;
            }
        }
        os << header.first << ": " << header.second << CRLF;
    }
    os << CRLF;

    async_write(&connection::handle_headers_written, response);
}

void connection::cancel_sending_response_with_error(const http_response &response, const std::exception_ptr &eptr)
{
    auto * context = static_cast<linux_request_context*>(response._get_server_context());
    context->m_response_completed.set_exception(eptr);
    
    // always terminate the connection since error happens
    finish_request_response();
}

void connection::handle_write_chunked_response(const http_response &response, const boost::system::error_code& ec)
{
    if (ec)
    {
        return handle_response_written(response, ec);
    }
        
    auto readbuf = response._get_impl()->instream().streambuf();
    if (readbuf.is_eof())
    {
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception("Response stream close early!")));
    }
    auto membuf = m_response_buf.prepare(ChunkSize + http::details::chunked_encoding::additional_encoding_space);

    readbuf.getn(buffer_cast<uint8_t *>(membuf) + http::details::chunked_encoding::data_offset, ChunkSize).then([=](pplx::task<size_t> actualSizeTask)
    {
        size_t actualSize = 0;
        try
        {
            actualSize = actualSizeTask.get();
        } catch (...)
        {
            return cancel_sending_response_with_error(response, std::current_exception());
        }
        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buffer_cast<uint8_t *>(membuf), ChunkSize+http::details::chunked_encoding::additional_encoding_space, actualSize);
        m_response_buf.commit(actualSize + http::details::chunked_encoding::additional_encoding_space);
        m_response_buf.consume(offset);
        async_write(actualSize == 0 ? &connection::handle_response_written : &connection::handle_write_chunked_response, response);
    });
}

void connection::handle_write_large_response(const http_response &response, const boost::system::error_code& ec)
{
    if (ec || m_write == m_write_size)
        return handle_response_written(response, ec);

    auto readbuf = response._get_impl()->instream().streambuf();
    if (readbuf.is_eof())
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception("Response stream close early!")));
    size_t readBytes = std::min(ChunkSize, m_write_size - m_write);
    readbuf.getn(buffer_cast<uint8_t *>(m_response_buf.prepare(readBytes)), readBytes).then([=](pplx::task<size_t> actualSizeTask)
    {
        size_t actualSize = 0;
        try
        {
            actualSize = actualSizeTask.get();
        } catch (...)
        {
            return cancel_sending_response_with_error(response, std::current_exception());
        }
        m_write += actualSize;
        m_response_buf.commit(actualSize);
        async_write(&connection::handle_write_large_response, response);
    });
}

void connection::handle_headers_written(const http_response &response, const boost::system::error_code& ec)
{
    if (ec)
    {
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception(ec.value(), "error writing headers")));
    }
    else
    {
        if (m_chunked)
            handle_write_chunked_response(response, ec);
        else
            handle_write_large_response(response, ec);
    }
}

void connection::handle_response_written(const http_response &response, const boost::system::error_code& ec)
{
    auto * context = static_cast<linux_request_context*>(response._get_server_context());
    if (ec)
    {
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception(ec.value(), "error writing response")));
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
        if (m_p_parent->m_connections.empty())
        {
            m_p_parent->m_all_connections_complete.set();
        }
    }
    
    close();
    if (--m_refs == 0) delete this;
}

void hostport_listener::stop()
{
    // halt existing connections
    {
        pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_connections_lock);
        m_acceptor.reset();
        for(auto connection : m_connections)
        {
            connection->close();
        }
    }

    m_all_connections_complete.wait();
}

void hostport_listener::add_listener(const std::string& path, web::http::experimental::listener::details::http_listener_impl* listener)
{
    pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);

    if (m_is_https != (listener->uri().scheme() == U("https")))
        throw std::invalid_argument("Error: http_listener can not simultaneously listen both http and https paths of one host");
    else if (!m_listeners.insert(std::map<std::string,web::http::experimental::listener::details::http_listener_impl*>::value_type(path, listener)).second)
        throw std::invalid_argument("Error: http_listener is already registered for this path");
}

void hostport_listener::remove_listener(const std::string& path, web::http::experimental::listener::details::http_listener_impl*)
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
        while (it != m_listeners.begin())
        {
            --it;
            it->second->stop();
        }
        return pplx::task_from_exception<void>(std::current_exception());
    }

    m_started = true;
    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::stop()
{
    pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_listeners_lock);

    m_started = false;

    for(auto & listener : m_listeners)
    {
        listener.second->stop();
    }

    return pplx::task_from_result();
}

std::pair<std::string,std::string> canonical_parts(const http::uri& uri)
{
    std::ostringstream endpoint;
    endpoint.imbue(std::locale::classic());
    endpoint << uri::decode(uri.host()) << ":" << uri.port();

    auto path = uri::decode(uri.path());

    if (path.size() > 1 && path[path.size()-1] != '/')
    {
        path += "/"; // ensure the end slash is present
    }

    return std::make_pair(endpoint.str(), path);
}

pplx::task<void> http_linux_server::register_listener(details::http_listener_impl* listener)
{
    auto parts = canonical_parts(listener->uri());
    auto hostport = parts.first;
    auto path = parts.second;
    bool is_https = listener->uri().scheme() == U("https");

    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);
        if (m_registered_listeners.find(listener) != m_registered_listeners.end())
        {
            throw std::invalid_argument("listener already registered");
        }

        try
        {
            m_registered_listeners[listener] = utility::details::make_unique<pplx::extensibility::reader_writer_lock_t>();

            auto found_hostport_listener = m_listeners.find(hostport);
            if (found_hostport_listener == m_listeners.end())
            {
                found_hostport_listener = m_listeners.insert(
                    std::make_pair(hostport, utility::details::make_unique<details::hostport_listener>(this, hostport, is_https, listener->configuration()))).first;

                if (m_started)
                {
                    found_hostport_listener->second->start();
                }
            }

            found_hostport_listener->second->add_listener(path, listener);
        }
        catch (...)
        {
            // Future improvement - really this API should entirely be asychronously.
            // the hostport_listener::start() method should be made to return a task
            // throwing the exception.
            m_registered_listeners.erase(listener);
            m_listeners.erase(hostport);
            throw;
        }
    }

    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::unregister_listener(details::http_listener_impl* listener)
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
    details::linux_request_context * p_context = static_cast<details::linux_request_context*>(response._get_server_context());
    return pplx::create_task(p_context->m_response_completed);
}

}}}}
