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
* http_server_api.cpp
*
* HTTP Library: exposes the entry points to the http server transport apis.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/http_server_api.h"

#ifdef _MS_WINDOWS
#include "cpprest/http_windows_server.h"
#else
#include "cpprest/http_linux_server.h"
#include "cpprest/log.h"
#endif

namespace web { namespace http
{
namespace experimental {
namespace details
{

using namespace web;
using namespace utility;
using namespace utility::experimental;
using namespace web::http::experimental::listener;

pplx::extensibility::critical_section_t http_server_api::s_lock;

std::unique_ptr<http_server> http_server_api::s_server_api((http_server*)nullptr);

pplx::details::atomic_long http_server_api::s_registrations(0L);

bool http_server_api::has_listener() 
{ 
    return s_registrations > 0L; 
}

void http_server_api::register_server_api(std::unique_ptr<http_server> server_api)
{
    pplx::extensibility::scoped_critical_section_t lock(s_lock);
    http_server_api::unsafe_register_server_api(std::move(server_api));
}

void http_server_api::unregister_server_api()
{
    pplx::extensibility::scoped_critical_section_t lock(s_lock);

    if (http_server_api::has_listener())
    {
        throw http_exception(_XPLATSTR("Server API was cleared while listeners were still attached"));
    }

    s_server_api.release();
}

void http_server_api::unsafe_register_server_api(std::unique_ptr<http_server> server_api)
{
    // we assume that the lock has been taken here. 

    if (http_server_api::has_listener())
    {
        throw http_exception(_XPLATSTR("Current server API instance has listeners attached."));
    }

    s_server_api.swap(server_api);
}

pplx::task<void> http_server_api::register_listener(_In_ http_listener *listener)
{
    pplx::extensibility::scoped_critical_section_t lock(s_lock);

    // the server API was not initialized, register a default
    if(s_server_api == nullptr)
    {
#ifdef _MS_WINDOWS
        std::unique_ptr<http_windows_server> server_api(new http_windows_server());
#else
        std::unique_ptr<http_linux_server> server_api(new http_linux_server());
#endif
        http_server_api::unsafe_register_server_api(std::move(server_api));
    }

    auto increment = []() { pplx::details::atomic_increment(s_registrations); };
    auto regster = 
        [listener,increment] () -> pplx::task<void> { 
            // Register listener.
            return s_server_api->register_listener(listener).then(increment);
        };

    // if nothing is registered yet, start the server.
    if ( s_registrations == 0L )
    {
        return s_server_api->start().then(regster);
    }

    return regster();
}

pplx::task<void> http_server_api::unregister_listener(_In_ http_listener *pListener)
{
    pplx::extensibility::scoped_critical_section_t lock(s_lock);
    
    auto stop = 
        [] () -> pplx::task<void> {
            if ( pplx::details::atomic_decrement(s_registrations) == 0L )
            {
                return server_api()->stop();
            }
            return pplx::task_from_result();
        };
    
    return server_api()->unregister_listener(pListener).then(stop);
}

http_server *http_server_api::server_api() 
{ 
    return s_server_api.get(); 
}

} // namespace listener
} // namespace experimental
}} // namespace web::http
