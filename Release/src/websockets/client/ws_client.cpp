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
* Portions common to both WinRT and Websocket++ implementations.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#if !defined(CPPREST_EXCLUDE_WEBSOCKETS)

namespace web
{
namespace websockets
{
namespace client
{
namespace details
{

websocket_client_task_impl::~websocket_client_task_impl() CPPREST_NOEXCEPT
{
    close_pending_tasks_with_error(websocket_exception("Websocket client is being destroyed"));
}

void websocket_client_task_impl::set_handler()
{
    m_callback_client->set_message_handler([=](const websocket_incoming_message &msg)
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

    m_callback_client->set_close_handler([=](websocket_close_status status, const utility::string_t& reason, const std::error_code& error_code)
    {
        CASABLANCA_UNREFERENCED_PARAMETER(status);
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

#endif