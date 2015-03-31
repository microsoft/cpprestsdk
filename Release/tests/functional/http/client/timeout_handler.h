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
* Simple utility for handling timeouts with http client test cases.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "cpprest/http_client.h"

namespace tests { namespace functional { namespace http { namespace client {

// helper function to check if failure is due to timeout.
inline bool is_timeout(const std::string &msg)
{
    if (msg.find("The operation timed out") != std::string::npos /* WinHTTP */ ||
        msg.find("The operation was timed out") != std::string::npos /* IXmlHttpRequest2 */)
    {
        return true;
    }
    return false;
}

template <typename Func>
void handle_timeout(const Func &f)
{
    try
    {
        f();
    }
    catch (const web::http::http_exception &e)
    {
        if (is_timeout(e.what()))
        {
            // Since this test depends on an outside server sometimes it sporadically can fail due to timeouts
            // especially on our build machines.
            return;
        }
        throw;
    }
}

}}}}