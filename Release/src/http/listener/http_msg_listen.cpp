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
* http_msg_listen.cpp
*
* HTTP Library: Request and reply message definitions (server side).
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

using namespace web;
using namespace utility;

namespace web { namespace http
{

// Actual initiates sending the response.
pplx::task<void> details::_http_request::_reply_impl(http_response response)
{
    // If the user didn't explicitly set a reason phrase then we should have it default
    // if they used one of the standard known status codes.
    if(response.reason_phrase().empty())
    {
        static http_status_to_phrase idToPhraseMap[] = {
#define _PHRASES
#define DAT(a,b,c) {status_codes::a, c},
#include "cpprest/http_constants.dat"
#undef _PHRASES
#undef DAT
        };

        for( auto iter = std::begin(idToPhraseMap); iter != std::end(idToPhraseMap); ++iter)
        {
            if( iter->id == response.status_code() )
            {
                response.set_reason_phrase(iter->phrase);
                break;
            }
        }
    }

    pplx::task<void> response_completed;

#if !defined(__cplusplus_winrt)
    auto server_api = experimental::details::http_server_api::server_api();

    if ( m_server_context && server_api )
    {
        // Add a task-based continuation so no exceptions thrown from the task go 'unobserved'.
        response._set_server_context(m_server_context);
        response_completed = experimental::details::http_server_api::server_api()->respond(response); 
        response_completed.then([](pplx::task<void> t)
        {
            try { t.wait(); } catch(...) {}
        });
    }
    else
#endif
    {
        // There's no server context. The message may be replied to locally, as in a HTTP client
        // pipeline stage. There's no sending required, so we can simply consider the reply
        // done and return an already filled-in task.
        response_completed = pplx::task_from_result();
    }

    m_response.set(response);
    return response_completed;
}

pplx::task<void> details::_http_request::_reply_if_not_already(status_code status)
{
    long expected = 0;
    long desired = 1;
    if (pplx::details::atomic_compare_exchange(m_initiated_response, desired, expected) == expected)
    {
        return _reply_impl(http_response(status));
    }
    return pplx::task_from_result();
}

pplx::task<void> details::_http_request::reply(http_response response)
{
    if(pplx::details::atomic_increment(m_initiated_response) != 1l)
    {
        throw http_exception(U("Error: trying to send multiple responses to an HTTP request"));
    }
    return _reply_impl(response);
}

}} // namespace web::http
