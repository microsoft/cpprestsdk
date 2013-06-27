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
* http_listen.cpp
*
* HTTP Library: HTTP listener (server-side) APIs
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/http_server_api.h"

namespace web { namespace http
{
namespace experimental {
namespace listener
{

using namespace web::http::experimental::details;

http_listener::http_listener(const http::uri &address) : m_uri(address), m_closed(true)
{
    m_pipeline = http::http_pipeline::create_pipeline(std::make_shared<_listener_stage>(this));

    // Somethings like proper URI schema are verified by the URI class.
    // We only need to check certain things specific to HTTP.
    if(m_uri.scheme() == U("https"))
    {
        throw std::invalid_argument("Listeners using 'https' are not yet supported");
    }

    if(m_uri.scheme() != U("http"))
    {
        throw std::invalid_argument("URI scheme must be 'http'");
    }

    if(m_uri.host().empty())
    {
        throw std::invalid_argument("URI must contain a hostname.");
    }

    if(!m_uri.query().empty())
    {
        throw std::invalid_argument("URI can't contain a query.");
    }

    if(!m_uri.fragment().empty())
    {
        throw std::invalid_argument("URI can't contain a fragment.");
    }
}

http_listener::~http_listener()
{
    close().wait();
}

pplx::task<void> http_listener::open()
{
    // Do nothing if the open operation was already attempted
    // Not thread safe
    if (!m_closed) return pplx::task_from_result();

    if ( m_uri.is_empty() )
        throw std::invalid_argument("No URI defined for listener.");
    m_closed = false;
    return http_server_api::register_listener(this);
}

pplx::task<void> http_listener::close()
{
    // Do nothing if the close operation was already attempted
    // Not thread safe.
    if (m_closed) return pplx::task_from_result();

    m_closed = true;
    return http_server_api::unregister_listener(this);
}

pplx::task<http_response> http_listener::handle_request(http_request msg)
{
    return m_pipeline->propagate(msg);
}

pplx::task<http_response> http_listener::dispatch_request(http_request msg)
{
    // Specific method handler takes priority over general.
    const method &mtd = msg.method();
    if(m_supported_methods.count(mtd))
    {
        m_supported_methods[mtd](msg);
    }
    else if(mtd == methods::OPTIONS)
    {
        handle_options(msg);
    }
    else if(mtd == methods::TRCE)
    {
        handle_trace(msg);
    }
    else if(m_all_requests != nullptr)
    {
        m_all_requests(msg);
    }
    else
    {
        // Method is not supported. 
        // Send back a list of supported methods to the client.
        http_response response(status_codes::MethodNotAllowed);
        response.headers().add(U("Allow"), get_supported_methods());
        msg.reply(response);
    }

    return msg.get_response();
}

utility::string_t http_listener::get_supported_methods() const 
{
    utility::string_t allowed;
    bool first = true;
    for (auto iter = m_supported_methods.begin(); iter != m_supported_methods.end(); ++iter)
    {
        if(!first)
        {
            allowed += U(", ");
        }
        else
        {
            first = false;
        }
        allowed += (iter->first);
    }
    return allowed;
}

void http_listener::handle_trace(http_request message)
{
    utility::string_t data = message.to_string();
    message.reply(status_codes::OK, data, U("message/http"));
}

void http_listener::handle_options(http_request message)
{
    http_response response(status_codes::OK);
    response.headers().add(U("Allow"), get_supported_methods());
    message.reply(response);
}

} // namespace listener
} // namespace experimental
}} // namespace web::http
