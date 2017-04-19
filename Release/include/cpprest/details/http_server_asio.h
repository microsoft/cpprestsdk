/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
****/

#pragma once

#include <set>
#include "pplx/threadpool.h"
#include "cpprest/details/http_server.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Winfinite-recursion"
#endif
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace web
{
namespace http
{
namespace experimental
{
namespace listener
{

class http_linux_server;

namespace details
{

struct linux_request_context : web::http::details::_http_server_context
{
    linux_request_context(){}

    pplx::task_completion_event<void> m_response_completed;

private:
    linux_request_context(const linux_request_context&);
    linux_request_context& operator=(const linux_request_context&);
};

class asio_server_connection;

class hostport_listener
{
private:
    friend class asio_server_connection;

    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
    std::map<std::string, web::http::experimental::listener::details::http_listener_impl* > m_listeners;
    pplx::extensibility::reader_writer_lock_t m_listeners_lock;

    std::mutex m_connections_lock;
    pplx::extensibility::event_t m_all_connections_complete;
    std::set<asio_server_connection*> m_connections;

    http_linux_server* m_p_server;

    std::string m_host;
    std::string m_port;

    bool m_is_https;
    const std::function<void(boost::asio::ssl::context&)>& m_ssl_context_callback;

public:
     hostport_listener(http_linux_server* server, const std::string& hostport, bool is_https, const http_listener_config& config)
    : m_acceptor()
    , m_listeners()
    , m_listeners_lock()
    , m_connections_lock()
    , m_connections()
    , m_p_server(server)
    , m_is_https(is_https)
    , m_ssl_context_callback(config.get_ssl_context_callback())
    {
        m_all_connections_complete.set();

        std::istringstream hostport_in(hostport);
        hostport_in.imbue(std::locale::classic());

        std::getline(hostport_in, m_host, ':');
        std::getline(hostport_in, m_port);
    }

    ~hostport_listener()
    {
        stop();
    }

    void start();
    void stop();

    void add_listener(const std::string& path, web::http::experimental::listener::details::http_listener_impl* listener);
    void remove_listener(const std::string& path, web::http::experimental::listener::details::http_listener_impl* listener);

private:
    void on_accept(boost::asio::ip::tcp::socket* socket, const boost::system::error_code& ec);

};

}

struct iequal_to
{
    bool operator()(const std::string& left, const std::string& right) const
    {
        return boost::ilexicographical_compare(left, right);
    }
};

class http_linux_server : public web::http::experimental::details::http_server
{
private:
    friend class http::experimental::listener::details::asio_server_connection;

    pplx::extensibility::reader_writer_lock_t m_listeners_lock;
    std::map<std::string, std::unique_ptr<details::hostport_listener>, iequal_to> m_listeners;
    std::unordered_map<details::http_listener_impl *, std::unique_ptr<pplx::extensibility::reader_writer_lock_t>> m_registered_listeners;
    bool m_started;

public:
    http_linux_server()
    : m_listeners_lock()
    , m_listeners()
    , m_started(false)
    {}

    ~http_linux_server()
    {
        stop();
    }

    virtual pplx::task<void> start();
    virtual pplx::task<void> stop();

    virtual pplx::task<void> register_listener(web::http::experimental::listener::details::http_listener_impl* listener);
    virtual pplx::task<void> unregister_listener(web::http::experimental::listener::details::http_listener_impl* listener);

    pplx::task<void> respond(http::http_response response);
};

}}}}
