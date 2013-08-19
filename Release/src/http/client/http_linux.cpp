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

namespace web { namespace http
{
    namespace client
    {
        namespace details
        {
            using boost::asio::ip::tcp;

            class linux_client;
            struct client;

			enum class httpclient_errorcode_context
			{
				none = 0,
				connect,
				writeheader,
				writebody,
				readheader,
				readbody,
				close
			};
            class linux_request_context : public request_context
            {
            public:
                static request_context * create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
                {
                    return new linux_request_context(client, request);
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
						case httpclient_errorcode_context::connect:
							if (ec == boost::system::errc::connection_refused)
								errorcodeValue = make_error_code(std::errc::host_unreachable).value();
							break;
						case httpclient_errorcode_context::readheader:
							if (ec.default_error_condition().value() == boost::system::errc::no_such_file_or_directory) // bug in boost error_code mapping
								errorcodeValue = make_error_code(std::errc::connection_aborted).value();
							break;
						}
					}
					request_context::report_error(errorcodeValue, message);
                }

                std::unique_ptr<tcp::socket> m_socket;
                std::unique_ptr<boost::asio::ssl::stream<tcp::socket>> m_ssl_stream;

                uri m_what;
                size_t m_known_size;
                size_t m_current_size;
                bool m_needChunked;
                bool m_timedout;
                boost::asio::streambuf m_request_buf;
                boost::asio::streambuf m_response_buf;
                std::unique_ptr<boost::asio::deadline_timer> m_timer;

                template <typename socket_type>
                void shutdown_socket(socket_type &socket)
                {
                    boost::system::error_code ignore;
                    socket.shutdown(tcp::socket::shutdown_both, ignore);
                    socket.close();
                }
                
                ~linux_request_context()
                {
                    if (m_timer)
                    {
                        m_timer->cancel();
                        m_timer.reset();
                    }

                    if (m_socket)
                    {
                        shutdown_socket(*m_socket);
                        m_socket.reset();
                    }
                    
                    if (m_ssl_stream)
                    {
                        shutdown_socket(m_ssl_stream->lowest_layer());
                        m_ssl_stream.reset();
                    }
                }

                void cancel(const boost::system::error_code& ec)
                {
                    if (!ec)
                    {
                        m_timedout = true;
                        if (m_ssl_stream)
                        {
                            boost::system::error_code error;
                            m_ssl_stream->lowest_layer().cancel(error);
                                
                            if (error)
                                report_error("Failed to cancel the socket", error);
                        }
                        else
                        {
                            auto sock = m_socket.get();
                            if (sock != nullptr)
                            {
                                sock->cancel();
                            }
                        }
                    }
                }

            private:
                linux_request_context(std::shared_ptr<_http_client_communicator> &client, http_request request) 
                    : request_context(client, request)
                    , m_known_size(0)
                    , m_needChunked(false)
                    , m_timedout(false)
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
                client(boost::asio::io_service& io_service, const http_client_config &config)
                    : m_resolver(io_service)
                    , m_io_service(io_service)
                    , m_config(config) {}

                void send_request(linux_request_context* ctx)
                {
                    auto what = ctx->m_what;
                    auto resource = what.resource().to_string();

                    if (what.scheme() == "https")
                    {
                        boost::asio::ssl::context context(boost::asio::ssl::context::sslv23);
                        context.set_default_verify_paths();
                        ctx->m_ssl_stream.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(m_io_service, context));
                    }
                    else
                        ctx->m_socket.reset(new tcp::socket(m_io_service));

                    if (resource == "") resource = "/";

                    auto method = ctx->m_request.method();
                    
                    // stop injection of headers via method
                    // resource should be ok, since it's been encoded
                    // and host won't resolve
                    if (!validate_method(method))
                    {
                        ctx->report_exception(http_exception("The method string is invalid."));
                        return;
                    }

                    auto host = what.host();
                    std::ostream request_stream(&ctx->m_request_buf);

                    request_stream << method << " " << resource << " " << "HTTP/1.1" << CRLF << "Host: " << host;

                    int port = what.port();
                    if (port == 0)
                        port = (ctx->m_ssl_stream ? 443 : 80);
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
                        request_stream << "Connection: close" << CRLF; // so we can just read to EOF
                    
                    request_stream << CRLF;

                    tcp::resolver::query query(host, utility::conversions::print_string(port));

                    ctx->m_timer.reset(new boost::asio::deadline_timer(m_io_service));
                    auto timeout = m_config.timeout();
                    int secs = static_cast<int>(timeout.count());
                    ctx->m_timer->expires_from_now(boost::posix_time::milliseconds(secs * 1000));
                    ctx->m_timer->async_wait(boost::bind(&linux_request_context::cancel, ctx, boost::asio::placeholders::error));

                    m_resolver.async_resolve(query, boost::bind(&client::handle_resolve, this, boost::asio::placeholders::error, boost::asio::placeholders::iterator, ctx));
                }

            private:
                boost::asio::io_service& m_io_service;
                tcp::resolver m_resolver;
                http_client_config m_config;

                static bool _check_streambuf(linux_request_context * ctx, concurrency::streams::streambuf<uint8_t> rdbuf, const utility::char_t* msg)
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

                void handle_resolve(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, linux_request_context* ctx)
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
                            if(m_config.validate_certificates())
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_peer);
                                ctx->m_ssl_stream->set_verify_callback(boost::asio::ssl::rfc2818_verification(ctx->m_what.host()));
                            }
                            else
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
                            }

                            ctx->m_ssl_stream->lowest_layer().async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                        }
                        else
                            ctx->m_socket->async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
                }

                void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        if (ctx->m_ssl_stream)
                            ctx->m_ssl_stream->async_handshake(boost::asio::ssl::stream_base::client, boost::bind(&client::handle_handshake, this, boost::asio::placeholders::error, ctx));
                        else
                            boost::asio::async_write(*ctx->m_socket, ctx->m_request_buf, boost::bind(&client::handle_write_request, this, boost::asio::placeholders::error, ctx));
                    }
                    else if (endpoints == tcp::resolver::iterator())
                    {
                        ctx->report_error("Failed to connect to any resolved endpoint", ec, httpclient_errorcode_context::connect);
                    }
                    else
                    {
                        boost::system::error_code ignore;
                        auto endpoint = *endpoints;
                        if (ctx->m_ssl_stream)
                        {
                            ctx->m_ssl_stream->lowest_layer().shutdown(tcp::socket::shutdown_both, ignore);
                            ctx->m_ssl_stream->lowest_layer().close();
                            boost::asio::ssl::context context(boost::asio::ssl::context::sslv23);
                            context.set_default_verify_paths();
                            ctx->m_ssl_stream.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(m_io_service, context));

                            // Check to turn off server certificate verification.
                            if(m_config.validate_certificates())
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_peer);
                                ctx->m_ssl_stream->set_verify_callback(boost::asio::ssl::rfc2818_verification(ctx->m_what.host()));
                            }
                            else
                            {
                                ctx->m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
                            }


                            ctx->m_ssl_stream->lowest_layer().async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                        }
                        else
                        {
                            ctx->m_socket->shutdown(tcp::socket::shutdown_both, ignore);
                            ctx->m_socket->close();
                            ctx->m_socket.reset(new tcp::socket(m_io_service));
                            ctx->m_socket->async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                        }
                    }
                }

                void handle_handshake(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (!ec)
                        boost::asio::async_write(*ctx->m_ssl_stream, ctx->m_request_buf, boost::bind(&client::handle_write_request, this, boost::asio::placeholders::error, ctx));
                    else
                        ctx->report_error("Error code in handle_handshake is ", ec);
                }

                void handle_write_chunked_body(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (ec)
                        return handle_write_body(ec, ctx);

                    auto progress = ctx->m_request._get_impl()->_progress_handler();
                    if ( progress )
                    {
                        (*progress)(message_direction::upload, ctx->m_uploaded);
                    }

                    auto readbuf = ctx->_get_readbuffer();
                    uint8_t *buf = boost::asio::buffer_cast<uint8_t *>(ctx->m_request_buf.prepare(m_config.chunksize() + http::details::chunked_encoding::additional_encoding_space));
                    readbuf.getn(buf + http::details::chunked_encoding::data_offset, m_config.chunksize()).then([=](pplx::task<size_t> op)
                    {
                        size_t readSize = 0;
                        try { readSize = op.get(); }
                        catch (...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buf, m_config.chunksize() + http::details::chunked_encoding::additional_encoding_space, readSize);
                        ctx->m_request_buf.commit(readSize + http::details::chunked_encoding::additional_encoding_space);
                        ctx->m_request_buf.consume(offset);
                        ctx->m_current_size += readSize;
                        ctx->m_uploaded += (size64_t)readSize;
                        if (ctx->m_ssl_stream)
                            boost::asio::async_write(*ctx->m_ssl_stream, ctx->m_request_buf,
                                boost::bind(readSize != 0 ? &client::handle_write_chunked_body : &client::handle_write_body, this, boost::asio::placeholders::error, ctx));
                        else
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

                    auto progress = ctx->m_request._get_impl()->_progress_handler();
                    if ( progress )
                    {
                        (*progress)(message_direction::upload, ctx->m_uploaded);
                    }

                    auto readbuf = ctx->_get_readbuffer();
                    size_t readSize = std::min(m_config.chunksize(), ctx->m_known_size - ctx->m_current_size);

                    readbuf.getn(boost::asio::buffer_cast<uint8_t *>(ctx->m_request_buf.prepare(readSize)), readSize).then([=](pplx::task<size_t> op)
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
                        ctx->m_request_buf.commit(actualSize);
                        if (ctx->m_ssl_stream)
                            boost::asio::async_write(*ctx->m_ssl_stream, ctx->m_request_buf,
                                boost::bind(&client::handle_write_large_body, this, boost::asio::placeholders::error, ctx));
                        else
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
                        ctx->report_error("Failed to write request headers", ec, httpclient_errorcode_context::writeheader);
                    }
                }

                void handle_write_body(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        auto progress = ctx->m_request._get_impl()->_progress_handler();
                        if ( progress )
                        {
                            (*progress)(message_direction::upload, ctx->m_uploaded);
                        }
                        
                    // Read until the end of entire headers
                    if (ctx->m_ssl_stream)
                        boost::asio::async_read_until(*ctx->m_ssl_stream, ctx->m_response_buf, CRLF+CRLF,
                            boost::bind(&client::handle_status_line, this, boost::asio::placeholders::error, ctx));
                    else
                        boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF+CRLF,
                            boost::bind(&client::handle_status_line, this, boost::asio::placeholders::error, ctx));
                    }
                    else
                    {
                        ctx->report_error("Failed to write request body", ec, httpclient_errorcode_context::writebody);
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
                            boost::algorithm::trim(name);
                            boost::algorithm::trim(value);

                            ctx->m_response.headers()[name] = value;

                            if (boost::iequals(name, header_names::transfer_encoding))
                            {
                                ctx->m_needChunked = boost::iequals(value, U("chunked"));
                            }
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
                            (*progress)(message_direction::download, 0);
                        }

                        ctx->complete_request(0);
                    }
                    else
                    {
                        ctx->m_current_size = 0;
                        if (!ctx->m_needChunked)
                            async_read_until_buffersize(std::min(ctx->m_known_size, m_config.chunksize()),
                            boost::bind(&client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
                        else
                        {
                            if (ctx->m_ssl_stream)
                                boost::asio::async_read_until(*ctx->m_ssl_stream, ctx->m_response_buf, CRLF,
                                    boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                            else
                                boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF,
                                    boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                        }
                    }
                }

                template <typename ReadHandler>
                void async_read_until_buffersize(size_t size, ReadHandler handler, linux_request_context* ctx)
                {
                    size_t size_to_read = 0;
                    if (ctx->m_ssl_stream)
                    {
                        if (ctx->m_response_buf.size() < size)
                            size_to_read = size - ctx->m_response_buf.size();
                        boost::asio::async_read(*ctx->m_ssl_stream, ctx->m_response_buf, boost::asio::transfer_at_least(size_to_read), handler);
                    }
                    else
                    {
                        if (ctx->m_response_buf.size() < size)
                            size_to_read = size - ctx->m_response_buf.size();
                        boost::asio::async_read(*ctx->m_socket, ctx->m_response_buf, boost::asio::transfer_at_least(size_to_read), handler);
                    }
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
                            ctx->report_error("Invalid chunked response header", boost::system::error_code(), httpclient_errorcode_context::readbody);
                        }
                        else
                            async_read_until_buffersize(octets + CRLF.size(), // +2 for crlf
                            boost::bind(&client::handle_chunk, this, boost::asio::placeholders::error, octets, ctx), ctx);
                    }
                    else
                    {
                        ctx->report_error("Retrieving message chunk header", ec, httpclient_errorcode_context::readbody);
                    }
                }

                void handle_chunk(const boost::system::error_code& ec, int to_read, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        ctx->m_current_size += to_read;
                        ctx->m_downloaded += (size64_t)to_read;
                        auto progress = ctx->m_request._get_impl()->_progress_handler();
                        if ( progress )
                        {
                            (*progress)(message_direction::download, ctx->m_downloaded);
                        }

                        if (to_read == 0)
                        {
                            ctx->m_response_buf.consume(CRLF.size());
                            ctx->_get_writebuffer().sync().then([ctx](pplx::task<void> op)
                            {
                                try { 
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
                            writeBuffer.putn(boost::asio::buffer_cast<const uint8_t *>(ctx->m_response_buf.data()), to_read).then([=](pplx::task<size_t> op)
                            {
                                try { op.wait(); }
                                catch (...)
                                {
                                    ctx->report_exception(std::current_exception());
                                    return;
                                }
                                ctx->m_response_buf.consume(to_read + CRLF.size()); // consume crlf

                                if (ctx->m_ssl_stream)
                                    boost::asio::async_read_until(*ctx->m_ssl_stream, ctx->m_response_buf, CRLF,
                                        boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                                else
                                    boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF,
                                        boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                            });
                        }
                    }
                    else
                    {
                        ctx->report_error("Failed to read chunked response part", ec, httpclient_errorcode_context::readbody);
                    }
                }

                void handle_read_content(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    auto writeBuffer = ctx->_get_writebuffer();

                    if (ec)
                    {
						if (ec == boost::asio::error::eof && ctx->m_known_size == std::numeric_limits<size_t>::max())
							ctx->m_known_size = ctx->m_current_size + ctx->m_response_buf.size();
						else
						{
							ctx->report_error("Failed to read response body", ec, httpclient_errorcode_context::readbody);
							return;
						}
                    }
                    auto progress = ctx->m_request._get_impl()->_progress_handler();
                    if ( progress )
                    {
                        (*progress)(message_direction::download, ctx->m_downloaded);
                    }

                    if (ctx->m_current_size < ctx->m_known_size)
                    {
                        // more data need to be read
                        writeBuffer.putn(boost::asio::buffer_cast<const uint8_t *>(ctx->m_response_buf.data()),
                            std::min(ctx->m_response_buf.size(), ctx->m_known_size - ctx->m_current_size)).then([=](pplx::task<size_t> op)
                        {
                            size_t writtenSize = 0;
                            try 
                            { 
                                writtenSize = op.get(); 
                                ctx->m_downloaded += (size64_t)writtenSize;
                                ctx->m_current_size += writtenSize;
                                ctx->m_response_buf.consume(writtenSize);
                                async_read_until_buffersize(std::min(m_config.chunksize(), ctx->m_known_size - ctx->m_current_size),
                                    boost::bind(&client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
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
                    m_client.reset(new client(crossplat::threadpool::shared_instance().service(), client_config()));
                    return 0;
                }

                void send_request(request_context* request_ctx)
                {
                    auto linux_ctx = static_cast<linux_request_context*>(request_ctx);

                    auto encoded_resource = uri_builder(m_address).append(linux_ctx->m_request.relative_uri()).to_uri();
                    linux_ctx->m_what = encoded_resource;

                    m_client->send_request(linux_ctx);
                }
            };
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
                m_http_client_impl(std::make_shared<details::linux_client>(base_uri, client_config))
            {
            }

            virtual pplx::task<http_response> propagate(http_request request)
            {
                details::request_context * context = details::linux_request_context::create_request_context(m_http_client_impl, request);

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
