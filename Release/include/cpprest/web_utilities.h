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
* web_utilities.h
*
* utility classes used by the different web:: clients
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_WEB_UTILITIES_H
#define _CASA_WEB_UTILITIES_H

#include "cpprest/xxpublic.h"
#include "cpprest/asyncrt_utils.h"

namespace web
{
namespace http { namespace client {
    class web_proxy;
    class http_client_config;
}}

namespace experimental { namespace web_sockets { namespace client {
    class websocket_client_config;
}}}

/// <summary>
/// credentials represents a set of user credentials (username and password) to be used
/// for the client and proxy authentication
/// </summary>
class credentials
{
public:
    credentials(utility::string_t username, utility::string_t password) :
        m_is_set(true),
        m_username(std::move(username)),
        m_password(std::move(password))
    {}

    /// <summary>
    /// The user name associated with the credentials.
    /// </summary>
    /// <returns>A reference to the username string.</returns>
    const utility::string_t& username() const { return m_username; }

    /// <summary>
    /// The password for the user name associated with the credentials.
    /// </summary>
    /// <returns>A reference to the password string.</returns>
    const utility::string_t& password() const { return m_password; }

    /// <summary>
    /// Checks if credentials have been set
    /// </summary>
    /// <returns><c>true</c> if username and password is set, <c>false</c> otherwise.</returns>
    bool is_set() const { return m_is_set; }

private:
    friend class web::http::client::web_proxy;
    friend class web::http::client::http_client_config;
    friend class web::experimental::web_sockets::client::websocket_client_config;

    credentials() : m_is_set(false) {}

    utility::string_t m_username;
    utility::string_t m_password;
    bool m_is_set;
};
}

#endif