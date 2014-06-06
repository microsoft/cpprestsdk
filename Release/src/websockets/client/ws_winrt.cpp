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
* ws_winrt.cpp
*
* Websocket library: Client-side APIs.
* 
* This file contains the implementation for Windows 8 
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include <concrt.h>

#if WINAPI_FAMILY == WINAPI_FAMILY_APP

using namespace ::Windows::Foundation;
using namespace ::Windows::Storage;
using namespace ::Windows::Storage::Streams;
using namespace ::Windows::Networking;
using namespace ::Windows::Networking::Sockets;
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

// This class is required by the implementation in order to function:
// The TypedEventHandler requires the message received and close handler to be a member of WinRT class.
ref class ReceiveContext sealed
{
public:
    ReceiveContext() {}

    friend class winrt_client;
    void OnReceive(MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args);
    void OnClosed(IWebSocket^ sender, WebSocketClosedEventArgs^ args);

private:
    // Public members cannot have native types
    ReceiveContext(std::function<void(std::shared_ptr<websocket_incoming_message>)> receive_handler, std::function<void()> close_handler): m_receive_handler(receive_handler), m_close_handler(close_handler) {}

    // Handler to be executed when a message has been received by the client
    std::function<void(std::shared_ptr<websocket_incoming_message>)> m_receive_handler;

    // Handler to be executed when a close message has been received by the client
    std::function<void()> m_close_handler;
};

class winrt_client : public _websocket_client_impl, public std::enable_shared_from_this<winrt_client>
{
public:
    winrt_client(web::uri address, websocket_client_config client_config)
        : _websocket_client_impl(std::move(address), std::move(client_config)), m_scheduled(0), m_client_closed(false)
    {
        verify_uri(m_uri);
        m_msg_websocket = ref new MessageWebSocket();

        // Sets the HTTP request headers to the HTTP request message used in the WebSocket protocol handshake 
        for (auto iter = client_config.headers().begin(); iter != client_config.headers().end(); ++iter)
        {
            Platform::String^ name = ref new Platform::String(iter->first.c_str());
            Platform::String^ val = ref new Platform::String(iter->second.c_str());
            m_msg_websocket->SetRequestHeader(name, val);
        }

        if (client_config.credentials().is_set())
        {
            m_msg_websocket->Control->ServerCredential = ref new Windows::Security::Credentials::PasswordCredential("WebSocketClientCredentialResource", 
                ref new Platform::String(client_config.credentials().username().c_str()), 
                ref new Platform::String(client_config.credentials().password().c_str()));
        }

        m_context = ref new ReceiveContext([=](std::shared_ptr<websocket_incoming_message> msg)
        {
            _ASSERTE(msg != nullptr);
            pplx::task_completion_event<websocket_incoming_message> tce; // This will be set if there are any tasks waiting to receive a message
            {
                std::unique_lock<std::mutex> lock(m_receive_queue_lock);
                if (m_receive_task_queue.empty()) // Push message to the queue as no one is waiting to receive
                {
                    m_receive_msg_queue.push(std::move(*msg));
                    return;
                }
                else // There are tasks waiting to receive a message.
                {
                    tce = m_receive_task_queue.front();
                    m_receive_task_queue.pop();
                }
            }
            // Setting the tce outside the receive lock for better performance
            tce.set(*msg);
        }, 
        [=]() // Close handler called upon receiving a close frame from the server.
        {
            close_pending_tasks_with_error();
            m_close_tce.set();
            m_server_close_complete.set();
        }); 
    }

    ~winrt_client()
    {
        // task_completion_event::set() returns false if it has already been set.
        // In that case, wait on the m_server_close_complete event for the tce::set() to complete.
        // The websocket client on close handler (upon receiving close frame from server) will 
        // set this event.
        // If we have not received a close frame from the server, this set will be a no-op as the 
        // websocket_client is anyways destructing.
        if (!m_close_tce.set())
        {
            m_server_close_complete.wait(); 
        }
        else
        {
            close_pending_tasks_with_error();
        }
    }

    void close_pending_tasks_with_error()
    {
        std::unique_lock<std::mutex> lock(m_receive_queue_lock);
        m_client_closed = true;
        while(!m_receive_task_queue.empty()) // There are tasks waiting to receive a message, signal them
        {
            auto tce = m_receive_task_queue.front();
            m_receive_task_queue.pop();
            tce.set_exception(std::make_exception_ptr(websocket_exception(_XPLATSTR("Websocket connection has been closed."))));
        }
    }

    pplx::task<void> connect()
    {
        const auto &proxy = config().proxy();
        if(!proxy.is_default())
        {
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Only a default proxy server is supported.")));
        }

        const auto &proxy_cred = proxy.credentials();
        if(proxy_cred.is_set())
        {
            m_msg_websocket->Control->ProxyCredential = ref new Windows::Security::Credentials::PasswordCredential("WebSocketClientProxyCredentialResource", 
                ref new Platform::String(proxy_cred.username().c_str()), 
                ref new Platform::String(proxy_cred.password().c_str()));
        }

        const auto uri = ref new Windows::Foundation::Uri(ref new Platform::String(m_uri.to_string().c_str()));

        m_msg_websocket->MessageReceived += ref new TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>(m_context, &ReceiveContext::OnReceive);
        m_msg_websocket->Closed += ref new TypedEventHandler<IWebSocket^, WebSocketClosedEventArgs^>(m_context, &ReceiveContext::OnClosed);

        return pplx::create_task(m_msg_websocket->ConnectAsync(uri)).then([=](pplx::task<void> result) -> pplx::task<void>
        {
            try
            {
                result.get();
                m_messageWriter = ref new DataWriter(m_msg_websocket->OutputStream);
            }
            catch(Platform::Exception^ ex)
            {
                close_pending_tasks_with_error();
                return pplx::task_from_exception<void>(websocket_exception(ex->HResult));
            }
            return pplx::task_from_result();
        });
    }

    pplx::task<void> send(websocket_outgoing_message msg)
    {
        if (m_messageWriter == nullptr)
        {
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Client not connected.")));
        }

        switch(msg._m_impl->message_type())
        {
        case websocket_message_type::binary_message :
            m_msg_websocket->Control->MessageType = SocketMessageType::Binary;
            break;
        case websocket_message_type::text_message:
            m_msg_websocket->Control->MessageType = SocketMessageType::Utf8;
            break;
        default:
            return pplx::task_from_exception<void>(websocket_exception(_XPLATSTR("Message Type not supported.")));
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
            std::unique_lock<std::mutex> lock(m_send_lock);
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

        read_task.then([this_client, acquired, sp_allocated, length]()
        {
            this_client->m_messageWriter->WriteBytes(Platform::ArrayReference<unsigned char>(sp_allocated.get(), static_cast<unsigned int>(length)));

            // Send the data as one complete message, in WinRT we do not have an option to send fragments.
            return pplx::task<unsigned int>(this_client->m_messageWriter->StoreAsync());
        }).then([this_client, msg, acquired, sp_allocated, length] (pplx::task<unsigned int> previousTask)
        {
            std::exception_ptr eptr;
            unsigned int bytes_written = 0;
            try
            { 
                // Catch exceptions from previous tasks, if any and convert it to websocket exception.
                bytes_written = previousTask.get();
                if (bytes_written != length)
                {
                    eptr = std::make_exception_ptr(websocket_exception(_XPLATSTR("Failed to send all the bytes.")));
                }
            }
            catch (const websocket_exception& ex)
            {
                eptr = std::make_exception_ptr(ex);
            }
            catch (...)
            {
                eptr = std::make_exception_ptr(websocket_exception(_XPLATSTR("Failed to send data over the websocket connection.")));
            }

            if (acquired)
            {
                msg._m_impl->streambuf().release(sp_allocated.get(), bytes_written);
            }

            // Set the send_task_completion_event after calling release.
            if (eptr)
            {
                msg.m_send_tce.set_exception(eptr);
            }

            {
                std::unique_lock<std::mutex> lock(this_client->m_send_lock);
                --this_client->m_scheduled;
                if (!this_client->m_outgoing_msg_queue.empty())
                {
                    auto next_msg = this_client->m_outgoing_msg_queue.front();
                    this_client->m_outgoing_msg_queue.pop();
                    this_client->send_msg(next_msg);
                }
            }
            msg.m_send_tce.set();
        });
    }

    pplx::task<websocket_incoming_message> receive()
    {
        std::unique_lock<std::mutex> lock(m_receive_queue_lock);
        if (m_client_closed == true)
        {
            return pplx::task_from_exception<websocket_incoming_message>(std::make_exception_ptr(websocket_exception(_XPLATSTR("Websocket connection has closed."))));
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

    pplx::task<void> close()
    {
        // Send a close frame to the server
        return close(websocket_close_status::normal);
    }

    pplx::task<void> close(websocket_close_status status, const utility::string_t &strreason=_XPLATSTR("")) 
    {
        // Send a close frame to the server 
        Platform::String^ reason = ref new Platform::String(strreason.data());
        m_msg_websocket->Close(static_cast<unsigned short>(status), reason);
        // Wait for the close response frame from the server.
        return pplx::create_task(m_close_tce);
    }

    static std::shared_ptr<details::_websocket_message> get_impl(websocket_incoming_message msg)
    {
        return msg._m_impl;
    }

private:

    // WinRT MessageWebSocket object
    Windows::Networking::Sockets::MessageWebSocket^ m_msg_websocket;
    Windows::Storage::Streams::DataWriter^ m_messageWriter;
    // Context object that implements the WinRT handlers: receive handler and close handler
    ReceiveContext ^ m_context;

    pplx::task_completion_event<void> m_close_tce;
    // There is a bug in ppl task_completion_event. The task_completion_event::set() accesses some 
    // internal data after signalling the event. The waiting thread might go ahead and start destroying the 
    // websocket_client. Due to this race, set() can cause a crash.
    // To workaround this bug, maintain another event: m_server_close_complete. We will signal this when the m_close_tce.set() has 
    // completed. The websocket_client destructor can wait on this event before proceeding.
    Concurrency::event m_server_close_complete;

    // m_client_closed maintains the state of the client. It is set to true when:
    // 1. the client has not connected 
    // 2. if it has received a close frame from the server.
    // We may want to keep an enum to maintain the client state in the future.
    bool m_client_closed;

    // When a message arrives, if there are tasks waiting for a message, signal the topmost one.
    // Else enqueue the message in a queue.
    // m_receive_queue_lock : to guard access to the queue & m_client_closed 
    std::mutex m_receive_queue_lock;
    // Queue to store incoming messages when there are no tasks waiting for a message
    std::queue<websocket_incoming_message> m_receive_msg_queue; 
    // Queue to maintain the receive tasks when there are no messages(yet).
    std::queue<pplx::task_completion_event<websocket_incoming_message>> m_receive_task_queue; 

    // The implementation has to ensure ordering of send requests
    std::mutex m_send_lock; 

    // Queue to order the sends
    std::queue<websocket_outgoing_message> m_outgoing_msg_queue;

    // Number of scheduled sends
    int m_scheduled; 
};

void ReceiveContext::OnReceive(MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args)
{
    websocket_incoming_message ws_incoming_message;
    auto msg = winrt_client::get_impl(ws_incoming_message);
    msg->_prepare_to_receive_data();

    switch(args->MessageType)
    {
    case SocketMessageType::Binary:
        msg->set_msg_type(websocket_message_type::binary_message);
        break;
    case SocketMessageType::Utf8:
        msg->set_msg_type(websocket_message_type::text_message);
        break;
    }

    auto& writebuf = msg->streambuf();

    try
    {
        DataReader^ reader = args->GetDataReader();
        auto len = reader->UnconsumedBufferLength;
        auto block = writebuf.alloc(len);
        reader->ReadBytes(Platform::ArrayReference<uint8>(block, len)); 
        writebuf.commit(len);
        writebuf.close(std::ios::out).wait(); // Since this is an in-memory stream, this call is not blocking.
        msg->set_length(len);
        msg->_set_data_available(); 
        m_receive_handler(std::make_shared<websocket_incoming_message>(ws_incoming_message));
    }
    catch(...)
    {
        // Swallow the exception for now. Following up on this with the WinRT team.
        // We can handle this more gracefully once we know the scenarios where DataReader operations can throw an exception:
        // When socket gets into a bad state
        // or only when we receive a valid message and message processing fails.
        // Tracking this on codeplex with : https://casablanca.codeplex.com/workitem/181
    }
}

void ReceiveContext::OnClosed(IWebSocket^ sender, WebSocketClosedEventArgs^ args)
{
    m_close_handler();
}
}

websocket_client::websocket_client(web::uri base_uri)
:m_client(std::make_shared<details::winrt_client>(std::move(base_uri), websocket_client_config()))
{
}

websocket_client::websocket_client(web::uri base_uri, websocket_client_config config)
:m_client(std::make_shared<details::winrt_client>(std::move(base_uri), std::move(config)))
{
}

}}}}
#endif /* WINAPI_FAMILY == WINAPI_FAMILY_APP */
