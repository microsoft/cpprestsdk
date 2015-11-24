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
* Websocket library: Client-side APIs.
*
* This file contains the websocket message implementation
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#if !defined(CPPREST_EXCLUDE_WEBSOCKETS)

using namespace concurrency;
using namespace concurrency::streams::details;

namespace web
{
namespace websockets
{
namespace client
{

static ::utility::string_t g_subProtocolHeader = _XPLATSTR("Sec-WebSocket-Protocol");
void websocket_client_config::add_subprotocol(const ::utility::string_t &name)
{
    m_headers.add(g_subProtocolHeader, name);
}

std::vector<::utility::string_t> websocket_client_config::subprotocols() const
{
    std::vector<::utility::string_t> values;
    auto subprotocolHeader = m_headers.find(g_subProtocolHeader);
    if (subprotocolHeader != m_headers.end())
    {
        utility::stringstream_t header(subprotocolHeader->second);
        utility::string_t token;
        while (std::getline(header, token, U(',')))
        {
            http::details::trim_whitespace(token);
            if (!token.empty())
            {
                values.push_back(token);
            }
        }
    }
    return values;
}

pplx::task<std::string> websocket_incoming_message::extract_string() const
{
    if (m_msg_type == websocket_message_type::binary_message)
    {
        return pplx::task_from_exception<std::string>(websocket_exception("Invalid message type"));
    }
    return pplx::task_from_result(std::move(m_body.collection()));
}

}}}

#endif
