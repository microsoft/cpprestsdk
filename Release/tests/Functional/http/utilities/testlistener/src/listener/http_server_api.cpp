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
#include "http_server_api.h"

#ifdef _MS_WINDOWS
#include "http_windows_server.h"
#else
#include "http_linux_server.h"
#include "log.h"
#endif

using namespace web; using namespace utility;

namespace web { namespace http
{
namespace listener
{

pplx::extensibility::critical_section_t http_server_api::s_lock;

std::unique_ptr<http_server> http_server_api::s_server_api((http_server*)nullptr);

int http_server_api::s_registrations = 0;

bool http_server_api::has_listener() 
{ 
    return s_registrations > 0; 
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
        logging::log::post(logging::LOG_WARNING, U("Server API was cleared while listeners were still attached"));
    }

    s_server_api.release();
}

void http_server_api::unsafe_register_server_api(std::unique_ptr<http_server> server_api)
{
    // we assume that the lock has been taken here. 

    if (http_server_api::has_listener())
    {
        logging::log::post(logging::LOG_FATAL, U("Attempted to register a new server API while listening to requests"));
        throw http_exception(U("Current server API instance has listeners attached. Register a server API before listening to requests"));
    }

    s_server_api.swap(server_api);
}

unsigned long http_server_api::register_listener(_In_ http_listener_interface *listener)
{
    pplx::extensibility::scoped_critical_section_t lock(s_lock);
    unsigned long error_code = 0;

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

    // if nothing is registered yet, start the server.
    if (s_registrations == 0)
    {
        error_code = s_server_api->start();
        if (FAILED(error_code))
        {
            return error_code;
        }
    }

    // Register listener.
    error_code = s_server_api->register_listener(listener);
    if (FAILED(error_code))
    {
        return error_code;
    }

    ++s_registrations;
    return error_code;
}

unsigned long http_server_api::unregister_listener(_In_ http_listener_interface *pListener)
{
    pplx::extensibility::scoped_critical_section_t lock(s_lock);
    unsigned long error_code = 0;
    
    error_code = server_api()->unregister_listener(pListener);
    if (FAILED(error_code))
    {
        return error_code;
    }

    s_registrations--;
    if(s_registrations == 0)
    {
        error_code = server_api()->stop();
        if (FAILED(error_code))
        {
            return error_code;
        }
    }

    return error_code;
}

http_server *http_server_api::server_api() 
{ 
    return s_server_api.get(); 
}

} // namespace listener
}} // namespace web::http
