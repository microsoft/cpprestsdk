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
* ws_msg.cpp
*
* Websocket library: Client-side APIs.
* 
* This file contains the websocket message implementation
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "cpprest/producerconsumerstream.h"

#if WINAPI_FAMILY == WINAPI_FAMILY_APP

using namespace ::Windows::Foundation;
using namespace ::Windows::Storage;
using namespace ::Windows::Storage::Streams;
using namespace ::Windows::Networking;
using namespace ::Windows::Networking::Sockets;
using namespace concurrency;
using namespace concurrency::streams::details;

namespace web 
{ 
namespace experimental
{
namespace web_sockets
{
namespace client
{

void details::_websocket_message::set_body(streams::istream instream)
{
    set_instream(instream);
}

void details::_websocket_message::_prepare_to_receive_data()
{
    // The user did not specify an outstream.
    // We will create one...
    concurrency::streams::producer_consumer_buffer<uint8_t> buf;
    set_outstream(buf.create_ostream());

    // Since we are creating the streambuffer, set the input stream
    // so that the user can retrieve the data.
    set_instream(buf.create_istream());
}

std::string details::_websocket_message::_extract_string()
{
    auto buf_r = instream().streambuf();

    if (buf_r.in_avail() == 0)
    {
        return std::string();
    }

    std::string body;
    body.resize(static_cast<std::string::size_type>(buf_r.in_avail()));
    buf_r.getn(reinterpret_cast<uint8_t*>(&body[0]), body.size()).get();
    return body;
}

pplx::task<std::string> websocket_incoming_message::extract_string() const
{
    if (_m_impl->message_type() == websocket_message_type::binary_fragment || 
        _m_impl->message_type() == websocket_message_type::binary_message)
    {
        return pplx::task_from_exception<std::string>(websocket_exception(_XPLATSTR("Invalid message type")));
    }

    return pplx::create_task(_m_impl->_get_data_available()).then([=]() { return _m_impl->_extract_string(); });
}

}}}}

#endif /* WINAPI_FAMILY == WINAPI_FAMILY_APP */