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
#include <memory>
#include <thread>

#if (!defined(WINAPI_FAMILY) || WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) && !defined(_M_ARM)
#if defined(__GNUC__)
#include "pplx/threadpool.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
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
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#pragma warning( pop )
#endif /* __GNUC__ */

typedef websocketpp::client<websocketpp::config::asio_client> wsppclient;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using namespace Concurrency::streams::details;

namespace web
{
namespace experimental
{
namespace web_sockets
{
namespace client
{
namespace details
{

class ws_desktop_client : public _websocket_client_impl, public std::enable_shared_from_this<ws_desktop_client>
{
private:
    enum State {
        UNINITIALIZED,
        INITIALIZED,
        CONNECTING,
        CONNECTED,
        CLOSING,
        CLOSED,
        DESTROYED
    };
public:
    ws_desktop_client(web::uri address,
                      websocket_client_config client_config)
        : _websocket_client_impl(std::move(address), std::move(client_config)),
          m_work(new boost::asio::io_service::work(m_service)),
          m_state(UNINITIALIZED),
          m_scheduled(0)
        {
            verify_uri(m_uri);

            m_client.clear_access_channels(websocketpp::log::alevel::all);
            m_client.clear_error_channels(websocketpp::log::alevel::all);

            m_client.init_asio(&m_service);
            m_state = INITIALIZED;
        }

    ~ws_desktop_client()
    {
        _ASSERTE(m_state < DESTROYED);
        std::unique_lock<std::mutex> lock(m_receive_queue_lock);

        // First, trigger the boost::io_service to spin down
        m_work.reset();

        // Now, what states could we be in?
        switch (m_state) {
        case UNINITIALIZED:
        case DESTROYED:
            // These should be impossible
            std::abort();
        case INITIALIZED:
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

    pplx::task<void> close()
    {
        return close(static_cast<websocket_close_status>(websocketpp::close::status::normal), U("going away"));
    }
    pplx::task<void> close(websocket_close_status status, const utility::string_t& reason)
    {
        std::lock_guard<std::mutex> lock(m_receive_queue_lock);
        if (m_state == CONNECTED)
        {
            m_state = CLOSING;

            websocketpp::lib::error_code ec;
            m_client.close(m_con, static_cast<websocketpp::close::status::value>(status), utility::conversions::to_utf8string(reason), ec);
            if (ec.value() != 0)
            {
                websocket_exception wx(utility::conversions::to_string_t(ec.message()));
                return pplx::task_from_exception<void>(wx);
            }
        }
        return pplx::task<void>(m_close_tce);
    }

    pplx::task<void> connect()
    {
        _ASSERTE(m_state == INITIALIZED);
        m_client.set_open_handler([this](websocketpp::connection_hdl)
        {
            _ASSERTE(m_state == CONNECTING);
            m_state = CONNECTED;
            m_connect_tce.set();
        });

        m_client.set_fail_handler([this](websocketpp::connection_hdl)
        {
            _ASSERTE(m_state == CONNECTING);
            std::lock_guard<std::mutex> lock(m_receive_queue_lock);
            close_pending_tasks_with_error();
            m_state = CLOSED;
            m_connect_tce.set_exception(websocket_exception(_XPLATSTR("Connection attempt failed.")));
        });

        m_client.set_message_handler([this](websocketpp::connection_hdl, message_ptr msg)
        {
            _ASSERTE(m_state >= CONNECTED && m_state < CLOSED);
            websocket_incoming_message ws_incoming_message;
            auto& incmsg = ws_incoming_message._m_impl;
            incmsg->_prepare_to_receive_data();

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

            auto& writebuf = incmsg->streambuf();

            const std::string& payload = msg->get_payload();
            auto len = payload.size();
            auto block = writebuf.alloc(len);

            if (block == nullptr)
            {
                // Unable to allocate memory for receiving.
                std::abort();
            }
#ifdef _WIN32
            memcpy_s(block, payload.size(), payload.data(), payload.size());
#else
            std::copy(payload.begin(), payload.end(), block);
#endif

            writebuf.commit(len);
            writebuf.close(std::ios::out).wait();

            incmsg->set_length(len);
            incmsg->_set_data_available();

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

        m_client.set_close_handler([this](websocketpp::connection_hdl)
        {
            std::unique_lock<std::mutex> lock(m_receive_queue_lock);
            _ASSERTE(m_state != CLOSED);
            close_pending_tasks_with_error();
            m_close_tce.set();
            m_state = CLOSED;
        });


        websocketpp::lib::error_code ec;
        auto con = m_client.get_connection(utility::conversions::to_utf8string(m_uri.to_string()), ec);
        m_con = con;
        if (ec.value() != 0)
        {
            websocket_exception wx(utility::conversions::to_string_t(ec.message()));
            return pplx::task_from_exception<void>(wx);
        }

        m_state = CONNECTING;
        m_client.connect(con);

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
    pplx::task<void> send(websocket_outgoing_message msg)
    {
        if (!m_connect_tce._IsTriggered())
        {
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Client not connected.")));
        }

        switch (msg._m_impl->message_type())
        {
        case websocket_message_type::text_message:
        case websocket_message_type::binary_message:
            break;
        default:
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Invalid message type")));
        }

        const auto length = msg._m_impl->length();
        if (length == 0)
        {
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Cannot send empty message.")));
        }

        if (length > UINT_MAX)
        {
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Message size too large. Ensure message length is less than or equal to UINT_MAX.")));
        }

        {
            std::lock_guard<std::mutex> lock(m_send_lock);
            ++m_scheduled;
            if (m_scheduled == 1) // No sends in progress
            {
                // Start sending the message
                send_msg(msg);
            }
            else
            {
                m_outgoing_msg_queue.push(msg);
            }
        }

        return pplx::create_task(msg.m_send_tce);
    }

    pplx::task<websocket_incoming_message> receive()
    {
        std::lock_guard<std::mutex> lock(m_receive_queue_lock);
        if (m_state > CONNECTED)
        {
            // The client has already been closed.
            return pplx::task_from_exception<websocket_incoming_message>(std::make_exception_ptr(websocket_exception(_XPLATSTR("Websocket connection has closed."))));
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

    void send_msg(websocket_outgoing_message msg)
    {
        auto this_client = this->shared_from_this();
        auto& is_buf = msg._m_impl->streambuf();
        auto length = msg._m_impl->length();

        // First try to acquire the data (Get a pointer to the next already allocated contiguous block of data)
        // If acquire succeeds, send the data over the socket connection, there is no copy of data from stream to temporary buffer.
        // If acquire fails, copy the data to a temporary buffer managed by sp_allocated and send it over the socket connection.
        std::shared_ptr<uint8_t> sp_allocated(nullptr, [](uint8_t *) { } );
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
            sp_allocated.reset(new uint8_t[length](), [=](uint8_t *p ) { delete[] p; });

            read_task = is_buf.getn(sp_allocated.get(), length).then([length](size_t bytes_read)
            {
                if (bytes_read != length)
                {
                    throw websocket_exception(_XPLATSTR("Failed to read required length of data from the stream."));
                }
            });
        }
        else
        {
            // Acquire succeeded, assign the acquired pointer to sp_allocated. Keep an empty custom destructor 
            // so that the data is not released when sp_allocated goes out of scope. The streambuf will manage its memory.
            sp_allocated.reset(ptr, [](uint8_t *) {});
        }

        read_task.then([this_client, msg, sp_allocated, length]()
        {
            websocketpp::lib::error_code ec;
            switch (msg._m_impl->message_type())
            {
            case websocket_message_type::text_message:
                this_client->m_client.send(this_client->m_con,
                                           sp_allocated.get(),
                                           length,
                                           websocketpp::frame::opcode::text,
                                           ec);
                break;
            case websocket_message_type::binary_message:
                this_client->m_client.send(this_client->m_con,
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
        }).then([this_client, msg, acquired, sp_allocated, length](pplx::task<websocketpp::lib::error_code> previousTask)
        {
            std::exception_ptr eptr;
            try
            {
                // Catch exceptions from previous tasks, if any and convert it to websocket exception.
                auto ec = previousTask.get();
                if (ec.value() != 0)
                {
                    websocket_exception wx(utility::conversions::to_string_t(ec.message()));
                    eptr = std::make_exception_ptr(wx);
                }
            }
            catch (...)
            {
                eptr = std::current_exception();
            }

            if (acquired)
            {
                msg._m_impl->streambuf().release(sp_allocated.get(), length);
            }

            // Set the send_task_completion_event after calling release.
            if (eptr)
            {
                msg.m_send_tce.set_exception(eptr);
            }
            else
            {
                msg.m_send_tce.set();
            }

            std::unique_lock<std::mutex> lock(this_client->m_send_lock);
            --this_client->m_scheduled;
            if (!this_client->m_outgoing_msg_queue.empty())
            {
                auto next_msg = this_client->m_outgoing_msg_queue.front();
                this_client->m_outgoing_msg_queue.pop();
                this_client->send_msg(next_msg);
            }
        });
    }

    // Note: must be called while m_receive_queue_lock is locked.
    void close_pending_tasks_with_error()
    {
        while (!m_receive_task_queue.empty())
        {
            // There are tasks waiting to receive a message, signal them
            auto tce = m_receive_task_queue.front();
            m_receive_task_queue.pop();
            tce.set_exception(std::make_exception_ptr(websocket_exception(_XPLATSTR("Websocket connection has been closed."))));
        }
    }

private:
    // The m_service should be the first member (and therefore the last to be destroyed)
    boost::asio::io_service m_service;
    std::unique_ptr<boost::asio::io_service::work> m_work;
    std::thread m_thread;

    wsppclient m_client;
    websocketpp::connection_hdl m_con;

    pplx::task_completion_event<void> m_connect_tce;
    pplx::task_completion_event<void> m_close_tce;

    State m_state;

    // When a message arrives, if there are tasks waiting for a message, signal the topmost one.
    // Else enqueue the message in a queue.
    // m_receive_queue_lock : to guard access to the queue & m_client_closed
    // There is a bug in ppl task_completion_event. The task_completion_event::set() accesses some
    // internal data after signalling the event. The waiting thread might go ahead and start destroying the
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

    // Number of scheduled sends
    int m_scheduled;
};

}

websocket_client::websocket_client(web::uri base_uri)
    :m_client(std::make_shared<details::ws_desktop_client>(
                  std::move(base_uri), websocket_client_config()))
        {
        }

websocket_client::websocket_client(web::uri base_uri, websocket_client_config config)
    :m_client(std::make_shared<details::ws_desktop_client>(
                  std::move(base_uri), std::move(config)))
        {
        }

}}}}

#endif // !defined(WINAPI_FAMILY) || WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
