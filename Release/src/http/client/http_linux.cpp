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

namespace web { namespace http
{
    namespace client
    {
        namespace details
        {
            using boost::asio::ip::tcp;

            class linux_client;
            struct client;

            class linux_request_context : public request_context
            {
            public:
                static request_context * create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
                {
                    return new linux_request_context(client, request);
                }

                void report_error(const utility::string_t &scope, boost::system::error_code ec)
                {
                    if (ec.default_error_condition().value() == boost::system::errc::operation_canceled && m_timedout)
                        request_context::report_error(boost::system::errc::timed_out, scope);
                    else
                        request_context::report_error(ec.default_error_condition().value(), scope);
                }

                std::unique_ptr<tcp::socket> m_socket;
                uri m_what;
                size_t m_known_size;
                size_t m_current_size;
                bool m_needChunked;
                bool m_timedout;
                boost::asio::streambuf m_request_buf;
                boost::asio::streambuf m_response_buf;
                std::unique_ptr<boost::asio::deadline_timer> m_timer;

                ~linux_request_context()
                {
                    if (m_timer)
                    {
                        m_timer->cancel();
                        m_timer.reset();
                    }

                    if (m_socket)
                    {
                        boost::system::error_code ignore;
                        m_socket->shutdown(tcp::socket::shutdown_both, ignore);
                        m_socket->close();
                        m_socket.reset();
                    }
                }

                void cancel(const boost::system::error_code& ec)
                {
                    if (!ec) 
                    {
                        m_timedout = true;
                        auto sock = m_socket.get();
                        if (sock != nullptr)
                        {
                            sock->cancel();
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
                client(boost::asio::io_service& io_service, size_t chunk_size)
                    : m_resolver(io_service)
                    , m_io_service(io_service)
                    , m_chunksize(chunk_size) {}

                void send_request(linux_request_context* ctx, int timeout)
                {
                    auto what = ctx->m_what;
                    ctx->m_socket.reset(new tcp::socket(m_io_service));

                    auto resource = what.resource().to_string();
                    if (resource == "") resource = "/";

                    auto method = ctx->m_request.method();
                    // stop injection of headers via method
                    // resource should be ok, since it's been encoded
                    // and host won't resolve
                    if (std::find(method.begin(), method.end(), '\r') != method.end())
                        throw std::runtime_error("invalid method string");

                    auto host = what.host();
                    std::ostream request_stream(&ctx->m_request_buf);

                    request_stream << method << " "
                        << resource << " "
                        << "HTTP/1.1" << CRLF
                        << "Host: " << host;

                    if (what.port() != 0 && what.port() != 80)
                        request_stream << ":" << what.port();

                    request_stream << CRLF;

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

                    request_stream << "Connection: close" << CRLF; // so we can just read to EOF
                    request_stream << CRLF;

                    tcp::resolver::query query(host, utility::conversions::print_string(what.port() == 0 ? 80 : what.port()));

                    ctx->m_timer.reset(new boost::asio::deadline_timer(m_io_service));
                    ctx->m_timer->expires_from_now(boost::posix_time::milliseconds(timeout));
                    ctx->m_timer->async_wait(boost::bind(&linux_request_context::cancel, ctx, boost::asio::placeholders::error));

                    m_resolver.async_resolve(query, boost::bind(&client::handle_resolve, this, boost::asio::placeholders::error, boost::asio::placeholders::iterator, ctx));
                }

            private:
                boost::asio::io_service& m_io_service;
                tcp::resolver m_resolver;
                size_t m_chunksize;

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
                        ctx->report_error("Error resolving address", ec);
                    }
                    else
                    {
                        auto endpoint = *endpoints;
                        ctx->m_socket->async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
                }

                void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoints, linux_request_context* ctx)
                {
                    if (!ec)
                    {
                        boost::asio::async_write(*ctx->m_socket, ctx->m_request_buf, boost::bind(&client::handle_write_request, this, boost::asio::placeholders::error, ctx));
                    }
                    else if (endpoints == tcp::resolver::iterator())
                    {
                        ctx->report_error("Failed to connect to any resolved endpoint", ec);
                    }
                    else
                    {
                        boost::system::error_code ignore;
                        ctx->m_socket->shutdown(tcp::socket::shutdown_both, ignore);
                        ctx->m_socket->close();
                        ctx->m_socket.reset(new tcp::socket(m_io_service));
                        auto endpoint = *endpoints;
                        ctx->m_socket->async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error, ++endpoints, ctx));
                    }
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
                    uint8_t *buf = boost::asio::buffer_cast<uint8_t *>(ctx->m_request_buf.prepare(m_chunksize + http::details::chunked_encoding::additional_encoding_space));
                    readbuf.getn(buf + http::details::chunked_encoding::data_offset, m_chunksize).then([=](pplx::task<size_t> op)
                    {
                        size_t readSize = 0;
                        try { readSize = op.get(); }
                        catch (...)
                        {
                            ctx->report_exception(std::current_exception());
                            return;
                        }
                        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buf, m_chunksize+http::details::chunked_encoding::additional_encoding_space, readSize);
                        ctx->m_request_buf.commit(readSize + http::details::chunked_encoding::additional_encoding_space);
                        ctx->m_request_buf.consume(offset);
                        ctx->m_current_size += readSize;
                        ctx->m_uploaded += (size64_t)readSize;
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
                    size_t readSize = std::min(m_chunksize, ctx->m_known_size - ctx->m_current_size);

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
                        ctx->report_error("Failed to write request headers", ec);
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
                        boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF+CRLF,
                            boost::bind(&client::handle_status_line, this, boost::asio::placeholders::error, ctx));
                    }
                    else
                    {
                        ctx->report_error("Failed to write request body", ec);
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
                            ctx->report_error("Invalid HTTP status line", ec);
                            return;
                        }

                        read_headers(ctx);
                    }
                    else
                    {
                        ctx->report_error("Failed to read HTTP status line", ec);
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

                    ctx->m_known_size = 0;
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
                            async_read_until_buffersize(std::min(ctx->m_known_size, m_chunksize),
                            boost::bind(&client::handle_read_content, this, boost::asio::placeholders::error, ctx), ctx);
                        else
                            boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF,
                            boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                    }
                }

                template <typename ReadHandler>
                void async_read_until_buffersize(size_t size, ReadHandler handler, linux_request_context* ctx)
                {
                    if (ctx->m_response_buf.size() >= size)
                        boost::asio::async_read(*ctx->m_socket, ctx->m_response_buf, boost::asio::transfer_at_least(0), handler);
                    else
                        boost::asio::async_read(*ctx->m_socket, ctx->m_response_buf, boost::asio::transfer_at_least(size - ctx->m_response_buf.size()), handler);
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
                            ctx->report_error("Invalid chunked response header", boost::system::error_code());
                        }
                        else
                            async_read_until_buffersize(octets + CRLF.size(), // +2 for crlf
                            boost::bind(&client::handle_chunk, this, boost::asio::placeholders::error, octets, ctx), ctx);
                    }
                    else
                    {
                        ctx->report_error("Retrieving message chunk header", ec);
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
                                boost::asio::async_read_until(*ctx->m_socket, ctx->m_response_buf, CRLF,
                                    boost::bind(&client::handle_chunk_header, this, boost::asio::placeholders::error, ctx));
                            });
                        }
                    }
                    else
                    {
                        ctx->report_error("Failed to read chunked response part", ec);
                    }
                }

                void handle_read_content(const boost::system::error_code& ec, linux_request_context* ctx)
                {
                    auto writeBuffer = ctx->_get_writebuffer();

                    if (ec)
                    {
                        ctx->report_error("Failed to read response body", ec);
                        return;
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
                                async_read_until_buffersize(std::min(m_chunksize, ctx->m_known_size - ctx->m_current_size),
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
                    m_client.reset(new client(crossplat::threadpool::shared_instance().service(), client_config().chunksize()));
                    return 0;
                }

                void send_request(request_context* request_ctx)
                {
                    auto linux_ctx = static_cast<linux_request_context*>(request_ctx);

                    auto encoded_resource = uri_builder(m_address).append(linux_ctx->m_request.relative_uri()).to_uri();

                    linux_ctx->m_what = encoded_resource;

                    auto& config = client_config();

                    auto timeout = config.timeout();
                    int secs = static_cast<int>(timeout.count());

                    m_client->send_request(linux_ctx, secs * 1000);
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
