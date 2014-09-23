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

#if (!defined(WINAPI_FAMILY) || WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) && !defined(_M_ARM) && (!defined(_MSC_VER) || (_MSC_VER < 1900))
#if defined(__GNUC__)
#include "pplx/threadpool.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#pragma GCC diagnostic pop
#else /* __GNUC__ */
#pragma warning( disable : 4503 )
#pragma warning( push )
#pragma warning( disable : 4100 4127 4996 4512 4996 4267 )
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#define _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_INITIALIZER_LISTS_
#define _WEBSOCKETPP_NOEXCEPT_TOKEN_
#define _WEBSOCKETPP_CONSTEXPR_TOKEN_
#else
#define _WEBSOCKETPP_NULLPTR_TOKEN_ 0
#endif
#ifndef _MS_WINDOWS
#include <websocketpp/config/asio_client.hpp>
#endif
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#pragma warning( pop )
#endif /* __GNUC__ */

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace web
{
namespace experimental
{
namespace websockets
{
namespace client
{
namespace details
{

static utility::string_t g_subProtocolHeader(_XPLATSTR("Sec-WebSocket-Protocol"));

class wspp_client : public _websocket_client_impl, public std::enable_shared_from_this<wspp_client>
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
    wspp_client(websocket_client_config config) :
        _websocket_client_impl(std::move(config)),
        m_work(utility::details::make_unique<boost::asio::io_service::work>(m_service)),
        m_state(CREATED),
        m_num_sends(0)
#if defined(__APPLE__) || defined(ANDROID)
        , m_openssl_failed(false)
#endif
    {}

    ~wspp_client()
    {
        _ASSERTE(m_state < DESTROYED);
        std::unique_lock<std::mutex> lock(m_receive_queue_lock);

        // First, trigger the boost::io_service to spin down
        m_work.reset();

        // Now, what states could we be in?
        switch (m_state) {
        case DESTROYED:
            // These should be impossible
            std::abort();
        case CREATED:
        case CLOSED:
            // In these cases, nothing need be done.
            lock.unlock();
            break;
        case CONNECTING:
        case CONNECTED:
        case CLOSING:
            // unlock the mutex so connect/close can use it
            lock.unlock();
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
        m_service.stop();
        if (m_thread.joinable())
            m_thread.join();

        // At this point, there should be no more references to me.
        m_state = DESTROYED;
    }

    pplx::task<void> connect()
    {
#ifndef _MS_WINDOWS
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
#if defined(__APPLE__) || defined(ANDROID)
                m_openssl_failed = false;
#endif
                sslContext->set_verify_callback([this](bool preverified, boost::asio::ssl::verify_context &verifyCtx)
                {
#if defined(__APPLE__) || defined(ANDROID)
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
                        return http::client::details::verify_cert_chain_platform_specific(verifyCtx, m_uri.host());
                    }
#endif
                    boost::asio::ssl::rfc2818_verification rfc2818(m_uri.host());
                    return rfc2818(preverified, verifyCtx);
                });

                return sslContext;
            });
            return connect_impl<websocketpp::config::asio_tls_client>();
        }
        else
#endif
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
        client.init_asio(&m_service);

        _ASSERTE(m_state == CREATED);
        client.set_open_handler([this](websocketpp::connection_hdl)
        {
            _ASSERTE(m_state == CONNECTING);
            m_state = CONNECTED;
            m_connect_tce.set();
        });

        client.set_fail_handler([this](websocketpp::connection_hdl)
        {
            _ASSERTE(m_state == CONNECTING);
            std::lock_guard<std::mutex> lock(m_receive_queue_lock);
            close_pending_tasks_with_error();
            m_state = CLOSED;
            m_connect_tce.set_exception(websocket_exception("Connection attempt failed."));
        });

        client.set_message_handler([this](websocketpp::connection_hdl, const websocketpp::config::asio_client::message_type::ptr &msg)
        {
            _ASSERTE(m_state >= CONNECTED && m_state < CLOSED);
            websocket_incoming_message ws_incoming_message;
            auto& incmsg = ws_incoming_message._m_impl;

            switch (msg->get_opcode())
            {
            case websocketpp::frame::opcode::binary:
                incmsg->set_msg_type(websocket_message_type::binary_message);
                break;
            case websocketpp::frame::opcode::text:
                incmsg->set_msg_type(websocket_message_type::text_message);
                break;
            default:
                // Unknown message type. Since both websocketpp and our code use the RFC codes, we'll just pass it on to the user.
                incmsg->set_msg_type(static_cast<websocket_message_type>(msg->get_opcode()));
                break;
            }

            // 'move' the payload into a container buffer to avoid any copies.
            auto &payload = msg->get_raw_payload();
            const auto len = payload.size();
            ws_incoming_message.m_body = concurrency::streams::container_buffer<std::string>(std::move(payload));
            incmsg->set_length(len);

            std::unique_lock<std::mutex> lock(m_receive_queue_lock);
            if (m_receive_task_queue.empty())
            {
                // Push message to the queue as no one is waiting to receive
                m_receive_msg_queue.push(ws_incoming_message);
                return;
            }
            else
            {
                // There are tasks waiting to receive a message.
                auto tce = m_receive_task_queue.front();
                m_receive_task_queue.pop();
                // Unlock the lock before setting the completion event to avoid contention
                lock.unlock();
                tce.set(ws_incoming_message);
            }
        });

        client.set_close_handler([this](websocketpp::connection_hdl)
        {
            std::unique_lock<std::mutex> lock(m_receive_queue_lock);
            _ASSERTE(m_state != CLOSED);
            close_pending_tasks_with_error();
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
            return pplx::task_from_exception<void>(websocket_exception(ec.message()));
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
                    return pplx::task_from_exception<void>(websocket_exception(ec.message()));
                }
            }
        }

        m_state = CONNECTING;
        client.connect(con);

        m_thread = std::thread([this]()
        {
            m_service.run();
            _ASSERTE(m_state == CLOSED);
            {
                std::unique_lock<std::mutex> lock(m_receive_queue_lock);
                close_pending_tasks_with_error();
            }
            _ASSERTE(m_state == CLOSED);
            return 0;
        });

        return pplx::create_task(m_connect_tce);
    }

    pplx::task<void> send(websocket_outgoing_message &msg)
    {
        if (!m_connect_tce._IsTriggered())
        {
            return pplx::task_from_exception<void>(websocket_exception("Client not connected."));
        }

        switch (msg._m_impl->message_type())
        {
        case websocket_message_type::text_message:
        case websocket_message_type::binary_message:
            break;
        default:
            return pplx::task_from_exception<void>(websocket_exception("Invalid message type"));
        }

        const auto length = msg._m_impl->length();
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

    pplx::task<websocket_incoming_message> receive()
    {
        std::lock_guard<std::mutex> lock(m_receive_queue_lock);
        if (m_state > CONNECTED)
        {
            // The client has already been closed.
            return pplx::task_from_exception<websocket_incoming_message>(std::make_exception_ptr(websocket_exception("Websocket connection has closed.")));
        }

        if (m_receive_msg_queue.empty())
        {
            // Push task completion event to the tce queue, so that it gets signaled when we have a message.
            pplx::task_completion_event<websocket_incoming_message> tce;
            m_receive_task_queue.push(tce);
            return pplx::create_task(tce);
        }
        else
        {
            // Receive message queue is not empty, return a message from the queue.
            auto& msg = m_receive_msg_queue.front();
            auto task = pplx::task_from_result<websocket_incoming_message>(std::move(msg));
            m_receive_msg_queue.pop();
            return task;
        }
    }

    void send_msg(websocket_outgoing_message &msg)
    {
#ifndef _MS_WINDOWS
        if (m_client->is_tls_client())
        {
            send_msg_impl<websocketpp::config::asio_tls_client>(msg);
        }
        else
#endif
        {
            send_msg_impl<websocketpp::config::asio_client>(msg);
        }
    }

    template <typename WebsocketClientType>
    void send_msg_impl(websocket_outgoing_message &msg)
    {
        auto this_client = this->shared_from_this();
        auto& is_buf = msg.m_body;
        auto length = msg._m_impl->length();

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
                        msg._m_impl->set_length(t.get());
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
            switch (msg._m_impl->message_type())
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
                auto ec = previousTask.get();
                if (ec.value() != 0)
                {
                    eptr = std::make_exception_ptr(websocket_exception(ec.message()));
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
#ifndef _MS_WINDOWS
        if (m_client->is_tls_client())
        {
            return close_impl<websocketpp::config::asio_tls_client>(status, reason);
        }
        else
#endif
        {
            return close_impl<websocketpp::config::asio_client>(status, reason);
        }
    }

    template <typename WebsocketConfig>
    pplx::task<void> close_impl(websocket_close_status status, const utility::string_t& reason)
    {
        auto &client = m_client->client<WebsocketConfig>();

        std::lock_guard<std::mutex> lock(m_receive_queue_lock);
        if (m_state == CONNECTED)
        {
            m_state = CLOSING;

            websocketpp::lib::error_code ec;
            client.close(m_con, static_cast<websocketpp::close::status::value>(status), utility::conversions::to_utf8string(reason), ec);
            if (ec.value() != 0)
            {
                return pplx::task_from_exception<void>(ec.message());
            }
        }
        return pplx::task<void>(m_close_tce);
    }

    // Note: must be called while m_receive_queue_lock is locked.
    void close_pending_tasks_with_error()
    {
        while (!m_receive_task_queue.empty())
        {
            // There are tasks waiting to receive a message, signal them
            auto tce = m_receive_task_queue.front();
            m_receive_task_queue.pop();
            tce.set_exception(std::make_exception_ptr(websocket_exception("Websocket connection has been closed.")));
        }
    }

private:
    // The m_service should be the first member (and therefore the last to be destroyed)
    boost::asio::io_service m_service;
    std::unique_ptr<boost::asio::io_service::work> m_work;
    std::thread m_thread;

    // Perform type erasure to set the websocketpp client in use at runtime
    // after construction based on the URI.
    struct websocketpp_client_base
    {
    	virtual ~websocketpp_client_base() _noexcept {}
    	template <typename WebsocketConfig>
    	websocketpp::client<WebsocketConfig> & client()
    	{
#ifndef _MS_WINDOWS
    		if(is_tls_client())
    		{
    			return reinterpret_cast<websocketpp::client<WebsocketConfig> &>(tls_client());
    		}
    		else
#endif
    		{
    			return reinterpret_cast<websocketpp::client<WebsocketConfig> &>(non_tls_client());
    		}
    	}
    	virtual websocketpp::client<websocketpp::config::asio_client> & non_tls_client()
		{
    		throw std::bad_cast();
		}
#ifndef _MS_WINDOWS
    	virtual websocketpp::client<websocketpp::config::asio_tls_client> & tls_client()
		{
    		throw std::bad_cast();
		}
#endif
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
#ifndef _MS_WINDOWS
    struct websocketpp_tls_client : websocketpp_client_base
    {
    	websocketpp::client<websocketpp::config::asio_tls_client> & tls_client() override
    	{
       		return m_client;
    	}
    	bool is_tls_client() const override { return true; }
    	websocketpp::client<websocketpp::config::asio_tls_client> m_client;
    };
#endif
    std::unique_ptr<websocketpp_client_base> m_client;

    websocketpp::connection_hdl m_con;

    pplx::task_completion_event<void> m_connect_tce;
    pplx::task_completion_event<void> m_close_tce;

    State m_state;

    // When a message arrives, if there are tasks waiting for a message, signal the topmost one.
    // Else enqueue the message in a queue.
    // m_receive_queue_lock : to guard access to the queue & m_client_closed
    // There is a bug in ppl task_completion_event. The task_completion_event::set() accesses some
    // internal data after signaling the event. The waiting thread might go ahead and start destroying the
    // websocket_client. Due to this race, set() can cause a crash.

    // Workaround: use m_receive_queue_lock as a critical section
    std::mutex m_receive_queue_lock;

    // Queue to store incoming messages when there are no tasks waiting for a message
    std::queue<websocket_incoming_message> m_receive_msg_queue;

    // Queue to maintain the receive tasks when there are no messages(yet).
    std::queue<pplx::task_completion_event<websocket_incoming_message>> m_receive_task_queue;

    // Guards access to m_outgoing_msg_queue
    std::mutex m_send_lock;

    // Queue to order the sends
    std::queue<websocket_outgoing_message> m_outgoing_msg_queue;

    // Number of sends in progress and queued up.
    std::atomic<int> m_num_sends;

    // Used to track if any of the OpenSSL server certificate verifications
    // failed. This can safely be tracked at the client level since connections
    // only happen once for each client.
#if defined(__APPLE__) || defined(ANDROID)
    bool m_openssl_failed;
#endif

};

}

websocket_client::websocket_client() :
    m_client(std::make_shared<details::wspp_client>(websocket_client_config()))
{}

websocket_client::websocket_client(websocket_client_config config) :
    m_client(std::make_shared<details::wspp_client>(std::move(config)))
{}

}}}}

#endif // !defined(WINAPI_FAMILY) || WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
