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
* http_linux.cpp
*
* HTTP Library: Client-side APIs.
* 
* This file contains the implementation for Linux
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#include "cpprest/http_client_impl.h"
#include <limits>

using boost::asio::ip::tcp;

namespace web { namespace http
{
    namespace client
    {
        namespace details
        {
            enum class httpclient_errorcode_context
            {
                none = 0,
                connect,
                handshake,
                writeheader,
                writebody,
                readheader,
                readbody,
                close
            };

            class linux_request_context : public request_context
            {
            public:
                static std::shared_ptr<request_context> create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
                {
                    return std::make_shared<linux_request_context>(client, request);
                }

                void report_error(const utility::string_t &message, boost::system::error_code ec, httpclient_errorcode_context context = httpclient_errorcode_context::none)
                {
                    // By default, errorcodeValue don't need to converted
                    long errorcodeValue = ec.value();

                    // map timer cancellation to time_out
                    if (ec == boost::system::errc::operation_canceled && m_timedout)
                        errorcodeValue = make_error_code(std::errc::timed_out).value();
                    else
                    {
                        // We need to correct inaccurate ASIO error code base on context information
                        switch (context)
                        {
                        case httpclient_errorcode_context::writeheader:
                            if (ec == boost::system::errc::broken_pipe) 
                                errorcodeValue = make_error_code(std::errc::host_unreachable).value();
                            break;
                        case httpclient_errorcode_context::connect:
                            if (ec == boost::system::errc::connection_refused)
                                errorcodeValue = make_error_code(std::errc::host_unreachable).value();
                            break;
                        case httpclient_errorcode_context::readheader:
                            if (ec.default_error_condition().value() == boost::system::errc::no_such_file_or_directory) // bug in boost error_code mapping
                                errorcodeValue = make_error_code(std::errc::connection_aborted).value();
                            break;
                        default:
                            break;
                        }
                    }
                    request_context::report_error(errorcodeValue, message);
                }

                tcp::socket m_socket;
                std::unique_ptr<boost::asio::ssl::stream<tcp::socket &>> m_ssl_stream;
                size_t m_known_size;
                size_t m_current_size;
                bool m_needChunked;
                bool m_timedout;
                boost::asio::streambuf m_body_buf;
                boost::asio::deadline_timer m_timer;

                ~linux_request_context()
                {
                    m_timer.cancel();
                    boost::system::error_code ignore;
                    m_socket.shutdown(tcp::socket::shutdown_both, ignore);
                    m_socket.close();
                }

                void cancel(const boost::system::error_code& ec)
                {
                    if (!ec)
                    {
                        m_timedout = true;

                        boost::system::error_code error;
                        m_socket.cancel(error);
                        if (error)
                        {
                            report_error("Failed to cancel the socket", error);
                        }
                    }
                }

                linux_request_context(std::shared_ptr<_http_client_communicator> &client, http_request request);

            protected:
                virtual void cleanup()
                {
                    delete this;
                }
            };

            class linux_client : public _http_client_communicator
            {
            public:
 
                linux_client(http::uri address, http_client_config client_config) 
                    : _http_client_communicator(std::move(address), std::move(client_config))
                    , m_resolver(crossplat::threadpool::shared_instance().service())
                    , m_io_service(crossplat::threadpool::shared_instance().service())
				{}

                unsigned long open()
                {
                    return 0;
                }

                void send_request(std::shared_ptr<request_context> request_ctx)
                {
                    auto ctx = std::static_pointer_cast<linux_request_context>(request_ctx);

                    if (m_uri.scheme() == "https")
                    {
                        boost::asio::ssl::context context(boost::asio::ssl::context::sslv23);
                        context.set_default_verify_paths();
                        ctx->m_ssl_stream.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>(ctx->m_socket, context));
                    }

                    auto encoded_resource = uri_builder(m_uri).append(ctx->m_request.relative_uri()).to_uri().resource().to_string();                   
                    if (encoded_resource == "") encoded_resource = "/";

                    const auto &method = ctx->m_request.method();

                    // stop injection of headers via method
                    // resource should be ok, since it's been encoded
                    // and host won't resolve
                    if (!validate_method(method))
                    {
                        ctx->report_exception(http_exception("The method string is invalid."));
                        return;
                    }

                    const auto &host = m_uri.host();
                    std::ostream request_stream(&ctx->m_body_buf);

                    request_stream << method << " " << encoded_resource << " " << "HTTP/1.1" << CRLF << "Host: " << host;

                    int port = m_uri.port();
                    if (m_uri.is_port_default())
                    {
                        port = (ctx->m_ssl_stream ? 443 : 80);
                    }
                    request_stream << ":" << port << CRLF;

                    // Check user specified transfer-encoding
                    std::string transferencoding;
                    if (ctx->m_request.headers().match(header_names::transfer_encoding, transferencoding) && transferencoding == "chunked")
                    {
                        ctx->m_needChunked = true;
                    }

                    bool has_body = true;

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
                            has_body = false;
                            ctx->m_request.headers()[header_names::content_length] = U("0");
                        }
                    }

                    if ( has_body && !_check_streambuf(ctx, ctx->_get_readbuffer(), "Input stream is not open") )
                    {
                        return;
                    }

                    request_stream << flatten_http_headers(ctx->m_request.headers());

                    if (!ctx->m_ssl_stream)
                    {
                        // so we can just read to EOF
                        request_stream << "Connection: close" << CRLF;
                    }

                    request_stream << CRLF;

                    tcp::resolver::query query(host, utility::conversions::print_string(port));

                    const int secs = static_cast<int>(client_config().timeout().count());
                    ctx->m_timer.expires_from_now(boost::posix_time::milliseconds(secs * 1000));
                    ctx->m_timer.async_wait(boost::bind(&linux_request_context::cancel, ctx.get(), boost::asio::placeholders::error));

                    m_resolver.async_resolve(query, boost::bind(&linux_client::handle_resolve, this, boost::asio::placeholders::error, boost::asio::placeholders::iterator, ctx));
                }

                boost::asio::io_service& m_io_service;

            private:
                tcp::resolver m_resolver;

                static bool _check_streambuf(std::shared_ptr<linux_request_context> ctx, concurrency::streams::streambuf<uint8_t> rdbuf, const utility::char_t* msg)
                {
                    if ( !rdbuf.is_open() )
                    {
                        auto eptr = rdbuf.exception();
                        if ( !(eptr == nullptr) )
                        {
                            ctx->report_exception(eptr);
                        }
                        else
                        {
                            ctx->report_exception(http_exception(msg));
                        }
                    }
                    return rdbuf.is_open();
                }

                void handle_resolve(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, std::shared_ptr<linux_request_context> ctx)
                {
                    if (ec)
                    {
                        ctx->report_error("Error resolving address", ec, httpclient_errorcode_context::connect);
                    }
                    else
                    {
                        auto endpoint = *endpoints;
                        if (ctx->m_ssl_stream)
                        {
                            // Check to turn off server certificate verification.
                            if(client_config().validate_certificates())
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_peer);
                                ctx->m_ssl_stream->set_verify_callback(boost::asio::ssl::rfc2818_verification(m_uri.host()));
                            }
                            else
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
                            }
                        }

                        ctx->m_socket.async_connect(endpoint, boost::bind(&linux_client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
                }

                void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, std::shared_ptr<linux_request_context> ctx)
                {
                    if (!ec)
                    {
                        if (ctx->m_ssl_stream)
                        {
                            ctx->m_ssl_stream->async_handshake(boost::asio::ssl::stream_base::client, boost::bind(&linux_client::handle_handshake, this, boost::asio::placeholders::error, ctx));
                        }
                        else
                        {
                            boost::asio::async_write(ctx->m_socket, ctx->m_body_buf, boost::bind(&linux_client::handle_write_request, this, boost::asio::placeholders::error, ctx));
                        }
                    }
                    else if (endpoints == tcp::resolver::iterator())
                    {
                        ctx->report_error("Failed to connect to any resolved endpoint", ec, httpclient_errorcode_context::connect);
                    }
                    else
                    {
                        boost::system::error_code ignore;

                        ctx->m_socket.shutdown(tcp::socket::shutdown_both, ignore);
                        ctx->m_socket.close();
                        ctx->m_socket = tcp::socket(m_io_service);

                        auto endpoint = *endpoints;
                        if (ctx->m_ssl_stream)
                        {
                            boost::asio::ssl::context context(boost::asio::ssl::context::sslv23);
                            context.set_default_verify_paths();
                            ctx->m_ssl_stream.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>(ctx->m_socket, context));

                            // Check to turn off server certificate verification.
                            if(client_config().validate_certificates())
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_peer);
                                ctx->m_ssl_stream->set_verify_callback(boost::asio::ssl::rfc2818_verification(m_uri.host()));
                            }
                            else
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
                            }
                        }

                        ctx->m_socket.async_connect(endpoint, boost::bind(&linux_client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
                }

                void handle_handshake(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    if (!ec)
                        boost::asio::async_write(*ctx->m_ssl_stream, ctx->m_body_buf, boost::bind(&linux_client::handle_write_request, this, boost::asio::placeholders::error, ctx));
                    else
                        ctx->report_error("Error code in handle_handshake is ", ec, httpclient_errorcode_context::handshake);
                }

                void handle_write_chunked_body(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    if (ec)
                        return handle_write_body(ec, ctx);

                    auto progress = ctx->m_request._get_impl()->_progress_handler();
                    if ( progress )
                    {
                        try { (*progress)(message_direction::upload, ctx->m_uploaded); } catch(...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                    }

                    auto readbuf = ctx->_get_readbuffer();
                    uint8_t *buf = boost::asio::buffer_cast<uint8_t *>(ctx->m_body_buf.prepare(client_config().chunksize() + http::details::chunked_encoding::additional_encoding_space));
                    readbuf.getn(buf + http::details::chunked_encoding::data_offset, client_config().chunksize()).then([=](pplx::task<size_t> op)
                    {
                        size_t readSize = 0;
                        try { readSize = op.get(); }
                        catch (...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                        const size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buf, client_config().chunksize() + http::details::chunked_encoding::additional_encoding_space, readSize);
                        ctx->m_body_buf.commit(readSize + http::details::chunked_encoding::additional_encoding_space);
                        ctx->m_body_buf.consume(offset);
                        ctx->m_current_size += readSize;
                        ctx->m_uploaded += (size64_t)readSize;
                        if (ctx->m_ssl_stream)
                            boost::asio::async_write(*ctx->m_ssl_stream, ctx->m_body_buf,
                                boost::bind(readSize != 0 ? &linux_client::handle_write_chunked_body : &linux_client::handle_write_body, this, boost::asio::placeholders::error, ctx));
                        else
                            boost::asio::async_write(ctx->m_socket, ctx->m_body_buf,
                                boost::bind(readSize != 0 ? &linux_client::handle_write_chunked_body : &linux_client::handle_write_body, this, boost::asio::placeholders::error, ctx));
                    });
                }

                void handle_write_large_body(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    if (ec || ctx->m_current_size >= ctx->m_known_size)
                    {
                        return handle_write_body(ec, ctx);
                    }

                    auto progress = ctx->m_request._get_impl()->_progress_handler();
                    if ( progress )
                    {
                        try { (*progress)(message_direction::upload, ctx->m_uploaded); } catch(...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                    }

                    auto readbuf = ctx->_get_readbuffer();
                    const size_t readSize = std::min(client_config().chunksize(), ctx->m_known_size - ctx->m_current_size);

                    readbuf.getn(boost::asio::buffer_cast<uint8_t *>(ctx->m_body_buf.prepare(readSize)), readSize).then([=](pplx::task<size_t> op)
                    {
                        size_t actualSize = 0;
                        try { actualSize = op.get(); }
                        catch (...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                        ctx->m_uploaded += (size64_t)actualSize;
                        ctx->m_current_size += actualSize;
                        ctx->m_body_buf.commit(actualSize);
                        if (ctx->m_ssl_stream)
                            boost::asio::async_write(*ctx->m_ssl_stream, ctx->m_body_buf,
                                boost::bind(&linux_client::handle_write_large_body, this, boost::asio::placeholders::error, ctx));
                        else
                            boost::asio::async_write(ctx->m_socket, ctx->m_body_buf,
                                boost::bind(&linux_client::handle_write_large_body, this, boost::asio::placeholders::error, ctx));
                    });
                }

                void handle_write_request(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
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
                        ctx->report_error("Failed to write request headers", ec, httpclient_errorcode_context::writeheader);
                    }
                }

                void handle_write_body(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    if (!ec)
                    {
                        auto progress = ctx->m_request._get_impl()->_progress_handler();
                        if ( progress )
                        {
                            try { (*progress)(message_direction::upload, ctx->m_uploaded); } catch(...)
                            {
                                ctx->report_exception(std::current_exception());
                                return;
                            }
                        }

                        // Read until the end of entire headers
                        if (ctx->m_ssl_stream)
                            boost::asio::async_read_until(*ctx->m_ssl_stream, ctx->m_body_buf, CRLF+CRLF,
                                boost::bind(&linux_client::handle_status_line, this, boost::asio::placeholders::error, ctx));
                        else
                            boost::asio::async_read_until(ctx->m_socket, ctx->m_body_buf, CRLF+CRLF,
                                boost::bind(&linux_client::handle_status_line, this, boost::asio::placeholders::error,  ctx));
                        }
                    else
                    {
                        ctx->report_error("Failed to write request body", ec, httpclient_errorcode_context::writebody);
                    }
                }

                void handle_status_line(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    if (!ec)
                    {
                        std::istream response_stream(&ctx->m_body_buf);
                        std::string http_version;
                        response_stream >> http_version;
                        status_code status_code;
                        response_stream >> status_code;

                        std::string status_message;
                        std::getline(response_stream, status_message);

                        ctx->m_response.set_status_code(status_code);

                        trim_whitespace(status_message);
                        ctx->m_response.set_reason_phrase(std::move(status_message));

                        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
                        {
                            ctx->report_error("Invalid HTTP status line", ec, httpclient_errorcode_context::readheader);
                            return;
                        }
                        
                        read_headers(ctx);
                    }
                    else
                    {
                        ctx->report_error("Failed to read HTTP status line", ec, httpclient_errorcode_context::readheader);
                    }
                }

                void read_headers(std::shared_ptr<linux_request_context> ctx)
                {
                    ctx->m_needChunked = false;
                    std::istream response_stream(&ctx->m_body_buf);
                    std::string header;
                    while (std::getline(response_stream, header) && header != "\r")
                    {
                        const auto colon = header.find(':');
                        if (colon != std::string::npos)
                        {
                            auto name = header.substr(0, colon);
                            auto value = header.substr(colon+2, header.size()-(colon+3)); // also exclude '\r'
                            boost::algorithm::trim(name);
                            boost::algorithm::trim(value);

                            if (boost::iequals(name, header_names::transfer_encoding))
                            {
                                ctx->m_needChunked = boost::iequals(value, U("chunked"));
                            }

                            ctx->m_response.headers().add(std::move(name), std::move(value));
                        }
                    }
                    ctx->complete_headers();

                    ctx->m_known_size = std::numeric_limits<size_t>::max(); // Without Content-Length header, size should be same as TCP stream - set it size_t max.
                    ctx->m_response.headers().match(header_names::content_length, ctx->m_known_size);

                    // note: need to check for 'chunked' here as well, azure storage sends both
                    // transfer-encoding:chunked and content-length:0 (although HTTP says not to)
                    if (ctx->m_request.method() == U("HEAD") || (!ctx->m_needChunked && ctx->m_known_size == 0))
                    {
                        // we can stop early - no body
                        auto progress = ctx->m_request._get_impl()->_progress_handler();
                        if ( progress )
                        {
                            try { (*progress)(message_direction::download, 0); } catch(...)
                            {
                                ctx->report_exception(std::current_exception());
                                return;
                            }
                        }

                        ctx->complete_request(0);
                    }
                    else
                    {
                        ctx->m_current_size = 0;
                        if (!ctx->m_needChunked)
                        {
                            async_read_until_buffersize(std::min(ctx->m_known_size, client_config().chunksize()),
                                boost::bind(&linux_client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
                        }
                        else
                        {
                            if (ctx->m_ssl_stream)
                                boost::asio::async_read_until(*ctx->m_ssl_stream, ctx->m_body_buf, CRLF,
                                    boost::bind(&linux_client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                            else
                                boost::asio::async_read_until(ctx->m_socket, ctx->m_body_buf, CRLF,
                                    boost::bind(&linux_client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                        }
                    }
                }

                template <typename ReadHandler>
                void async_read_until_buffersize(size_t size, ReadHandler handler, std::shared_ptr<linux_request_context> ctx)
                {
                    size_t size_to_read = 0;
                    if (ctx->m_ssl_stream)
                    {
                        if (ctx->m_body_buf.size() < size)
                        {
                            size_to_read = size - ctx->m_body_buf.size();
                        }
                        boost::asio::async_read(*ctx->m_ssl_stream, ctx->m_body_buf, boost::asio::transfer_at_least(size_to_read), handler);
                    }
                    else
                    {
                        if (ctx->m_body_buf.size() < size)
                        {
                            size_to_read = size - ctx->m_body_buf.size();
                        }
                        boost::asio::async_read(ctx->m_socket, ctx->m_body_buf, boost::asio::transfer_at_least(size_to_read), handler);
                    }
                }

                void handle_chunk_header(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    if (!ec)
                    {
                        std::istream response_stream(&ctx->m_body_buf);
                        std::string line;
                        std::getline(response_stream, line);

                        std::istringstream octetLine(std::move(line));
                        int octets = 0;
                        octetLine >> std::hex >> octets;

                        if (octetLine.fail())
                        {
                            ctx->report_error("Invalid chunked response header", boost::system::error_code(), httpclient_errorcode_context::readbody);
                        }
                        else
                            async_read_until_buffersize(octets + CRLF.size(), // +2 for crlf
                            boost::bind(&linux_client::handle_chunk, this, boost::asio::placeholders::error, octets, ctx), ctx);
                    }
                    else
                    {
                        ctx->report_error("Retrieving message chunk header", ec, httpclient_errorcode_context::readbody);
                    }
                }

                void handle_chunk(const boost::system::error_code& ec, int to_read, std::shared_ptr<linux_request_context> ctx)
                {
                    if (!ec)
                    {
                        ctx->m_current_size += to_read;
                        ctx->m_downloaded += (size64_t)to_read;
                        auto progress = ctx->m_request._get_impl()->_progress_handler();
                        if ( progress )
                        {
                            try { (*progress)(message_direction::download, ctx->m_downloaded); } catch(...)
                            {
                                ctx->report_exception(std::current_exception());
                                return;
                            }
                        }

                        if (to_read == 0)
                        {
                            ctx->m_body_buf.consume(CRLF.size());
                            ctx->_get_writebuffer().sync().then([ctx](pplx::task<void> op)
                            {
                                try 
                                { 
                                    op.wait(); 
                                    ctx->complete_request(ctx->m_current_size);
                                }
                                catch (...)
                                {
                                    ctx->report_exception(std::current_exception());
                                }
                            });
                        }
                        else
                        {
                            auto writeBuffer = ctx->_get_writebuffer();
                            writeBuffer.putn(boost::asio::buffer_cast<const uint8_t *>(ctx->m_body_buf.data()), to_read).then([=](pplx::task<size_t> op)
                            {
                                try { op.wait(); }
                                catch (...)
                                {
                                    ctx->report_exception(std::current_exception());
                                    return;
                                }
                                ctx->m_body_buf.consume(to_read + CRLF.size()); // consume crlf

                                if (ctx->m_ssl_stream)
                                    boost::asio::async_read_until(*ctx->m_ssl_stream, ctx->m_body_buf, CRLF,
                                        boost::bind(&linux_client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                                else
                                    boost::asio::async_read_until(ctx->m_socket, ctx->m_body_buf, CRLF,
                                        boost::bind(&linux_client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                            });
                        }
                    }
                    else
                    {
                        ctx->report_error("Failed to read chunked response part", ec, httpclient_errorcode_context::readbody);
                    }
                }

                void handle_read_content(const boost::system::error_code& ec, std::shared_ptr<linux_request_context> ctx)
                {
                    auto writeBuffer = ctx->_get_writebuffer();

                    if (ec)
                    {
                        if (ec == boost::asio::error::eof && ctx->m_known_size == std::numeric_limits<size_t>::max())
                            ctx->m_known_size = ctx->m_current_size + ctx->m_body_buf.size();
                        else
                        {
                            ctx->report_error("Failed to read response body", ec, httpclient_errorcode_context::readbody);
                            return;
                        }
                    }
                    auto progress = ctx->m_request._get_impl()->_progress_handler();
                    if ( progress )
                    {
                        try { (*progress)(message_direction::download, ctx->m_downloaded); } catch(...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                    }

                    if (ctx->m_current_size < ctx->m_known_size)
                    {
                        // more data need to be read
                        writeBuffer.putn(boost::asio::buffer_cast<const uint8_t *>(ctx->m_body_buf.data()),
                            std::min(ctx->m_body_buf.size(), ctx->m_known_size - ctx->m_current_size)).then([=](pplx::task<size_t> op)
                        {
                            size_t writtenSize = 0;
                            try 
                            { 
                                writtenSize = op.get(); 
                                ctx->m_downloaded += (size64_t)writtenSize;
                                ctx->m_current_size += writtenSize;
                                ctx->m_body_buf.consume(writtenSize);
                                async_read_until_buffersize(std::min(client_config().chunksize(), ctx->m_known_size - ctx->m_current_size),
                                    boost::bind(&linux_client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
                            }
                            catch (...)
                            {
                                ctx->report_exception(std::current_exception());
                                return;
                            }
                        });
                    }
                    else
                    {
                        writeBuffer.sync().then([ctx](pplx::task<void> op)
                        {
                            try 
                            {
                                op.wait(); 
                                ctx->complete_request(ctx->m_current_size);
                            }
                            catch (...)
                            {
                                ctx->report_exception(std::current_exception());
                            }
                        });
                    }
                }
            };

            http_network_handler::http_network_handler(uri base_uri, http_client_config client_config) :
                m_http_client_impl(std::make_shared<details::linux_client>(std::move(base_uri), std::move(client_config)))
            {
            }

            pplx::task<http_response> http_network_handler::propagate(http_request request)
            {
                auto context = details::linux_request_context::create_request_context(m_http_client_impl, request);

                // Use a task to externally signal the final result and completion of the task.
                auto result_task = pplx::create_task(context->m_request_completion);

                // Asynchronously send the response with the HTTP client implementation.
                m_http_client_impl->async_send_request(context);

                return result_task;
            }

            linux_request_context::linux_request_context(std::shared_ptr<_http_client_communicator> &client, http_request request)
                    : request_context(client, request)
                    , m_known_size(0)
                    , m_needChunked(false)
                    , m_timedout(false)
                    , m_current_size(0)
                    , m_timer(crossplat::threadpool::shared_instance().service())
                    , m_socket(std::static_pointer_cast<linux_client>(client)->m_io_service)
                {
                }

}}}} // namespaces
