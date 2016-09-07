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
* HTTP Library: Client-side APIs.
*
* This file contains a cross platform implementation based on Boost.ASIO.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/algorithm/string.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(BOOST_NO_CXX11_SMART_PTR)
#error "Cpp rest SDK requires c++11 smart pointer support from boost"
#endif

#include "cpprest/details/http_client_impl.h"
#include "cpprest/details/x509_cert_utilities.h"
#include <unordered_set>

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

class asio_connection_pool;
class asio_connection
{
    friend class asio_connection_pool;
    friend class asio_client;
public:
    asio_connection(boost::asio::io_service& io_service, bool start_with_ssl, const std::function<void(boost::asio::ssl::context&)>& ssl_context_callback) :
    m_socket(io_service),
    m_ssl_context_callback(ssl_context_callback),
    m_pool_timer(io_service),
    m_is_reused(false),
    m_keep_alive(true)
    {
        if (start_with_ssl)
        {
            upgrade_to_ssl();
        }
    }

    ~asio_connection()
    {
        close();
    }

    // This simply instantiates the internal state to support ssl. It does not perform the handshake.
    void upgrade_to_ssl()
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        assert(!is_ssl());
        boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23);
        ssl_context.set_default_verify_paths();
        ssl_context.set_options(boost::asio::ssl::context::default_workarounds);
        m_ssl_context_callback(ssl_context);
        m_ssl_stream = utility::details::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>>(m_socket, ssl_context);
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);

        // Ensures closed connections owned by request_context will not be put to pool when they are released.
        m_keep_alive = false;

        boost::system::error_code error;
        m_socket.shutdown(tcp::socket::shutdown_both, error);
        m_socket.close(error);
    }

    boost::system::error_code cancel()
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        boost::system::error_code error;
        m_socket.cancel(error);
        return error;
    }

    void cancel_pool_timer()
    {
        m_pool_timer.cancel();
    }

    bool is_reused() const { return m_is_reused; }
    void set_keep_alive(bool keep_alive) { m_keep_alive = keep_alive; }
    bool keep_alive() const { return m_keep_alive; }
    bool is_ssl() const { return m_ssl_stream ? true : false; }

    template <typename Iterator, typename Handler>
    void async_connect(const Iterator &begin, const Handler &handler)
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        m_socket.async_connect(begin, handler);
    }

    template <typename HandshakeHandler, typename CertificateHandler>
    void async_handshake(boost::asio::ssl::stream_base::handshake_type type,
                         const http_client_config &config,
                         const utility::string_t &host_name,
                         const HandshakeHandler &handshake_handler,
                         const CertificateHandler &cert_handler)
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        assert(is_ssl());

        // Check to turn on/off server certificate verification.
        if (config.validate_certificates())
        {
            m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_peer);
            m_ssl_stream->set_verify_callback(cert_handler);
        }
        else
        {
            m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
        }

        // Check to set host name for Server Name Indication (SNI)
        if (config.is_tlsext_sni_enabled())
        {
            SSL_set_tlsext_host_name(m_ssl_stream->native_handle(), const_cast<char *>(host_name.data()));
        }

        m_ssl_stream->async_handshake(type, handshake_handler);
    }

    template <typename ConstBufferSequence, typename Handler>
    void async_write(ConstBufferSequence &buffer, const Handler &writeHandler)
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (m_ssl_stream)
        {
            boost::asio::async_write(*m_ssl_stream, buffer, writeHandler);
        }
        else
        {
            boost::asio::async_write(m_socket, buffer, writeHandler);
        }
    }

    template <typename MutableBufferSequence, typename CompletionCondition, typename Handler>
    void async_read(MutableBufferSequence &buffer, const CompletionCondition &condition, const Handler &readHandler)
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (m_ssl_stream)
        {
            boost::asio::async_read(*m_ssl_stream, buffer, condition, readHandler);
        }
        else
        {
            boost::asio::async_read(m_socket, buffer, condition, readHandler);
        }
    }

    template <typename Handler>
    void async_read_until(boost::asio::streambuf &buffer, const std::string &delim, const Handler &readHandler)
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (m_ssl_stream)
        {
            boost::asio::async_read_until(*m_ssl_stream, buffer, delim, readHandler);
        }
        else
        {
            boost::asio::async_read_until(m_socket, buffer, delim, readHandler);
        }
    }

private:
    template <typename TimeoutHandler>
    void start_pool_timer(int timeout_secs, const TimeoutHandler &handler)
    {
        m_pool_timer.expires_from_now(boost::posix_time::milliseconds(timeout_secs * 1000));
        m_pool_timer.async_wait(handler);
    }

    void start_reuse()
    {
        cancel_pool_timer();
        m_is_reused = true;
    }

    void handle_pool_timer(const boost::system::error_code& ec);

    // Guards concurrent access to socket/ssl::stream. This is necessary
    // because timeouts and cancellation can touch the socket at the same time
    // as normal message processing.
    std::mutex m_socket_lock;
    tcp::socket m_socket;
    std::unique_ptr<boost::asio::ssl::stream<tcp::socket &> > m_ssl_stream;

    std::function<void(boost::asio::ssl::context&)> m_ssl_context_callback;

    boost::asio::deadline_timer m_pool_timer;
    bool m_is_reused;
    bool m_keep_alive;
};

class asio_connection_pool
{
public:

    asio_connection_pool(boost::asio::io_service& io_service, bool start_with_ssl, const std::chrono::seconds &idle_timeout, const std::function<void(boost::asio::ssl::context&)> &ssl_context_callback) :
    m_io_service(io_service),
    m_timeout_secs(static_cast<int>(idle_timeout.count())),
    m_start_with_ssl(start_with_ssl),
    m_ssl_context_callback(ssl_context_callback)
    {}

    ~asio_connection_pool()
    {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        // Cancel the pool timer for all connections.
        for (auto& connection : m_connections)
        {
            connection->cancel_pool_timer();
        }
    }

    void release(const std::shared_ptr<asio_connection> &connection)
    {
        if (connection->keep_alive() && (m_timeout_secs > 0))
        {
            connection->cancel();

            std::lock_guard<std::mutex> lock(m_connections_mutex);
            // This will destroy and remove the connection from pool after the set timeout.
            // We use 'this' because async calls to timer handler only occur while the pool exists.
            connection->start_pool_timer(m_timeout_secs, boost::bind(&asio_connection_pool::handle_pool_timer, this, boost::asio::placeholders::error, connection));
            m_connections.push_back(connection);
        }
        // Otherwise connection is not put to the pool and it will go out of scope.
    }

    std::shared_ptr<asio_connection> obtain()
    {
        std::unique_lock<std::mutex> lock(m_connections_mutex);
        if (m_connections.empty())
        {
            lock.unlock();

            // No connections in pool => create a new connection instance.
            return std::make_shared<asio_connection>(m_io_service, m_start_with_ssl, m_ssl_context_callback);
        }
        else
        {
            // Reuse connection from pool.
            auto connection = m_connections.back();
            m_connections.pop_back();
            lock.unlock();

            connection->start_reuse();
            return connection;
        }
    }

private:

    // Using weak_ptr here ensures bind() to this handler will not prevent the connection object from going out of scope.
    void handle_pool_timer(const boost::system::error_code& ec, const std::weak_ptr<asio_connection> &connection)
    {
        if (!ec)
        {
            auto connection_shared = connection.lock();
            if (connection_shared)
            {
                std::lock_guard<std::mutex> lock(m_connections_mutex);
                const auto &iter = std::find(m_connections.begin(), m_connections.end(), connection_shared);
                if (iter != m_connections.end())
                {
                    m_connections.erase(iter);
                }
            }
        }
    }

    boost::asio::io_service& m_io_service;
    const int m_timeout_secs;
    const bool m_start_with_ssl;
    const std::function<void(boost::asio::ssl::context&)>& m_ssl_context_callback;
    std::vector<std::shared_ptr<asio_connection> > m_connections;
    std::mutex m_connections_mutex;
};



class asio_client : public _http_client_communicator, public std::enable_shared_from_this<asio_client>
{
public:
    asio_client(http::uri address, http_client_config client_config)
    : _http_client_communicator(std::move(address), std::move(client_config))
    , m_pool(crossplat::threadpool::shared_instance().service(),
             base_uri().scheme() == "https" && !_http_client_communicator::client_config().proxy().is_specified(),
             std::chrono::seconds(30), // Unused sockets are kept in pool for 30 seconds.
             this->client_config().get_ssl_context_callback()) 
    , m_resolver(crossplat::threadpool::shared_instance().service())
    {}

    void send_request(const std::shared_ptr<request_context> &request_ctx) override;

    unsigned long open() override { return 0; }

    asio_connection_pool m_pool;
    tcp::resolver m_resolver;
};

class asio_context : public request_context, public std::enable_shared_from_this<asio_context>
{
    friend class asio_client;
public:
    asio_context(const std::shared_ptr<_http_client_communicator> &client,
                 http_request &request,
                 const std::shared_ptr<asio_connection> &connection)
    : request_context(client, request)
    , m_content_length(0)
    , m_needChunked(false)
    , m_timer(client->client_config().timeout<std::chrono::microseconds>())
    , m_connection(connection)
#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__))
    , m_openssl_failed(false)
#endif
    {}

    virtual ~asio_context()
    {
        m_timer.stop();
        // Release connection back to the pool. If connection was not closed, it will be put to the pool for reuse.
        std::static_pointer_cast<asio_client>(m_http_client)->m_pool.release(m_connection);
    }

    static std::shared_ptr<request_context> create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
    {
        auto client_cast(std::static_pointer_cast<asio_client>(client));
        auto connection(client_cast->m_pool.obtain());
        auto ctx = std::make_shared<asio_context>(client, request, connection);
        ctx->m_timer.set_ctx(std::weak_ptr<asio_context>(ctx));
        return ctx;
    }
    
    class ssl_proxy_tunnel : public std::enable_shared_from_this<ssl_proxy_tunnel>
    {
    public:
        ssl_proxy_tunnel(std::shared_ptr<asio_context> context, std::function<void(std::shared_ptr<asio_context>)> ssl_tunnel_established)
            : m_ssl_tunnel_established(ssl_tunnel_established), m_context(context)
        {}
        
        void start_proxy_connect()
        {
            auto proxy = m_context->m_http_client->client_config().proxy();
            auto proxy_uri = proxy.address();

            utility::string_t proxy_host = proxy_uri.host();
            int proxy_port = proxy_uri.port() == -1 ? 8080 : proxy_uri.port();

            const auto &base_uri = m_context->m_http_client->base_uri();
            const auto &host = base_uri.host();

            std::ostream request_stream(&m_request);
            request_stream.imbue(std::locale::classic());

            request_stream << "CONNECT " << host << ":" << 443 << " HTTP/1.1" << CRLF;
            request_stream << "Host: " << host << ":" << 443 << CRLF;
            request_stream << "Proxy-Connection: Keep-Alive" << CRLF;

            if(m_context->m_http_client->client_config().proxy().credentials().is_set())
            {
                request_stream << m_context->generate_basic_proxy_auth_header() << CRLF;
            }

            request_stream << CRLF;

            m_context->m_timer.start();

            tcp::resolver::query query(proxy_host, utility::conversions::print_string(proxy_port, std::locale::classic()));

            auto client = std::static_pointer_cast<asio_client>(m_context->m_http_client);
            client->m_resolver.async_resolve(query, boost::bind(&ssl_proxy_tunnel::handle_resolve, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::iterator));
        }

    private: 
        void handle_resolve(const boost::system::error_code& ec, tcp::resolver::iterator endpoints)
        {
            if (ec)
            {
                m_context->report_error("Error resolving proxy address", ec, httpclient_errorcode_context::connect);
            }
            else
            {
                m_context->m_timer.reset();
                auto endpoint = *endpoints;
                m_context->m_connection->async_connect(endpoint, boost::bind(&ssl_proxy_tunnel::handle_tcp_connect, shared_from_this(), boost::asio::placeholders::error, ++endpoints));
            }
        }

        void handle_tcp_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoints)
        {
            if (!ec)
            {
                m_context->m_timer.reset();
                m_context->m_connection->async_write(m_request, boost::bind(&ssl_proxy_tunnel::handle_write_request, shared_from_this(), boost::asio::placeholders::error));
            }
            else if (endpoints == tcp::resolver::iterator())
            {
                m_context->report_error("Failed to connect to any resolved proxy endpoint", ec, httpclient_errorcode_context::connect);
            }
            else
            {
                m_context->m_timer.reset();
                //// Replace the connection. This causes old connection object to go out of scope.
                auto client = std::static_pointer_cast<asio_client>(m_context->m_http_client);
                m_context->m_connection = client->m_pool.obtain();

                auto endpoint = *endpoints;
                m_context->m_connection->async_connect(endpoint, boost::bind(&ssl_proxy_tunnel::handle_tcp_connect, shared_from_this(), boost::asio::placeholders::error, ++endpoints));
            }

        }

        void handle_write_request(const boost::system::error_code& err)
        {
            if (!err)
            {
                m_context->m_timer.reset();
                m_context->m_connection->async_read_until(m_response, CRLF + CRLF, boost::bind(&ssl_proxy_tunnel::handle_status_line, shared_from_this(), boost::asio::placeholders::error));
            }
            else
            {
                m_context->report_error("Failed to send connect request to proxy.", err, httpclient_errorcode_context::writebody);
            }
	    }
    
        void handle_status_line(const boost::system::error_code& ec)
        {
            if (!ec)
            {
                m_context->m_timer.reset();
                std::istream response_stream(&m_response);
                response_stream.imbue(std::locale::classic());
                std::string http_version;
                response_stream >> http_version;
                status_code status_code;
                response_stream >> status_code;
                
                if (!response_stream || http_version.substr(0, 5) != "HTTP/")
                {
                    m_context->report_error("Invalid HTTP status line during proxy connection", ec, httpclient_errorcode_context::readheader);
                    return;
                }
                
                if (status_code != 200)
                {
                    utility::stringstream_t err_ss;
                    err_ss << U("Expected a 200 response from proxy, received: ") << status_code;
                    m_context->report_error(err_ss.str(), ec, httpclient_errorcode_context::readheader);
                    return;
                }
                
                m_context->m_connection->upgrade_to_ssl();
                
                m_ssl_tunnel_established(m_context);
            }
            else
            {
                // These errors tell if connection was closed.
                const bool socket_was_closed((boost::asio::error::eof == ec)
                    || (boost::asio::error::connection_reset == ec)
                    || (boost::asio::error::connection_aborted == ec));
                if (socket_was_closed && m_context->m_connection->is_reused())
                {
                    // Failed to write to socket because connection was already closed while it was in the pool.
                    // close() here ensures socket is closed in a robust way and prevents the connection from being put to the pool again.
                    m_context->m_connection->close();
            
                    // Create a new context and copy the request object, completion event and
                    // cancellation registration to maintain the old state.
                    // This also obtains a new connection from pool.
                    auto new_ctx = m_context->create_request_context(m_context->m_http_client, m_context->m_request);
                    new_ctx->m_request_completion = m_context->m_request_completion;
                    new_ctx->m_cancellationRegistration = m_context->m_cancellationRegistration;
            
                    auto client = std::static_pointer_cast<asio_client>(m_context->m_http_client);
                    // Resend the request using the new context.
                    client->send_request(new_ctx);
                }
                else
                {
                    m_context->report_error("Failed to read HTTP status line from proxy", ec, httpclient_errorcode_context::readheader);
                }
            }
        }
    
        std::function<void(std::shared_ptr<asio_context>)> m_ssl_tunnel_established;
        std::shared_ptr<asio_context> m_context;
    
        boost::asio::streambuf m_request;
        boost::asio::streambuf m_response;
    };
    
    
    enum class http_proxy_type
    {
        none,
        http,
        ssl_tunnel
    };

    void start_request()
    {
        if (m_request._cancellation_token().is_canceled())
        {
            request_context::report_error(make_error_code(std::errc::operation_canceled).value(), "Request canceled by user.");
            return;
        }
        
        http_proxy_type proxy_type = http_proxy_type::none;
        utility::string_t proxy_host;
        int proxy_port = -1;
        
        // There is no support for auto-detection of proxies on non-windows platforms, it must be specified explicitly from the client code.
        if (m_http_client->client_config().proxy().is_specified())
        {
            proxy_type = m_http_client->base_uri().scheme() == U("https") ? http_proxy_type::ssl_tunnel : http_proxy_type::http;
            auto proxy = m_http_client->client_config().proxy();
            auto proxy_uri = proxy.address();
            proxy_port = proxy_uri.port() == -1 ? 8080 : proxy_uri.port();
            proxy_host = proxy_uri.host();
        }
        
        auto start_http_request_flow = [proxy_type, proxy_host, proxy_port](std::shared_ptr<asio_context> ctx)
        {
            if (ctx->m_request._cancellation_token().is_canceled())
            {
                ctx->request_context::report_error(make_error_code(std::errc::operation_canceled).value(), "Request canceled by user.");
                return;
            }
                
            const auto &base_uri = ctx->m_http_client->base_uri();
            const auto full_uri = uri_builder(base_uri).append(ctx->m_request.relative_uri()).to_uri();
                
            // For a normal http proxy, we need to specify the full request uri, otherwise just specify the resource
            auto encoded_resource = proxy_type == http_proxy_type::http ? full_uri.to_string() : full_uri.resource().to_string();
                
            if (encoded_resource == "")
            {
                encoded_resource = "/";
            }
                
            const auto &method = ctx->m_request.method();
                
            // stop injection of headers via method
            // resource should be ok, since it's been encoded
            // and host won't resolve
            if (!::web::http::details::validate_method(method))
            {
                ctx->report_exception(http_exception("The method string is invalid."));
                return;
            }
                
            std::ostream request_stream(&ctx->m_body_buf);
            request_stream.imbue(std::locale::classic());
            const auto &host = base_uri.host();
                
            request_stream << method << " " << encoded_resource << " " << "HTTP/1.1" << CRLF;
                
            int port = base_uri.port();
                
            if (base_uri.is_port_default())
            {
                port = (ctx->m_connection->is_ssl() ? 443 : 80);
            }
                
            // Add the Host header if user has not specified it explicitly
            if (!ctx->m_request.headers().has(header_names::host))
            {
                request_stream << "Host: " << host << ":" << port << CRLF;
            }
                
            // Extra request headers are constructed here.
            utility::string_t extra_headers;
                
            // Add header for basic proxy authentication
            if (proxy_type == http_proxy_type::http && ctx->m_http_client->client_config().proxy().credentials().is_set())
            {
                extra_headers.append(ctx->generate_basic_proxy_auth_header());
            }
                
            // Check user specified transfer-encoding.
            std::string transferencoding;
            if (ctx->m_request.headers().match(header_names::transfer_encoding, transferencoding) && transferencoding == "chunked")
            {
                ctx->m_needChunked = true;
            }
            else if (!ctx->m_request.headers().match(header_names::content_length, ctx->m_content_length))
            {
                // Stream without content length is the signal of requiring transfer encoding chunked.
                if (ctx->m_request.body())
                {
                    ctx->m_needChunked = true;
                    extra_headers.append(header_names::transfer_encoding);
                    extra_headers.append(":chunked" + CRLF);
                }
            }
                
            if (proxy_type == http_proxy_type::http)
            {
                extra_headers.append(header_names::cache_control);
                extra_headers.append(": no-store, no-cache" + CRLF);
                extra_headers.append(header_names::pragma);
                extra_headers.append(": no-cache" + CRLF);
            }
                
            request_stream << flatten_http_headers(ctx->m_request.headers());
            request_stream << extra_headers;
            // Enforce HTTP connection keep alive (even for the old HTTP/1.0 protocol).
            request_stream << "Connection: Keep-Alive" << CRLF << CRLF;
                
            // Start connection timeout timer.
            if (!ctx->m_timer.has_started())
            {
                ctx->m_timer.start();
            }
                
            if (ctx->m_connection->is_reused() || proxy_type == http_proxy_type::ssl_tunnel)
            {
                // If socket is a reused connection or we're connected via an ssl-tunneling proxy, try to write the request directly. In both cases we have already established a tcp connection.
                ctx->write_request();
            }
            else
            {
                // If the connection is new (unresolved and unconnected socket), then start async
                // call to resolve first, leading eventually to request write.
                    
                // For normal http proxies, we want to connect directly to the proxy server. It will relay our request.
                auto tcp_host = proxy_type == http_proxy_type::http ? proxy_host : host;
                auto tcp_port = proxy_type == http_proxy_type::http ? proxy_port : port;
                    
                tcp::resolver::query query(tcp_host, utility::conversions::print_string(tcp_port, std::locale::classic()));
                auto client = std::static_pointer_cast<asio_client>(ctx->m_http_client);
                client->m_resolver.async_resolve(query, boost::bind(&asio_context::handle_resolve, ctx, boost::asio::placeholders::error, boost::asio::placeholders::iterator));
            }
        
                // Register for notification on cancellation to abort this request.
            if (ctx->m_request._cancellation_token() != pplx::cancellation_token::none())
            {
                // weak_ptr prevents lambda from taking shared ownership of the context.
                // Otherwise context replacement in the handle_status_line() would leak the objects.
                std::weak_ptr<asio_context> ctx_weak(ctx);
                ctx->m_cancellationRegistration = ctx->m_request._cancellation_token().register_callback([ctx_weak]()
                {
                    if (auto ctx_lock = ctx_weak.lock())
                    {
                        // Shut down transmissions, close the socket and prevent connection from being pooled.
                        ctx_lock->m_connection->close();
                    }
                });
            }
        };
        
        if (proxy_type == http_proxy_type::ssl_tunnel)
        {
            // The ssl_tunnel_proxy keeps the context alive and then calls back once the ssl tunnel is established via 'start_http_request_flow'
            std::shared_ptr<ssl_proxy_tunnel> ssl_tunnel = std::make_shared<ssl_proxy_tunnel>(shared_from_this(), start_http_request_flow);
            ssl_tunnel->start_proxy_connect();
        }
        else
        {
            start_http_request_flow(shared_from_this());
        }
    }

    template<typename _ExceptionType>
    void report_exception(const _ExceptionType &e)
    {
        report_exception(std::make_exception_ptr(e));
    }

    void report_exception(std::exception_ptr exceptionPtr) override
    {
        // Don't recycle connections that had an error into the connection pool.
        m_connection->close();
        request_context::report_exception(exceptionPtr);
    }

private:

    utility::string_t generate_basic_proxy_auth_header()
    {
        utility::string_t header;
        
        header.append(header_names::proxy_authorization);
        header.append(": Basic ");
        
        auto credential_str = web::details::plaintext_string(new ::utility::string_t(m_http_client->client_config().proxy().credentials().username()));
        credential_str->append(":");
        credential_str->append(*m_http_client->client_config().proxy().credentials().decrypt());
        
        std::vector<unsigned char> credentials_buffer(credential_str->begin(), credential_str->end());
        
        header.append(utility::conversions::to_base64(credentials_buffer));
        header.append(CRLF);
        return header;
    }

    void report_error(const utility::string_t &message, const boost::system::error_code &ec, httpclient_errorcode_context context = httpclient_errorcode_context::none)
    {
        // By default, errorcodeValue don't need to converted
        long errorcodeValue = ec.value();

        // map timer cancellation to time_out
        if (ec == boost::system::errc::operation_canceled && m_timer.has_timedout())
        {
            errorcodeValue = make_error_code(std::errc::timed_out).value();
        }
        else
        {
            // We need to correct inaccurate ASIO error code base on context information
            switch (context)
            {
                case httpclient_errorcode_context::writeheader:
                    if (ec == boost::system::errc::broken_pipe)
                    {
                        errorcodeValue = make_error_code(std::errc::host_unreachable).value();
                    }
                    break;
                case httpclient_errorcode_context::connect:
                    if (ec == boost::system::errc::connection_refused)
                    {
                        errorcodeValue = make_error_code(std::errc::host_unreachable).value();
                    }
                    break;
                case httpclient_errorcode_context::readheader:
                    if (ec.default_error_condition().value() == boost::system::errc::no_such_file_or_directory) // bug in boost error_code mapping
                    {
                        errorcodeValue = make_error_code(std::errc::connection_aborted).value();
                    }
                    break;
                default:
                    break;
            }
        }
        request_context::report_error(errorcodeValue, message);
    }

    void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoints)
    {
       
        m_timer.reset();
        if (!ec)
        {
            write_request();
        }
        else if (ec.value() == boost::system::errc::operation_canceled)
        {
            request_context::report_error(ec.value(), "Request canceled by user.");
        }
        else if (endpoints == tcp::resolver::iterator())
        {
            report_error("Failed to connect to any resolved endpoint", ec, httpclient_errorcode_context::connect);
        }
        else
        {
            // Replace the connection. This causes old connection object to go out of scope.
            auto client = std::static_pointer_cast<asio_client>(m_http_client);
            m_connection = client->m_pool.obtain();

            auto endpoint = *endpoints;
            m_connection->async_connect(endpoint, boost::bind(&asio_context::handle_connect, shared_from_this(), boost::asio::placeholders::error, ++endpoints));
        }
    }

    void handle_resolve(const boost::system::error_code& ec, tcp::resolver::iterator endpoints)
    {
        if (ec)
        {
            report_error("Error resolving address", ec, httpclient_errorcode_context::connect);
        }
        else
        {
            m_timer.reset();
            auto endpoint = *endpoints;
            m_connection->async_connect(endpoint, boost::bind(&asio_context::handle_connect, shared_from_this(), boost::asio::placeholders::error, ++endpoints));
        }
    }

    void write_request()
    {
        // Only perform handshake if a TLS connection and not being reused.
        if (m_connection->is_ssl() && !m_connection->is_reused())
        {
            const auto weakCtx = std::weak_ptr<asio_context>(shared_from_this());
            m_connection->async_handshake(boost::asio::ssl::stream_base::client,
                                          m_http_client->client_config(),
                                          m_http_client->base_uri().host(),
                                          boost::bind(&asio_context::handle_handshake, shared_from_this(), boost::asio::placeholders::error),

                                          // Use a weak_ptr since the verify_callback is stored until the connection is destroyed.
                                          // This avoids creating a circular reference since we pool connection objects.
                                          [weakCtx](bool preverified, boost::asio::ssl::verify_context &verify_context)
                                          {
                                              auto this_request = weakCtx.lock();
                                              if(this_request)
                                              {
                                                  return this_request->handle_cert_verification(preverified, verify_context);
                                              }
                                              return false;
                                          });
        }
        else
        {
            m_connection->async_write(m_body_buf, boost::bind(&asio_context::handle_write_headers, shared_from_this(), boost::asio::placeholders::error));
        }
    }

    void handle_handshake(const boost::system::error_code& ec)
    {
        if (!ec)
        {
            m_connection->async_write(m_body_buf, boost::bind(&asio_context::handle_write_headers, shared_from_this(), boost::asio::placeholders::error));
        }
        else
        {
            report_error("Error in SSL handshake", ec, httpclient_errorcode_context::handshake);
        }
    }

    bool handle_cert_verification(bool preverified, boost::asio::ssl::verify_context &verifyCtx)
    {
        // OpenSSL calls the verification callback once per certificate in the chain,
        // starting with the root CA certificate. The 'leaf', non-Certificate Authority (CA)
        // certificate, i.e. actual server certificate is at the '0' position in the
        // certificate chain, the rest are optional intermediate certificates, followed
        // finally by the root CA self signed certificate.

        const auto &host = m_http_client->base_uri().host();
#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__))
        // On OS X, iOS, and Android, OpenSSL doesn't have access to where the OS
        // stores keychains. If OpenSSL fails we will doing verification at the
        // end using the whole certificate chain so wait until the 'leaf' cert.
        // For now return true so OpenSSL continues down the certificate chain.
        if(!preverified)
        {
            m_openssl_failed = true;
        }
        if(m_openssl_failed)
        {
            return verify_cert_chain_platform_specific(verifyCtx, host);
        }
#endif

        boost::asio::ssl::rfc2818_verification rfc2818(host);
        return rfc2818(preverified, verifyCtx);
    }

    void handle_write_headers(const boost::system::error_code& ec)
    {
        if(ec)
        {
            report_error("Failed to write request headers", ec, httpclient_errorcode_context::writeheader);
        }
        else
        {
            if (m_needChunked)
            {
                handle_write_chunked_body(ec);
            }
            else
            {
                handle_write_large_body(ec);
            }
        }
    }


    void handle_write_chunked_body(const boost::system::error_code& ec)
    {
        if (ec)
        {
            // Reuse error handling.
            return handle_write_body(ec);
        }

        m_timer.reset();
        const auto &progress = m_request._get_impl()->_progress_handler();
        if (progress)
        {
            try
            {
                (*progress)(message_direction::upload, m_uploaded);
            }
            catch(...)
            {
                report_exception(std::current_exception());
                return;
            }
        }

        const auto & chunkSize = m_http_client->client_config().chunksize();
        auto readbuf = _get_readbuffer();
        uint8_t *buf = boost::asio::buffer_cast<uint8_t *>(m_body_buf.prepare(chunkSize + http::details::chunked_encoding::additional_encoding_space));
        const auto this_request = shared_from_this();
        readbuf.getn(buf + http::details::chunked_encoding::data_offset, chunkSize).then([this_request, buf, chunkSize](pplx::task<size_t> op)
        {
            size_t readSize = 0;
            try
            {
                readSize = op.get();
            }
            catch (...)
            {
                this_request->report_exception(std::current_exception());
                return;
            }

            const size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buf, chunkSize + http::details::chunked_encoding::additional_encoding_space, readSize);
            this_request->m_body_buf.commit(readSize + http::details::chunked_encoding::additional_encoding_space);
            this_request->m_body_buf.consume(offset);
            this_request->m_uploaded += static_cast<uint64_t>(readSize);

            if (readSize != 0)
            {
                this_request->m_connection->async_write(this_request->m_body_buf,
                                                        boost::bind(&asio_context::handle_write_chunked_body, this_request, boost::asio::placeholders::error));
            }
            else
            {
                this_request->m_connection->async_write(this_request->m_body_buf,
                                                        boost::bind(&asio_context::handle_write_body, this_request, boost::asio::placeholders::error));
            }
        });
    }

    void handle_write_large_body(const boost::system::error_code& ec)
    {
        if (ec || m_uploaded >= m_content_length)
        {
            // Reuse error handling.
            return handle_write_body(ec);
        }
        
        m_timer.reset();
        const auto &progress = m_request._get_impl()->_progress_handler();
        if (progress)
        {
            try
            {
                (*progress)(message_direction::upload, m_uploaded);
            }
            catch(...)
            {
                report_exception(std::current_exception());
                return;
            }
        }

        const auto this_request = shared_from_this();
        const auto readSize = static_cast<size_t>(std::min(static_cast<uint64_t>(m_http_client->client_config().chunksize()), m_content_length - m_uploaded));
        auto readbuf = _get_readbuffer();
        readbuf.getn(boost::asio::buffer_cast<uint8_t *>(m_body_buf.prepare(readSize)), readSize).then([this_request](pplx::task<size_t> op)
        {
            try
            {
                const auto actualReadSize = op.get();
                if(actualReadSize == 0)
                {
                    this_request->report_exception(http_exception("Unexpected end of request body stream encountered before Content-Length satisfied."));
                    return;
                }
                this_request->m_uploaded += static_cast<uint64_t>(actualReadSize);
                this_request->m_body_buf.commit(actualReadSize);
                this_request->m_connection->async_write(this_request->m_body_buf, boost::bind(&asio_context::handle_write_large_body, this_request, boost::asio::placeholders::error));
            }
            catch (...)
            {
                this_request->report_exception(std::current_exception());
                return;
            }
        });
    }

    void handle_write_body(const boost::system::error_code& ec)
    {
        if (!ec)
        {
            m_timer.reset();
            const auto &progress = m_request._get_impl()->_progress_handler();
            if (progress)
            {
                try
                {
                    (*progress)(message_direction::upload, m_uploaded);
                }
                catch(...)
                {
                    report_exception(std::current_exception());
                    return;
                }
            }

            // Read until the end of entire headers
            m_connection->async_read_until(m_body_buf, CRLF + CRLF, boost::bind(&asio_context::handle_status_line, shared_from_this(), boost::asio::placeholders::error));
        }
        else
        {
            report_error("Failed to write request body", ec, httpclient_errorcode_context::writebody);
        }
    }

    void handle_status_line(const boost::system::error_code& ec)
    {
        if (!ec)
        {
            m_timer.reset();

            std::istream response_stream(&m_body_buf);
            response_stream.imbue(std::locale::classic());
            std::string http_version;
            response_stream >> http_version;
            status_code status_code;
            response_stream >> status_code;
            
            std::string status_message;
            std::getline(response_stream, status_message);

            m_response.set_status_code(status_code);

            ::web::http::details::trim_whitespace(status_message);
            m_response.set_reason_phrase(std::move(status_message));

            if (!response_stream || http_version.substr(0, 5) != "HTTP/")
            {
                report_error("Invalid HTTP status line", ec, httpclient_errorcode_context::readheader);
                return;
            }

            read_headers();
        }
        else
        {
            // These errors tell if connection was closed.
            const bool socket_was_closed((boost::asio::error::eof == ec)
                                         || (boost::asio::error::connection_reset == ec)
                                         || (boost::asio::error::connection_aborted == ec));
            if (socket_was_closed && m_connection->is_reused())
            {
                // Failed to write to socket because connection was already closed while it was in the pool.
                // close() here ensures socket is closed in a robust way and prevents the connection from being put to the pool again.
                m_connection->close();

                // Create a new context and copy the request object, completion event and
                // cancellation registration to maintain the old state.
                // This also obtains a new connection from pool.
                auto new_ctx = create_request_context(m_http_client, m_request);
                new_ctx->m_request_completion = m_request_completion;
                new_ctx->m_cancellationRegistration = m_cancellationRegistration;

                auto client = std::static_pointer_cast<asio_client>(m_http_client);
                // Resend the request using the new context.
                client->send_request(new_ctx);
            }
            else
            {
                report_error("Failed to read HTTP status line", ec, httpclient_errorcode_context::readheader);
            }
        }
    }

    void read_headers()
    {
        auto needChunked = false;
        std::istream response_stream(&m_body_buf);
        response_stream.imbue(std::locale::classic());
        std::string header;
        while (std::getline(response_stream, header) && header != "\r")
        {
            const auto colon = header.find(':');
            if (colon != std::string::npos)
            {
                auto name = header.substr(0, colon);
                auto value = header.substr(colon + 2, header.size() - (colon + 3)); // also exclude '\r'
                boost::algorithm::trim(name);
                boost::algorithm::trim(value);

                if (boost::iequals(name, header_names::transfer_encoding))
                {
                    needChunked = boost::iequals(value, U("chunked"));
                }

                if (boost::iequals(name, header_names::connection))
                {
                    // This assumes server uses HTTP/1.1 so that 'Keep-Alive' is the default,
                    // so connection is explicitly closed only if we get "Connection: close".
                    // We don't handle HTTP/1.0 server here. HTTP/1.0 server would need
                    // to respond using 'Connection: Keep-Alive' every time.
                    m_connection->set_keep_alive(!boost::iequals(value, U("close")));
                }

                m_response.headers().add(std::move(name), std::move(value));
            }
        }
        complete_headers();

        m_content_length = std::numeric_limits<size_t>::max(); // Without Content-Length header, size should be same as TCP stream - set it size_t max.
        m_response.headers().match(header_names::content_length, m_content_length);

        // note: need to check for 'chunked' here as well, azure storage sends both
        // transfer-encoding:chunked and content-length:0 (although HTTP says not to)
        if (m_request.method() == U("HEAD") || (!needChunked && m_content_length == 0))
        {
            // we can stop early - no body
            const auto &progress = m_request._get_impl()->_progress_handler();
            if (progress)
            {
                try
                {
                    (*progress)(message_direction::download, 0);
                }
                catch(...)
                {
                    report_exception(std::current_exception());
                    return;
                }
            }

            complete_request(0);
        }
        else
        {
            if (!needChunked)
            {
                async_read_until_buffersize(static_cast<size_t>(std::min(m_content_length, static_cast<uint64_t>(m_http_client->client_config().chunksize()))),
                                            boost::bind(&asio_context::handle_read_content, shared_from_this(), boost::asio::placeholders::error));
            }
            else
            {
                m_connection->async_read_until(m_body_buf, CRLF, boost::bind(&asio_context::handle_chunk_header, shared_from_this(), boost::asio::placeholders::error));
            }
        }
    }

    template <typename ReadHandler>
    void async_read_until_buffersize(size_t size, const ReadHandler &handler)
    {
        size_t size_to_read = 0;
        if (m_body_buf.size() < size)
        {
            size_to_read = size - m_body_buf.size();
        }

        m_connection->async_read(m_body_buf, boost::asio::transfer_exactly(size_to_read), handler);
    }

    void handle_chunk_header(const boost::system::error_code& ec)
    {
        if (!ec)
        {
            m_timer.reset();

            std::istream response_stream(&m_body_buf);
            response_stream.imbue(std::locale::classic());
            std::string line;
            std::getline(response_stream, line);

            std::istringstream octetLine(std::move(line));
            octetLine.imbue(std::locale::classic());
            int octets = 0;
            octetLine >> std::hex >> octets;

            if (octetLine.fail())
            {
                report_error("Invalid chunked response header", boost::system::error_code(), httpclient_errorcode_context::readbody);
            }
            else
            {
                async_read_until_buffersize(octets + CRLF.size(), // + 2 for crlf
                                            boost::bind(&asio_context::handle_chunk, shared_from_this(), boost::asio::placeholders::error, octets));
            }
        }
        else
        {
            report_error("Retrieving message chunk header", ec, httpclient_errorcode_context::readbody);
        }
    }

    void handle_chunk(const boost::system::error_code& ec, int to_read)
    {
        if (!ec)
        {
            m_timer.reset();

            m_downloaded += static_cast<uint64_t>(to_read);
            const auto &progress = m_request._get_impl()->_progress_handler();
            if (progress)
            {
                try
                {
                    (*progress)(message_direction::download, m_downloaded);
                }
                catch (...)
                {
                    report_exception(std::current_exception());
                    return;
                }
            }

            if (to_read == 0)
            {
                m_body_buf.consume(CRLF.size());
                complete_request(m_downloaded);
            }
            else
            {
                auto writeBuffer = _get_writebuffer();
                const auto this_request = shared_from_this();
                writeBuffer.putn_nocopy(boost::asio::buffer_cast<const uint8_t *>(m_body_buf.data()), to_read).then([this_request, to_read](pplx::task<size_t> op)
                {
                    try
                    {
                        op.wait();
                    }
                    catch (...)
                    {
                        this_request->report_exception(std::current_exception());
                        return;
                    }
                    this_request->m_body_buf.consume(to_read + CRLF.size()); // consume crlf
                    this_request->m_connection->async_read_until(this_request->m_body_buf, CRLF, boost::bind(&asio_context::handle_chunk_header, this_request, boost::asio::placeholders::error));
                });
            }
        }
        else
        {
            report_error("Failed to read chunked response part", ec, httpclient_errorcode_context::readbody);
        }
    }

    void handle_read_content(const boost::system::error_code& ec)
    {
        auto writeBuffer = _get_writebuffer();

        if (ec)
        {
            if (ec == boost::asio::error::eof && m_content_length == std::numeric_limits<size_t>::max())
            {
                m_content_length = m_downloaded + m_body_buf.size();
            }
            else
            {
                report_error("Failed to read response body", ec, httpclient_errorcode_context::readbody);
                return;
            }
        }

        m_timer.reset();
        const auto &progress = m_request._get_impl()->_progress_handler();
        if (progress)
        {
            try
            {
                (*progress)(message_direction::download, m_downloaded);
            }
            catch (...)
            {
                report_exception(std::current_exception());
                return;
            }
        }

        if (m_downloaded < m_content_length)
        {
            // more data need to be read
            const auto this_request = shared_from_this();
            writeBuffer.putn_nocopy(boost::asio::buffer_cast<const uint8_t *>(m_body_buf.data()),
                             static_cast<size_t>(std::min(static_cast<uint64_t>(m_body_buf.size()), m_content_length - m_downloaded)))
            .then([this_request](pplx::task<size_t> op)
            {
                size_t writtenSize = 0;
                try
                {
                    writtenSize = op.get();
                    this_request->m_downloaded += static_cast<uint64_t>(writtenSize);
                    this_request->m_body_buf.consume(writtenSize);
                    this_request->async_read_until_buffersize(static_cast<size_t>(std::min(static_cast<uint64_t>(this_request->m_http_client->client_config().chunksize()), this_request->m_content_length - this_request->m_downloaded)),
                                                              boost::bind(&asio_context::handle_read_content, this_request, boost::asio::placeholders::error));
                }
                catch (...)
                {
                    this_request->report_exception(std::current_exception());
                    return;
                }
            });
        }
        else
        {
            // Request is complete no more data to read.
            complete_request(m_downloaded);
        }
    }

    // Simple timer class wrapping Boost deadline timer.
    // Closes the connection when timer fires.
    class timeout_timer
    {
    public:

        timeout_timer(const std::chrono::microseconds& timeout) :
        m_duration(timeout.count()),
        m_state(created),
        m_timer(crossplat::threadpool::shared_instance().service())
        {}

        void set_ctx(const std::weak_ptr<asio_context> &ctx)
        {
            m_ctx = ctx;
        }

        void start()
        {
            assert(m_state == created);
            assert(!m_ctx.expired());
            m_state = started;

            m_timer.expires_from_now(m_duration);
            auto ctx = m_ctx;
            m_timer.async_wait([ctx](const boost::system::error_code& ec)
                               {
                                   handle_timeout(ec, ctx);
                               });
        }

        void reset()
        {
            assert(m_state == started || m_state == timedout);
            assert(!m_ctx.expired());
            if(m_timer.expires_from_now(m_duration) > 0)
            {
                // The existing handler was canceled so schedule a new one.
                assert(m_state == started);
                auto ctx = m_ctx;
                m_timer.async_wait([ctx](const boost::system::error_code& ec)
                {
                    handle_timeout(ec, ctx);
                });
            }
        }

        bool has_timedout() const { return m_state == timedout; }

        bool has_started() const { return m_state == started; }

        void stop()
        {
            m_state = stopped;
            m_timer.cancel();
        }

        static void handle_timeout(const boost::system::error_code& ec,
                                   const std::weak_ptr<asio_context> &ctx)
        {
            if(!ec)
            {
                auto shared_ctx = ctx.lock();
                if (shared_ctx)
                {
                    assert(shared_ctx->m_timer.m_state != timedout);
                    shared_ctx->m_timer.m_state = timedout;
                    shared_ctx->m_connection->close();
                }
            }
        }

    private:
        enum timer_state
        {
            created,
            started,
            stopped,
            timedout
        };

#if defined(ANDROID) || defined(__ANDROID__)
        boost::chrono::microseconds m_duration;
#else
        std::chrono::microseconds m_duration;
#endif
        timer_state m_state;
        std::weak_ptr<asio_context> m_ctx;
        boost::asio::steady_timer m_timer;
    };

    uint64_t m_content_length;
    bool m_needChunked;
    timeout_timer m_timer;
    boost::asio::streambuf m_body_buf;
    std::shared_ptr<asio_connection> m_connection;

#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__))
    bool m_openssl_failed;
#endif
};



http_network_handler::http_network_handler(const uri &base_uri, const http_client_config &client_config) :
    m_http_client_impl(std::make_shared<asio_client>(base_uri, client_config))
{}

pplx::task<http_response> http_network_handler::propagate(http_request request)
{
    auto context = details::asio_context::create_request_context(m_http_client_impl, request);

    // Use a task to externally signal the final result and completion of the task.
    auto result_task = pplx::create_task(context->m_request_completion);

    // Asynchronously send the response with the HTTP client implementation.
    m_http_client_impl->async_send_request(context);

    return result_task;
}

void asio_client::send_request(const std::shared_ptr<request_context> &request_ctx)
{
    auto ctx = std::static_pointer_cast<asio_context>(request_ctx);

    try
    {
        if (ctx->m_connection->is_ssl())
        {
            client_config().invoke_nativehandle_options(ctx->m_connection->m_ssl_stream.get());
        }
        else 
        {
            client_config().invoke_nativehandle_options(&(ctx->m_connection->m_socket));
        }
    }
    catch (...)
    {
        request_ctx->report_exception(std::current_exception());
        return;
    }

    ctx->start_request();
}

}}}} // namespaces
