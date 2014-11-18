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
* ws_client.cpp
*
* Websocket library: Client-side APIs.
*
* This file contains the implementation for Windows 8 Desktop & Non-Windows
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/producerconsumerstream.h"
#include "cpprest/containerstream.h"
#include "cpprest/ws_msg.h"
#include "cpprest/ws_client.h"
#include "cpprest/x509_cert_utilities.h"
#include <memory>
#include <thread>

// Force websocketpp to use C++ std::error_code instead of Boost.
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_

#if !defined(CPPREST_EXCLUDE_WEBSOCKETS)

#if ((!defined(WINAPI_FAMILY) || WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) && !defined(_M_ARM) && (!defined(_MSC_VER) || (_MSC_VER < 1900)))
#if defined(__GNUC__)
#include "pplx/threadpool.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#if defined(__APPLE__)
#include "stdlib.h"
// Issue caused by iOS SDK 8.0
#pragma push_macro("ntohll")
#pragma push_macro("htonll")
#undef ntohll
#undef htonll
#endif
#define _WEBSOCKETPP_NULLPTR_TOKEN_ 0
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#if defined(__APPLE__)
#pragma pop_macro("htonll")
#pragma pop_macro("ntohll")
#endif
#pragma GCC diagnostic pop
#else /* __GNUC__ */
#pragma warning( push )
#pragma warning( disable : 4100 4127 4512 4996 4701 4267 )
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#define _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_INITIALIZER_LISTS_
#define _WEBSOCKETPP_NOEXCEPT_TOKEN_
#define _WEBSOCKETPP_CONSTEXPR_TOKEN_
#else
#define _WEBSOCKETPP_NULLPTR_TOKEN_ 0
#endif
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#pragma warning( pop )
#endif /* __GNUC__ */

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace web
{
namespace websockets
{
namespace client
{
namespace details
{

// Utility function to build up error string based on error code and location.
static std::string build_error_msg(const std::error_code &ec, const std::string &location)
{
    std::stringstream ss;
    ss << location
       << ": " << ec.value()
       << ": " << ec.message();
    return ss.str();
}

static utility::string_t g_subProtocolHeader(_XPLATSTR("Sec-WebSocket-Protocol"));

class wspp_callback_client : public websocket_client_callback_impl, public std::enable_shared_from_this<wspp_callback_client>
{
private:
    enum State {
        CREATED,
        CONNECTING,
        CONNECTED,
        CLOSING,
        CLOSED,
        DESTROYED
    };
public:
    wspp_callback_client(websocket_client_config config) :
        websocket_client_callback_impl(std::move(config)),
        m_state(CREATED),
        m_num_sends(0)
#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__)) || defined(_MS_WINDOWS)
        , m_openssl_failed(false)
#endif
    {}

    ~wspp_callback_client()
    {
        _ASSERTE(m_state < DESTROYED);

        // Now, what states could we be in?
        switch (m_state) {
        case DESTROYED:
            // These should be impossible
            std::abort();
        case CREATED:
        case CLOSED:
            // In these cases, nothing need be done.
            break;
        case CONNECTING:
        case CONNECTED:
        case CLOSING:
            try
            {
                // This will do nothing in the already-connected case
                pplx::task<void>(m_connect_tce).get();
            }
            catch (...) {}
            try
            {
                // This will do nothing in the already-closing case
                close().wait();
            }
            catch (...) {}
            break;
        }

        // We have released the lock on all paths here
        if (m_client)
        {
            if (m_client->is_tls_client())
            {
                stop_client_impl<websocketpp::config::asio_tls_client>();
            }
            else
            {
                stop_client_impl<websocketpp::config::asio_client>();
            }
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        // At this point, there should be no more references to me.
        m_state = DESTROYED;
    }

    template <typename WebsocketConfigType>
    void stop_client_impl()
    {
        auto &client = m_client->client<WebsocketConfigType>();
        client.stop_perpetual();
    }

    pplx::task<void> connect()
    {
        if (m_uri.scheme() == U("wss"))
        {
            m_client = std::unique_ptr<websocketpp_client_base>(new websocketpp_tls_client());

            // Options specific to TLS client.
            auto &client = m_client->client<websocketpp::config::asio_tls_client>();
            client.set_tls_init_handler([this](websocketpp::connection_hdl)
            {
                auto sslContext = websocketpp::lib::shared_ptr<boost::asio::ssl::context>(new boost::asio::ssl::context(boost::asio::ssl::context::sslv23));
                sslContext->set_default_verify_paths();
                sslContext->set_options(boost::asio::ssl::context::default_workarounds);
                sslContext->set_verify_mode(boost::asio::ssl::context::verify_peer);
#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__)) || defined(_MS_WINDOWS)
                m_openssl_failed = false;
#endif
                sslContext->set_verify_callback([this](bool preverified, boost::asio::ssl::verify_context &verifyCtx)
                {
#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__)) || defined(_MS_WINDOWS)
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
                        return http::client::details::verify_cert_chain_platform_specific(verifyCtx, utility::conversions::to_utf8string(m_uri.host()));
                    }
#endif
                    boost::asio::ssl::rfc2818_verification rfc2818(utility::conversions::to_utf8string(m_uri.host()));
                    return rfc2818(preverified, verifyCtx);
                });

                return sslContext;
            });
            return connect_impl<websocketpp::config::asio_tls_client>();
        }
        else
        {
            m_client = std::unique_ptr<websocketpp_client_base>(new websocketpp_client());
            return connect_impl<websocketpp::config::asio_client>();
        }
    }

    template <typename WebsocketConfigType>
    pplx::task<void> connect_impl()
    {
        auto &client = m_client->client<WebsocketConfigType>();

        client.clear_access_channels(websocketpp::log::alevel::all);
        client.clear_error_channels(websocketpp::log::alevel::all);
        client.init_asio();
        client.start_perpetual();

        _ASSERTE(m_state == CREATED);
        client.set_open_handler([this](websocketpp::connection_hdl)
        {
            _ASSERTE(m_state == CONNECTING);
            m_state = CONNECTED;
            m_connect_tce.set();
        });

        client.set_fail_handler([this](websocketpp::connection_hdl con_hdl)
        {
            _ASSERTE(m_state == CONNECTING);
            auto &client = m_client->client<WebsocketConfigType>();
            const auto &ec = client.get_con_from_hdl(con_hdl)->get_ec();
            websocket_exception exc(ec, build_error_msg(ec, "set_fail_handler"));
            m_connect_tce.set_exception(exc);

            m_state = CLOSED;
        });

        client.set_message_handler([this](websocketpp::connection_hdl, const websocketpp::config::asio_client::message_type::ptr &msg)
        {
            if (m_external_received_handler)
            {
                _ASSERTE(m_state >= CONNECTED && m_state < CLOSED);
                websocket_incoming_message incoming_msg;

                switch (msg->get_opcode())
                {
                case websocketpp::frame::opcode::binary:
                    incoming_msg.m_msg_type = websocket_message_type::binary_message;
                    break;
                case websocketpp::frame::opcode::text:
                    incoming_msg.m_msg_type = websocket_message_type::text_message;
                    break;
                default:
                    // Unknown message type. Since both websocketpp and our code use the RFC codes, we'll just pass it on to the user.
                    incoming_msg.m_msg_type = static_cast<websocket_message_type>(msg->get_opcode());
                    break;
                }

                // 'move' the payload into a container buffer to avoid any copies.
                auto &payload = msg->get_raw_payload();
                incoming_msg.m_body = concurrency::streams::container_buffer<std::string>(std::move(payload));

                m_external_received_handler(incoming_msg);
            }
        });

        client.set_close_handler([this](websocketpp::connection_hdl con_hdl)
        {
            _ASSERTE(m_state != CLOSED);

            if (m_external_closed_handler)
            {
                auto &client = m_client->client<WebsocketConfigType>();
                auto connection = client.get_con_from_hdl(con_hdl);

                const auto &ec = connection->get_ec();

                const auto& clientCloseCode = connection->get_local_close_code();
                const auto& clientReason = connection->get_local_close_reason();
                if (clientCloseCode != websocketpp::close::status::blank)
                {
                    const auto& serverCloseCode = connection->get_remote_close_code();
                    const auto& serverReason = connection->get_remote_close_reason();
                    m_external_closed_handler(static_cast<websocket_close_status>(serverCloseCode), utility::conversions::to_string_t(serverReason), ec);
                }
                else
                {
                    m_external_closed_handler(static_cast<websocket_close_status>(clientCloseCode), utility::conversions::to_string_t(clientReason), ec);
                }

            }
            m_close_tce.set();
            m_state = CLOSED;
        });

        // Get the connection handle to save for later, have to create temporary
        // because type erasure occurs with connection_hdl.
        websocketpp::lib::error_code ec;
        auto con = client.get_connection(utility::conversions::to_utf8string(m_uri.to_string()), ec);
        m_con = con;
        if (ec.value() != 0)
        {
            return pplx::task_from_exception<void>(websocket_exception(ec, build_error_msg(ec, "get_connection")));
        }

        // Add any request headers specified by the user.
        const auto & headers = m_config.headers();
        for (const auto & header : headers)
        {
            if (!utility::details::str_icmp(header.first, g_subProtocolHeader))
            {
                con->append_header(utility::conversions::to_utf8string(header.first), utility::conversions::to_utf8string(header.second));
            }
        }

        // Add any specified subprotocols.
        if (headers.has(g_subProtocolHeader))
        {
            const std::vector<utility::string_t> protocols = m_config.subprotocols();
            for (const auto & value : protocols)
            {
                con->add_subprotocol(utility::conversions::to_utf8string(value), ec);
                if (ec.value())
                {
                    return pplx::task_from_exception<void>(websocket_exception(ec, build_error_msg(ec, "add_subprotocol")));
                }
            }
        }

        m_state = CONNECTING;
        client.connect(con);
        m_thread = std::thread(&websocketpp::client<WebsocketConfigType>::run, &client);
        return pplx::create_task(m_connect_tce);
    }

    pplx::task<void> send(websocket_outgoing_message &msg)
    {
        if (!m_connect_tce._IsTriggered())
        {
            return pplx::task_from_exception<void>(websocket_exception("Client not connected."));
        }

        switch (msg.m_msg_type)
        {
        case websocket_message_type::text_message:
        case websocket_message_type::binary_message:
            break;
        default:
            return pplx::task_from_exception<void>(websocket_exception("Invalid message type"));
        }

        const auto length = msg.m_length;
        if (length == 0)
        {
            return pplx::task_from_exception<void>(websocket_exception("Cannot send empty message."));
        }
        if (length >= UINT_MAX && length != SIZE_MAX)
        {
            return pplx::task_from_exception<void>(websocket_exception("Message size too large. Ensure message length is less than UINT_MAX."));
        }

        {
            if (++m_num_sends == 1) // No sends in progress
            {
                // Start sending the message
                send_msg(msg);
            }
            else
            {
                // Only actually have to take the lock if touching the queue.
                std::lock_guard<std::mutex> lock(m_send_lock);
                m_outgoing_msg_queue.push(msg);
            }
        }

        return pplx::create_task(msg.body_sent());
    }

    void send_msg(websocket_outgoing_message &msg)
    {
        if (m_client->is_tls_client())
        {
            send_msg_impl<websocketpp::config::asio_tls_client>(msg);
        }
        else
        {
            send_msg_impl<websocketpp::config::asio_client>(msg);
        }
    }

    template <typename WebsocketClientType>
    void send_msg_impl(websocket_outgoing_message &msg)
    {
        auto this_client = this->shared_from_this();
        auto& is_buf = msg.m_body;
        auto length = msg.m_length;

        if (length == SIZE_MAX)
        {
            // This indicates we should determine the length automatically.
            if (is_buf.has_size())
            {
                // The user's stream knows how large it is -- there's no need to buffer.
                auto buf_sz = is_buf.size();
                if (buf_sz >= SIZE_MAX)
                {
                    msg.signal_body_sent(std::make_exception_ptr(websocket_exception("Cannot send messages larger than SIZE_MAX.")));
                    return;
                }
                length = static_cast<size_t>(buf_sz);
                // We have determined the length and can proceed normally.
            }
            else
            {
                // The stream needs to be buffered.
                auto is_buf_istream = is_buf.create_istream();
                msg.m_body = concurrency::streams::container_buffer<std::vector<uint8_t>>();
                is_buf_istream.read_to_end(msg.m_body).then([this_client, msg](pplx::task<size_t> t) mutable
                {
                    try
                    {
                        msg.m_length = t.get();
                        this_client->send_msg(msg);
                    }
                    catch (...)
                    {
                        msg.signal_body_sent(std::current_exception());
                    }
                });
                // We have postponed the call to send_msg() until after the data is buffered.
                return;
            }
        }

        // First try to acquire the data (Get a pointer to the next already allocated contiguous block of data)
        // If acquire succeeds, send the data over the socket connection, there is no copy of data from stream to temporary buffer.
        // If acquire fails, copy the data to a temporary buffer managed by sp_allocated and send it over the socket connection.
        std::shared_ptr<uint8_t> sp_allocated;
        size_t acquired_size = 0;
        uint8_t* ptr;
        auto read_task = pplx::task_from_result();
        bool acquired = is_buf.acquire(ptr, acquired_size);

        if (!acquired || acquired_size < length) // Stream does not support acquire or failed to acquire specified number of bytes
        {
            // If acquire did not return the required number of bytes, do not rely on its return value.
            if (acquired_size < length)
            {
                acquired = false;
                is_buf.release(ptr, 0);
            }

            // Allocate buffer to hold the data to be read from the stream.
            sp_allocated.reset(new uint8_t[length], [=](uint8_t *p) { delete [] p; });

            read_task = is_buf.getn(sp_allocated.get(), length).then([length](size_t bytes_read)
            {
                if (bytes_read != length)
                {
                    throw websocket_exception("Failed to read required length of data from the stream.");
                }
            });
        }
        else
        {
            // Acquire succeeded, assign the acquired pointer to sp_allocated. Use an empty custom destructor
            // so that the data is not released when sp_allocated goes out of scope. The streambuf will manage its memory.
            sp_allocated.reset(ptr, [](uint8_t *) {});
        }

        read_task.then([this_client, msg, sp_allocated, length]()
        {
            auto &client = this_client->m_client->client<WebsocketClientType>();
            websocketpp::lib::error_code ec;
            switch (msg.m_msg_type)
            {
            case websocket_message_type::text_message:
                client.send(
                    this_client->m_con,
                    sp_allocated.get(),
                    length,
                    websocketpp::frame::opcode::text,
                    ec);
                break;
            case websocket_message_type::binary_message:
                client.send(
                    this_client->m_con,
                    sp_allocated.get(),
                    length,
                    websocketpp::frame::opcode::binary,
                    ec);
                break;
            default:
                // This case should have already been filtered above.
                std::abort();
            }

            return ec;
        }).then([this_client, msg, is_buf, acquired, sp_allocated, length](pplx::task<websocketpp::lib::error_code> previousTask) mutable
        {
            std::exception_ptr eptr;
            try
            {
                // Catch exceptions from previous tasks, if any and convert it to websocket exception.
                const auto &ec = previousTask.get();
                if (ec.value() != 0)
                {
                    eptr = std::make_exception_ptr(websocket_exception(ec, build_error_msg(ec, "sending message")));
                }
            }
            catch (...)
            {
                eptr = std::current_exception();
            }

            if (acquired)
            {
                is_buf.release(sp_allocated.get(), length);
            }

            // Set the send_task_completion_event after calling release.
            if (eptr)
            {
                msg.signal_body_sent(eptr);
            }
            else
            {
                msg.signal_body_sent();
            }

            if (--this_client->m_num_sends > 0)
            {
                // Only hold the lock when actually touching the queue.
                websocket_outgoing_message next_msg;
                {
                    std::lock_guard<std::mutex> lock(this_client->m_send_lock);
                    next_msg = this_client->m_outgoing_msg_queue.front();
                    this_client->m_outgoing_msg_queue.pop();
                }
                this_client->send_msg(next_msg);
            }
        });
    }

    pplx::task<void> close()
    {
        return close(static_cast<websocket_close_status>(websocketpp::close::status::normal), U("going away"));
    }

    pplx::task<void> close(websocket_close_status status, const utility::string_t& reason)
    {
        if (m_client == nullptr) return pplx::task_from_result();

        if (m_client->is_tls_client())
        {
            return close_impl<websocketpp::config::asio_tls_client>(status, reason);
        }
        else
        {
            return close_impl<websocketpp::config::asio_client>(status, reason);
        }
    }

    template <typename WebsocketConfig>
    pplx::task<void> close_impl(websocket_close_status status, const utility::string_t& reason)
    {
        auto &client = m_client->client<WebsocketConfig>();

        if (m_state == CONNECTED)
        {
            m_state = CLOSING;

            websocketpp::lib::error_code ec;
            client.close(m_con, static_cast<websocketpp::close::status::value>(status), utility::conversions::to_utf8string(reason), ec);
            if (ec.value() != 0)
            {
                return pplx::task_from_exception<void>(ec.message());
            }

            return pplx::task<void>(m_close_tce);
        }

        return pplx::task_from_result();
    }

    void set_received_handler(const std::function<void(const websocket_incoming_message&)>& handler)
    {
        m_external_received_handler = handler;
    }

    void set_closed_handler(const std::function<void(websocket_close_status, const utility::string_t&, const std::error_code&)>& handler)
    {
        m_external_closed_handler = handler;
    }

private:
    std::thread m_thread;

    // Perform type erasure to set the websocketpp client in use at runtime
    // after construction based on the URI.
    struct websocketpp_client_base
    {
        virtual ~websocketpp_client_base() CPPREST_NOEXCEPT {}
        template <typename WebsocketConfig>
        websocketpp::client<WebsocketConfig> & client()
        {
            if(is_tls_client())
            {
                return reinterpret_cast<websocketpp::client<WebsocketConfig> &>(tls_client());
            }
            else
            {
                return reinterpret_cast<websocketpp::client<WebsocketConfig> &>(non_tls_client());
            }
        }
        virtual websocketpp::client<websocketpp::config::asio_client> & non_tls_client()
        {
            throw std::bad_cast();
        }
        virtual websocketpp::client<websocketpp::config::asio_tls_client> & tls_client()
        {
            throw std::bad_cast();
        }
        virtual bool is_tls_client() const = 0;
    };
    struct websocketpp_client : websocketpp_client_base
    {
        websocketpp::client<websocketpp::config::asio_client> & non_tls_client() override
        {
            return m_client;
        }
        bool is_tls_client() const override { return false; }
        websocketpp::client<websocketpp::config::asio_client> m_client;
    };
    struct websocketpp_tls_client : websocketpp_client_base
    {
        websocketpp::client<websocketpp::config::asio_tls_client> & tls_client() override
        {
            return m_client;
        }
        bool is_tls_client() const override { return true; }
        websocketpp::client<websocketpp::config::asio_tls_client> m_client;
    };
    std::unique_ptr<websocketpp_client_base> m_client;

    websocketpp::connection_hdl m_con;

    pplx::task_completion_event<void> m_connect_tce;
    pplx::task_completion_event<void> m_close_tce;

    State m_state;

    // Guards access to m_outgoing_msg_queue
    std::mutex m_send_lock;

    // Queue to order the sends
    std::queue<websocket_outgoing_message> m_outgoing_msg_queue;

    // Number of sends in progress and queued up.
    std::atomic<int> m_num_sends;

    // External callback for handling received and close event
    std::function<void(websocket_incoming_message)> m_external_received_handler;
    std::function<void(websocket_close_status, const utility::string_t&, std::error_code)> m_external_closed_handler;

    // Used to track if any of the OpenSSL server certificate verifications
    // failed. This can safely be tracked at the client level since connections
    // only happen once for each client.
#if defined(__APPLE__) || (defined(ANDROID) || defined(__ANDROID__)) || defined(_MS_WINDOWS)
    bool m_openssl_failed;
#endif

};

websocket_client_task_impl::websocket_client_task_impl(websocket_client_config config) :
m_callback_client(std::make_shared<details::wspp_callback_client>(std::move(config))),
m_client_closed(false)
{
    set_handler();
}

}

websocket_callback_client::websocket_callback_client() :
    m_client(std::make_shared<details::wspp_callback_client>(websocket_client_config()))
{}

websocket_callback_client::websocket_callback_client(websocket_client_config config) :
m_client(std::make_shared<details::wspp_callback_client>(std::move(config)))
{}

}}}

#endif // !defined(WINAPI_FAMILY) || WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

namespace web
{
namespace websockets
{
namespace client
{
namespace details
{

websocket_client_task_impl::~websocket_client_task_impl()
{
    try
    {
        pplx::create_task(m_callback_client->close()).wait();
    }
    catch (...){}
}

void websocket_client_task_impl::set_handler()
{
    m_callback_client->set_received_handler([=](const websocket_incoming_message &msg)
    {
        pplx::task_completion_event<websocket_incoming_message> tce; // This will be set if there are any tasks waiting to receive a message
        {
            std::lock_guard<std::mutex> lock(m_receive_queue_lock);
            if (m_receive_task_queue.empty()) // Push message to the queue as no one is waiting to receive
            {
                m_receive_msg_queue.push(msg);
                return; 
            }
            else // There are tasks waiting to receive a message.
            {
                tce = m_receive_task_queue.front();
                m_receive_task_queue.pop();
            }
        }
        // Setting the tce outside the receive lock for better performance
        tce.set(msg);
    });

    m_callback_client->set_closed_handler([=](websocket_close_status status, const utility::string_t& reason, std::error_code error_code)
    {
        UNREFERENCED_PARAMETER(status);
        close_pending_tasks_with_error(websocket_exception(error_code, reason));
    });
}

void websocket_client_task_impl::close_pending_tasks_with_error(const websocket_exception &exc)
{
    std::lock_guard<std::mutex> lock(m_receive_queue_lock);
    m_client_closed = true;
    while (!m_receive_task_queue.empty()) // There are tasks waiting to receive a message, signal them
    {
        auto tce = m_receive_task_queue.front();
        m_receive_task_queue.pop();
        tce.set_exception(std::make_exception_ptr(exc));
    }
}

pplx::task<websocket_incoming_message> websocket_client_task_impl::receive()
{
    std::lock_guard<std::mutex> lock(m_receive_queue_lock);
    if (m_client_closed == true)
    {
        return pplx::task_from_exception<websocket_incoming_message>(std::make_exception_ptr(websocket_exception("Websocket connection has closed.")));
    }

    if (m_receive_msg_queue.empty()) // Push task completion event to the tce queue, so that it gets signaled when we have a message.
    {
        pplx::task_completion_event<websocket_incoming_message> tce;
        m_receive_task_queue.push(tce);
        return pplx::create_task(tce);
    }
    else // Receive message queue is not empty, return a message from the queue.
    {
        auto msg = m_receive_msg_queue.front();
        m_receive_msg_queue.pop();
        return pplx::task_from_result<websocket_incoming_message>(msg);
    }
}

}}}}

#endif //!defined(CPPREST_EXCLUDE_WEBSOCKETS)
