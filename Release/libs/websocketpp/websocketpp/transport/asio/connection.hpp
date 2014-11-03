/*
 * Copyright (c) 2013, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP
#define WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/logger/levels.hpp>
#include <websocketpp/http/constants.hpp>
#include <websocketpp/transport/asio/base.hpp>
#include <websocketpp/transport/base/connection.hpp>

#include <websocketpp/base64/base64.hpp>
#include <websocketpp/error.hpp>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <sstream>
#include <vector>

namespace websocketpp {
namespace transport {
namespace asio {

typedef lib::function<void(connection_hdl)> tcp_init_handler;

/// Boost Asio based connection transport component
/**
 * transport::asio::connection implements a connection transport component using
 * Boost ASIO that works with the transport::asio::endpoint endpoint transport
 * component.
 */
template <typename config>
class connection : public config::socket_type::socket_con_type {
public:
    /// Type of this connection transport component
    typedef connection<config> type;
    /// Type of a shared pointer to this connection transport component
    typedef lib::shared_ptr<type> ptr;

    /// Type of the socket connection component
    typedef typename config::socket_type::socket_con_type socket_con_type;
    /// Type of a shared pointer to the socket connection component
    typedef typename socket_con_type::ptr socket_con_ptr;
    /// Type of this transport's access logging policy
    typedef typename config::alog_type alog_type;
    /// Type of this transport's error logging policy
    typedef typename config::elog_type elog_type;

    typedef typename config::request_type request_type;
    typedef typename request_type::ptr request_ptr;
    typedef typename config::response_type response_type;
    typedef typename response_type::ptr response_ptr;

    /// Type of a pointer to the ASIO io_service being used
    typedef boost::asio::io_service* io_service_ptr;
    /// Type of a pointer to the ASIO io_service::strand being used
    typedef lib::shared_ptr<boost::asio::io_service::strand> strand_ptr;
    /// Type of a pointer to the ASIO timer class
    typedef lib::shared_ptr<boost::asio::deadline_timer> timer_ptr;

    // connection is friends with its associated endpoint to allow the endpoint
    // to call private/protected utility methods that we don't want to expose
    // to the public api.
    friend class endpoint<config>;

    // generate and manage our own io_service
    explicit connection(bool is_server, alog_type& alog, elog_type& elog)
      : m_is_server(is_server)
      , m_alog(alog)
      , m_elog(elog)
    {
        m_alog.write(log::alevel::devel,"asio con transport constructor");
    }

    /// Get a shared pointer to this component
    ptr get_shared() {
        return lib::static_pointer_cast<type>(socket_con_type::get_shared());
    }

    bool is_secure() const {
        return socket_con_type::is_secure();
    }

    /// Sets the tcp pre init handler
    /**
     * The tcp pre init handler is called after the raw tcp connection has been
     * established but before any additional wrappers (proxy connects, TLS
     * handshakes, etc) have been performed.
     *
     * @since 0.3.0
     *
     * @param h The handler to call on tcp pre init.
     */
    void set_tcp_pre_init_handler(tcp_init_handler h) {
        m_tcp_pre_init_handler = h;
    }

    /// Sets the tcp pre init handler (deprecated)
    /**
     * The tcp pre init handler is called after the raw tcp connection has been
     * established but before any additional wrappers (proxy connects, TLS
     * handshakes, etc) have been performed.
     *
     * @deprecated Use set_tcp_pre_init_handler instead
     *
     * @param h The handler to call on tcp pre init.
     */
    void set_tcp_init_handler(tcp_init_handler h) {
        set_tcp_pre_init_handler(h);
    }

    /// Sets the tcp post init handler
    /**
     * The tcp post init handler is called after the tcp connection has been
     * established and all additional wrappers (proxy connects, TLS handshakes,
     * etc have been performed. This is fired before any bytes are read or any
     * WebSocket specific handshake logic has been performed.
     *
     * @since 0.3.0
     *
     * @param h The handler to call on tcp post init.
     */
    void set_tcp_post_init_handler(tcp_init_handler h) {
        m_tcp_post_init_handler = h;
    }

    /// Set the proxy to connect through (exception free)
    /**
     * The URI passed should be a complete URI including scheme. For example:
     * http://proxy.example.com:8080/
     *
     * The proxy must be set up as an explicit (CONNECT) proxy allowed to
     * connect to the port you specify. Traffic to the proxy is not encrypted.
     *
     * @param uri The full URI of the proxy to connect to.
     *
     * @param ec A status value
     */
    void set_proxy(const std::string & uri, lib::error_code & ec) {
        // TODO: return errors for illegal URIs here?
        // TODO: should https urls be illegal for the moment?
        m_proxy = uri;
        m_proxy_data.reset(new proxy_data());
        ec = lib::error_code();
    }

    /// Set the proxy to connect through (exception)
    void set_proxy(const std::string & uri) {
        lib::error_code ec;
        set_proxy(uri,ec);
        if (ec) { throw ec; }
    }

    /// Set the basic auth credentials to use (exception free)
    /**
     * The URI passed should be a complete URI including scheme. For example:
     * http://proxy.example.com:8080/
     *
     * The proxy must be set up as an explicit proxy
     *
     * @param username The username to send
     *
     * @param password The password to send
     *
     * @param ec A status value
     */
    void set_proxy_basic_auth(const std::string & username, const
        std::string & password, lib::error_code & ec)
    {
        if (!m_proxy_data) {
            ec = make_error_code(websocketpp::error::invalid_state);
            return;
        }

        // TODO: username can't contain ':'
        std::string val = "Basic "+base64_encode(username + ":" + password);
        m_proxy_data->req.replace_header("Proxy-Authorization",val);
        ec = lib::error_code();
    }

    /// Set the basic auth credentials to use (exception)
    void set_proxy_basic_auth(const std::string & username, const
        std::string & password)
    {
        lib::error_code ec;
        set_proxy_basic_auth(username,password,ec);
        if (ec) { throw ec; }
    }

    /// Set the proxy timeout duration (exception free)
    /**
     * Duration is in milliseconds. Default value is based on the transport
     * config
     *
     * @param duration The number of milliseconds to wait before aborting the
     * proxy connection.
     *
     * @param ec A status value
     */
    void set_proxy_timeout(long duration, lib::error_code & ec) {
        if (!m_proxy_data) {
            ec = make_error_code(websocketpp::error::invalid_state);
            return;
        }

        m_proxy_data->timeout_proxy = duration;
        ec = lib::error_code();
    }

    /// Set the proxy timeout duration (exception)
    void set_proxy_timeout(long duration) {
        lib::error_code ec;
        set_proxy_timeout(duration,ec);
        if (ec) { throw ec; }
    }

    const std::string & get_proxy() const {
        return m_proxy;
    }

    /// Get the remote endpoint address
    /**
     * The iostream transport has no information about the ultimate remote
     * endpoint. It will return the string "iostream transport". To indicate
     * this.
     *
     * TODO: allow user settable remote endpoint addresses if this seems useful
     *
     * @return A string identifying the address of the remote endpoint
     */
    std::string get_remote_endpoint() const {
        lib::error_code ec;

        std::string ret = socket_con_type::get_remote_endpoint(ec);

        if (ec) {
            m_elog.write(log::elevel::info,ret);
            return "Unknown";
        } else {
            return ret;
        }
    }

    /// Get the connection handle
    connection_hdl get_handle() const {
        return m_connection_hdl;
    }

    /// Call back a function after a period of time.
    /**
     * Sets a timer that calls back a function after the specified period of
     * milliseconds. Returns a handle that can be used to cancel the timer.
     * A cancelled timer will return the error code error::operation_aborted
     * A timer that expired will return no error.
     *
     * @param duration Length of time to wait in milliseconds
     *
     * @param callback The function to call back when the timer has expired
     *
     * @return A handle that can be used to cancel the timer if it is no longer
     * needed.
     */
    timer_ptr set_timer(long duration, timer_handler callback) {
        timer_ptr new_timer(
            new boost::asio::deadline_timer(
                *m_io_service,
                boost::posix_time::milliseconds(duration)
            )
        );

        if (config::enable_multithreading) {
            new_timer->async_wait(m_strand->wrap(lib::bind(
                &type::handle_timer, get_shared(),
                new_timer,
                callback,
                lib::placeholders::_1
            )));
        } else {
            new_timer->async_wait(lib::bind(
                &type::handle_timer, get_shared(),
                new_timer,
                callback,
                lib::placeholders::_1
            ));
        }

        return new_timer;
    }

    /// Timer callback
    /**
     * The timer pointer is included to ensure the timer isn't destroyed until
     * after it has expired.
     *
     * TODO: candidate for protected status
     *
     * @param t Pointer to the timer in question
     * @param callback The function to call back
     * @param ec The status code
     */
    void handle_timer(timer_ptr t, timer_handler callback,
        boost::system::error_code const & ec)
    {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                callback(make_error_code(transport::error::operation_aborted));
            } else {
                log_err(log::elevel::info,"asio handle_timer",ec);
                callback(make_error_code(error::pass_through));
            }
        } else {
            callback(lib::error_code());
        }
    }
protected:
    /// Get a pointer to this connection's strand
    strand_ptr get_strand() {
        return m_strand;
    }

    /// Initialize transport for reading
    /**
     * init_asio is called once immediately after construction to initialize
     * boost::asio components to the io_service
     *
     * The transport initialization sequence consists of the following steps:
     * - Pre-init: the underlying socket is initialized to the point where
     * bytes may be written. No bytes are actually written in this stage
     * - Proxy negotiation: if a proxy is set, a request is made to it to start
     * a tunnel to the final destination. This stage ends when the proxy is
     * ready to forward the
     * next byte to the remote endpoint.
     * - Post-init: Perform any i/o with the remote endpoint, such as setting up
     * tunnels for encryption. This stage ends when the connection is ready to
     * read or write the WebSocket handshakes. At this point the original
     * callback function is called.
     */
    void init(init_handler callback) {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection init");
        }

        // TODO: pre-init timeout. Right now no implemented socket policies
        // actually have an asyncronous pre-init

        m_init_handler = callback;

        socket_con_type::pre_init(
            lib::bind(
                &type::handle_pre_init,
                get_shared(),
                lib::placeholders::_1
            )
        );
    }

    /// initialize the proxy buffers and http parsers
    /**
     *
     * @param authority The address of the server we want the proxy to tunnel to
     * in the format of a URI authority (host:port)
     *
     * @return Status code indicating what errors occurred, if any
     */
    lib::error_code proxy_init(std::string const & authority) {
        if (!m_proxy_data) {
            return websocketpp::error::make_error_code(
                websocketpp::error::invalid_state);
        }
        m_proxy_data->req.set_version("HTTP/1.1");
        m_proxy_data->req.set_method("CONNECT");

        m_proxy_data->req.set_uri(authority);
        m_proxy_data->req.replace_header("Host",authority);

        return lib::error_code();
    }

    /// Finish constructing the transport
    /**
     * init_asio is called once immediately after construction to initialize
     * boost::asio components to the io_service.
     *
     * @param io_service A pointer to the io_service to register with this
     * connection
     *
     * @return Status code for the success or failure of the initialization
     */
    lib::error_code init_asio (io_service_ptr io_service) {
        m_io_service = io_service;

        if (config::enable_multithreading) {
            m_strand.reset(new boost::asio::strand(*io_service));

            m_async_read_handler = m_strand->wrap(lib::bind(
                &type::handle_async_read, get_shared(),lib::placeholders::_1,
                lib::placeholders::_2));

            m_async_write_handler = m_strand->wrap(lib::bind(
                &type::handle_async_write, get_shared(),lib::placeholders::_1,
                lib::placeholders::_2));
        } else {
            m_async_read_handler = lib::bind(&type::handle_async_read,
                get_shared(), lib::placeholders::_1, lib::placeholders::_2);

            m_async_write_handler = lib::bind(&type::handle_async_write,
                get_shared(), lib::placeholders::_1, lib::placeholders::_2);
        }

        lib::error_code ec = socket_con_type::init_asio(io_service, m_strand,
            m_is_server);

        if (ec) {
            // reset the handlers to break the circular reference:
            // this->handler->this
            m_async_read_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
            m_async_write_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
        }

        return ec;
    }

    void handle_pre_init(lib::error_code const & ec) {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection handle pre_init");
        }

        if (m_tcp_pre_init_handler) {
            m_tcp_pre_init_handler(m_connection_hdl);
        }

        if (ec) {
            m_init_handler(ec);
        }

        // If we have a proxy set issue a proxy connect, otherwise skip to
        // post_init
        if (!m_proxy.empty()) {
            proxy_write();
        } else {
            post_init();
        }
    }

    void post_init() {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection post_init");
        }

        timer_ptr post_timer;
        
        if (config::timeout_socket_post_init > 0) {
            post_timer = set_timer(
                config::timeout_socket_post_init,
                lib::bind(
                    &type::handle_post_init_timeout,
                    get_shared(),
                    post_timer,
                    m_init_handler,
                    lib::placeholders::_1
                )
            );
        }

        socket_con_type::post_init(
            lib::bind(
                &type::handle_post_init,
                get_shared(),
                post_timer,
                m_init_handler,
                lib::placeholders::_1
            )
        );
    }

    void handle_post_init_timeout(timer_ptr post_timer, init_handler callback,
        lib::error_code const & ec)
    {
        lib::error_code ret_ec;

        if (ec) {
            if (ec == transport::error::operation_aborted) {
                m_alog.write(log::alevel::devel,
                    "asio post init timer cancelled");
                return;
            }

            log_err(log::elevel::devel,"asio handle_post_init_timeout",ec);
            ret_ec = ec;
        } else {
            if (socket_con_type::get_ec()) {
                ret_ec = socket_con_type::get_ec();
            } else {
                ret_ec = make_error_code(transport::error::timeout);
            }
        }

        m_alog.write(log::alevel::devel,"Asio transport post-init timed out");
        socket_con_type::cancel_socket();
        callback(ret_ec);
    }

    void handle_post_init(timer_ptr post_timer, init_handler callback,
        lib::error_code const & ec)
    {
        if (ec == transport::error::operation_aborted ||
            (post_timer && post_timer->expires_from_now().is_negative()))
        {
            m_alog.write(log::alevel::devel,"post_init cancelled");
            return;
        }

        if (post_timer) {
            post_timer->cancel();
        }

        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection handle_post_init");
        }

		if (m_tcp_post_init_handler) {
            m_tcp_post_init_handler(m_connection_hdl);
        }

        callback(ec);
    }

    void proxy_write() {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection proxy_write");
        }

        if (!m_proxy_data) {
            m_elog.write(log::elevel::library,
                "assertion failed: !m_proxy_data in asio::connection::proxy_write");
            m_init_handler(make_error_code(error::general));
            return;
        }

        m_proxy_data->write_buf = m_proxy_data->req.raw();

        m_bufs.push_back(boost::asio::buffer(m_proxy_data->write_buf.data(),
                                             m_proxy_data->write_buf.size()));

        m_alog.write(log::alevel::devel,m_proxy_data->write_buf);

        // Set a timer so we don't wait forever for the proxy to respond
        m_proxy_data->timer = this->set_timer(
            m_proxy_data->timeout_proxy,
            lib::bind(
                &type::handle_proxy_timeout,
                get_shared(),
                m_init_handler,
                lib::placeholders::_1
            )
        );

        // Send proxy request
        if (config::enable_multithreading) {
            boost::asio::async_write(
                socket_con_type::get_next_layer(),
                m_bufs,
                m_strand->wrap(lib::bind(
                    &type::handle_proxy_write, get_shared(),
                    m_init_handler,
                    lib::placeholders::_1
                ))
            );
        } else {
            boost::asio::async_write(
                socket_con_type::get_next_layer(),
                m_bufs,
                lib::bind(
                    &type::handle_proxy_write, get_shared(),
                    m_init_handler,
                    lib::placeholders::_1
                )
            );
        }
    }

    void handle_proxy_timeout(init_handler callback, lib::error_code const & ec)
    {
        if (ec == transport::error::operation_aborted) {
            m_alog.write(log::alevel::devel,
                "asio handle_proxy_write timer cancelled");
            return;
        } else if (ec) {
            log_err(log::elevel::devel,"asio handle_proxy_write",ec);
            callback(ec);
        } else {
            m_alog.write(log::alevel::devel,
                "asio handle_proxy_write timer expired");
            socket_con_type::cancel_socket();
            callback(make_error_code(transport::error::timeout));
        }
    }

    void handle_proxy_write(init_handler callback,
        boost::system::error_code const & ec)
    {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,
                "asio connection handle_proxy_write");
        }

        m_bufs.clear();

        // Timer expired or the operation was aborted for some reason.
        // Whatever aborted it will be issuing the callback so we are safe to
        // return
        if (ec == boost::asio::error::operation_aborted ||
            m_proxy_data->timer->expires_from_now().is_negative())
        {
            m_elog.write(log::elevel::devel,"write operation aborted");
            return;
        }

        if (ec) {
            log_err(log::elevel::info,"asio handle_proxy_write",ec);
            m_proxy_data->timer->cancel();
            callback(make_error_code(error::pass_through));
            return;
        }

        proxy_read(callback);
    }

    void proxy_read(init_handler callback) {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection proxy_read");
        }

        if (!m_proxy_data) {
            m_elog.write(log::elevel::library,
                "assertion failed: !m_proxy_data in asio::connection::proxy_read");
            m_proxy_data->timer->cancel();
            callback(make_error_code(error::general));
            return;
        }

        if (config::enable_multithreading) {
            boost::asio::async_read_until(
                socket_con_type::get_next_layer(),
                m_proxy_data->read_buf,
                "\r\n\r\n",
                m_strand->wrap(lib::bind(
                    &type::handle_proxy_read, get_shared(),
                    callback,
                    lib::placeholders::_1, lib::placeholders::_2
                ))
            );
        } else {
            boost::asio::async_read_until(
                socket_con_type::get_next_layer(),
                m_proxy_data->read_buf,
                "\r\n\r\n",
                lib::bind(
                    &type::handle_proxy_read, get_shared(),
                    callback,
                    lib::placeholders::_1, lib::placeholders::_2
                )
            );
        }
    }

    void handle_proxy_read(init_handler callback,
        boost::system::error_code const & ec, size_t bytes_transferred)
    {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,
                "asio connection handle_proxy_read");
        }

        // Timer expired or the operation was aborted for some reason.
        // Whatever aborted it will be issuing the callback so we are safe to
        // return
        if (ec == boost::asio::error::operation_aborted ||
            m_proxy_data->timer->expires_from_now().is_negative())
        {
            m_elog.write(log::elevel::devel,"read operation aborted");
            return;
        }

        // At this point there is no need to wait for the timer anymore
        m_proxy_data->timer->cancel();

        if (ec) {
            m_elog.write(log::elevel::info,
                "asio handle_proxy_read error: "+ec.message());
            callback(make_error_code(error::pass_through));
        } else {
            if (!m_proxy_data) {
                m_elog.write(log::elevel::library,
                    "assertion failed: !m_proxy_data in asio::connection::handle_proxy_read");
                callback(make_error_code(error::general));
                return;
            }

            std::istream input(&m_proxy_data->read_buf);

            m_proxy_data->res.consume(input);

            if (!m_proxy_data->res.headers_ready()) {
                // we read until the headers were done in theory but apparently
                // they aren't. Internal endpoint error.
                callback(make_error_code(error::general));
                return;
            }

            m_alog.write(log::alevel::devel,m_proxy_data->res.raw());

            if (m_proxy_data->res.get_status_code() != http::status_code::ok) {
                // got an error response back
                // TODO: expose this error in a programmatically accessible way?
                // if so, see below for an option on how to do this.
                std::stringstream s;
                s << "Proxy connection error: "
                  << m_proxy_data->res.get_status_code()
                  << " ("
                  << m_proxy_data->res.get_status_msg()
                  << ")";
                m_elog.write(log::elevel::info,s.str());
                callback(make_error_code(error::proxy_failed));
                return;
            }

            // we have successfully established a connection to the proxy, now
            // we can continue and the proxy will transparently forward the
            // WebSocket connection.

            // TODO: decide if we want an on_proxy callback that would allow
            // access to the proxy response.

            // free the proxy buffers and req/res objects as they aren't needed
            // anymore
            m_proxy_data.reset();

            // Continue with post proxy initialization
            post_init();
        }
    }

    /// read at least num_bytes bytes into buf and then call handler.
    /**
     *
     *
     */
    void async_read_at_least(size_t num_bytes, char *buf, size_t len,
        read_handler handler)
    {
        if (m_alog.static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "asio async_read_at_least: " << num_bytes;
            m_alog.write(log::alevel::devel,s.str());
        }

        if (!m_async_read_handler) {
            m_alog.write(log::alevel::devel,
                "async_read_at_least called after async_shutdown");
            handler(make_error_code(transport::error::action_after_shutdown),0);
            return;
        }

        // TODO: safety vs speed ?
        // maybe move into an if devel block
        /*if (num_bytes > len) {
            m_elog.write(log::elevel::devel,
                "asio async_read_at_least error::invalid_num_bytes");
            handler(make_error_code(transport::error::invalid_num_bytes),
                size_t(0));
            return;
        }*/

        m_read_handler = handler;

        if (!m_read_handler) {
            m_alog.write(log::alevel::devel,
                "asio con async_read_at_least called with bad handler");
        }

        boost::asio::async_read(
            socket_con_type::get_socket(),
            boost::asio::buffer(buf,len),
            boost::asio::transfer_at_least(num_bytes),
            make_custom_alloc_handler(
            	m_read_handler_allocator,
            	m_async_read_handler
            )
        );
    }

    void handle_async_read(boost::system::error_code const & ec,
        size_t bytes_transferred)
    {
        m_alog.write(log::alevel::devel, "asio con handle_async_read");

        // translate boost error codes into more lib::error_codes
        lib::error_code tec;
        if (ec == boost::asio::error::eof) {
            tec = make_error_code(transport::error::eof);
        } else if (ec) {
            // We don't know much more about the error at this point. As our
            // socket/security policy if it knows more:
            tec = socket_con_type::translate_ec(ec);

            if (tec == transport::error::tls_error ||
                tec == transport::error::pass_through)
            {
                // These are aggregate/catch all errors. Log some human readable
                // information to the info channel to give library users some
                // more details about why the upstream method may have failed.
                log_err(log::elevel::info,"asio async_read_at_least",ec);
            }
        }
        if (m_read_handler) {
            m_read_handler(tec,bytes_transferred);
            // TODO: why does this line break things?
            //m_read_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
        } else {
            // This can happen in cases where the connection is terminated while
            // the transport is waiting on a read.
            m_alog.write(log::alevel::devel,
                "handle_async_read called with null read handler");
        }
    }

    void async_write(const char* buf, size_t len, write_handler handler) {
        if (!m_async_write_handler) {
            m_alog.write(log::alevel::devel,
                "async_write (single) called after async_shutdown");
            handler(make_error_code(transport::error::action_after_shutdown));
            return;
        }

		m_bufs.push_back(boost::asio::buffer(buf,len));

        m_write_handler = handler;

        boost::asio::async_write(
            socket_con_type::get_socket(),
            m_bufs,
            make_custom_alloc_handler(
            	m_write_handler_allocator,
            	m_async_write_handler
            )
        );
    }

    void async_write(const std::vector<buffer>& bufs, write_handler handler) {
        if (!m_async_write_handler) {
            m_alog.write(log::alevel::devel,
                "async_write (vector) called after async_shutdown");
            handler(make_error_code(transport::error::action_after_shutdown));
            return;
        }
        std::vector<buffer>::const_iterator it;

        for (it = bufs.begin(); it != bufs.end(); ++it) {
            m_bufs.push_back(boost::asio::buffer((*it).buf,(*it).len));
        }

        m_write_handler = handler;

        boost::asio::async_write(
            socket_con_type::get_socket(),
            m_bufs,
            make_custom_alloc_handler(
            	m_write_handler_allocator,
            	m_async_write_handler
            )
        );
    }

    void handle_async_write(boost::system::error_code const & ec,
        size_t bytes_transferred)
    {
        m_bufs.clear();
        lib::error_code tec;
        if (ec) {
            log_err(log::elevel::info,"asio async_write",ec);
            tec = make_error_code(transport::error::pass_through);
        }
        if (m_write_handler) {
            m_write_handler(tec);
            // TODO: why does this line break things?
            //m_write_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
        } else {
            // This can happen in cases where the connection is terminated while
            // the transport is waiting on a read.
            m_alog.write(log::alevel::devel,
                "handle_async_write called with null write handler");
        }
    }

    /// Set Connection Handle
    /**
     * See common/connection_hdl.hpp for information
     *
     * @param hdl A connection_hdl that the transport will use to refer
     * to itself
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
        socket_con_type::set_handle(hdl);
    }

    /// Trigger the on_interrupt handler
    /**
     * This needs to be thread safe
     */
    lib::error_code interrupt(interrupt_handler handler) {
        if (config::enable_multithreading) {
            m_io_service->post(m_strand->wrap(handler));
        } else {
            m_io_service->post(handler);
        }
        return lib::error_code();
    }

    lib::error_code dispatch(dispatch_handler handler) {
        if (config::enable_multithreading) {
            m_io_service->post(m_strand->wrap(handler));
        } else {
            m_io_service->post(handler);
        }
        return lib::error_code();
    }

    /*void handle_interrupt(interrupt_handler handler) {
        handler();
    }*/

    /// close and clean up the underlying socket
    void async_shutdown(shutdown_handler callback) {
        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,"asio connection async_shutdown");
        }

		// Reset cached handlers now that we won't be reading or writing anymore
		// These cached handlers store shared pointers to this connection and
		// will leak the connection if not destroyed.
		m_async_read_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
		m_async_write_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
		m_init_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;

        m_read_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;
        m_write_handler = _WEBSOCKETPP_NULLPTR_TOKEN_;

        timer_ptr shutdown_timer;
        shutdown_timer = set_timer(
            config::timeout_socket_shutdown,
            lib::bind(
                &type::handle_async_shutdown_timeout,
                get_shared(),
                shutdown_timer,
                callback,
                lib::placeholders::_1
            )
        );

        socket_con_type::async_shutdown(
            lib::bind(
                &type::handle_async_shutdown,
                get_shared(),
                shutdown_timer,
                callback,
                lib::placeholders::_1
            )
        );
    }

    void handle_async_shutdown_timeout(timer_ptr shutdown_timer, init_handler
        callback, lib::error_code const & ec)
    {
        lib::error_code ret_ec;

        if (ec) {
            if (ec == transport::error::operation_aborted) {
                m_alog.write(log::alevel::devel,
                    "asio socket shutdown timer cancelled");
                return;
            }

            log_err(log::elevel::devel,"asio handle_async_shutdown_timeout",ec);
            ret_ec = ec;
        } else {
            ret_ec = make_error_code(transport::error::timeout);
        }

        m_alog.write(log::alevel::devel,
            "Asio transport socket shutdown timed out");
        socket_con_type::cancel_socket();
        callback(ret_ec);
    }

    void handle_async_shutdown(timer_ptr shutdown_timer, shutdown_handler
        callback, boost::system::error_code const & ec)
    {
        if (ec == boost::asio::error::operation_aborted ||
            shutdown_timer->expires_from_now().is_negative())
        {
            m_alog.write(log::alevel::devel,"async_shutdown cancelled");
            return;
        }

        shutdown_timer->cancel();

        lib::error_code tec;
        if (ec) {
            if (ec == boost::asio::error::not_connected) {
                // The socket was already closed when we tried to close it. This
                // happens periodically (usually if a read or write fails
                // earlier and if it is a real error will be caught at another
                // level of the stack.
            } else {
                // We don't know anything more about this error, give our
                // socket/security policy a crack at it.
                tec = socket_con_type::translate_ec(ec);

                if (tec == transport::error::tls_short_read) {
                    // TLS short read at this point is somewhat expected if both
                    // sides try and end the connection at the same time or if
                    // SSLv2 is being used. In general there is nothing that can
                    // be done here other than a low level development log.
                } else {
                    // all other errors are effectively pass through errors of
                    // some sort so print some detail on the info channel for
                    // library users to look up if needed.
                    log_err(log::elevel::info,"asio async_shutdown",ec);
                }
            }
        } else {
            if (m_alog.static_test(log::alevel::devel)) {
                m_alog.write(log::alevel::devel,
                    "asio con handle_async_shutdown");
            }
        }
        callback(tec);
    }
private:
    /// Convenience method for logging the code and message for an error_code
    template <typename error_type>
    void log_err(log::level l, const char * msg, const error_type & ec) {
        std::stringstream s;
        s << msg << " error: " << ec << " (" << ec.message() << ")";
        m_elog.write(l,s.str());
    }

    // static settings
    const bool m_is_server;
    alog_type& m_alog;
    elog_type& m_elog;

    struct proxy_data {
        proxy_data() : timeout_proxy(config::timeout_proxy) {}

        request_type req;
        response_type res;
        std::string write_buf;
        boost::asio::streambuf read_buf;
        long timeout_proxy;
        timer_ptr timer;
    };

    std::string m_proxy;
    lib::shared_ptr<proxy_data> m_proxy_data;

    // transport resources
    io_service_ptr  m_io_service;
    strand_ptr      m_strand;
    connection_hdl  m_connection_hdl;

    std::vector<boost::asio::const_buffer> m_bufs;

    // Handlers
    tcp_init_handler    m_tcp_pre_init_handler;
    tcp_init_handler    m_tcp_post_init_handler;

    handler_allocator   m_read_handler_allocator;
    handler_allocator   m_write_handler_allocator;

    read_handler        m_read_handler;
    write_handler       m_write_handler;
    init_handler        m_init_handler;

    async_read_handler  m_async_read_handler;
    async_write_handler m_async_write_handler;
};


} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP
