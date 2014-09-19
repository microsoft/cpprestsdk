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
    class web_proxy;
namespace http { namespace client {
    class http_client_config;
}}

namespace experimental { namespace websockets { namespace client {
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
    friend class web::web_proxy;
    friend class web::http::client::http_client_config;
    friend class web::experimental::websockets::client::websocket_client_config;

    credentials() : m_is_set(false) {}

    bool m_is_set;
    utility::string_t m_username;
    utility::string_t m_password;
};

/// <summary>
/// web_proxy represents the concept of the web proxy, which can be auto-discovered,
/// disabled, or specified explicitly by the user
/// </summary>
class web_proxy
{
    enum web_proxy_mode_internal{ use_default_, use_auto_discovery_, disabled_, user_provided_ };
public:
    enum web_proxy_mode{ use_default = use_default_, use_auto_discovery = use_auto_discovery_, disabled  = disabled_};

    /// <summary>
    /// Constructs a proxy with the default settings.
    /// </summary>
    web_proxy() : m_address(_XPLATSTR("")), m_mode(use_default_) {}
    
    /// <summary>
    /// Creates a proxy with specified mode.
    /// </summary>
    /// <param name="mode">Mode to use.</param>
    web_proxy( web_proxy_mode mode ) : m_address(_XPLATSTR("")), m_mode(static_cast<web_proxy_mode_internal>(mode)) {}
    
    /// <summary>
    /// Creates a proxy explicitly with provided address.
    /// </summary>
    /// <param name="address">Proxy URI to use.</param>
    web_proxy( uri address ) : m_address(address), m_mode(user_provided_) {}

    /// <summary>
    /// Gets this proxy's URI address. Returns an empty URI if not explicitly set by user.
    /// </summary>
    /// <returns>A reference to this proxy's URI.</returns>
    const uri& address() const { return m_address; }

    /// <summary>
    /// Gets the credentials used for authentication with this proxy.
    /// </summary>
    /// <returns>Credentials to for this proxy.</returns>
    const web::credentials& credentials() const { return m_credentials; }
    
    /// <summary>
    /// Sets the credentials to use for authentication with this proxy.
    /// </summary>
    /// <param name="cred">Credentials to use for this proxy.</param>
    void set_credentials(web::credentials cred) {
        if( m_mode == disabled_ )
        {
            throw std::invalid_argument("Cannot attach credentials to a disabled proxy");
        }
        m_credentials = std::move(cred);
    }

    /// <summary>
    /// Checks if this proxy was constructed with default settings.
    /// </summary>
    /// <returns>True if default, false otherwise.</param>
    bool is_default() const { return m_mode == use_default_; }

    /// <summary>
    /// Checks if using a proxy is disabled.
    /// </summary>
    /// <returns>True if disabled, false otherwise.</returns>
    bool is_disabled() const { return m_mode == disabled_; }

    /// <summary>
    /// Checks if the auto discovery protocol, WPAD, is to be used.
    /// </summary>
    /// <returns>True if auto discovery enabled, false otherwise.</returns>
    bool is_auto_discovery() const { return m_mode == use_auto_discovery_; }
    
    /// <summary>
    /// Checks if a proxy address is explicitly specified by the user.
    /// </summary>
    /// <returns>True if a proxy address was explicitly specified, false otherwise.</returns> 
    bool is_specified() const { return m_mode == user_provided_; }

private:
    uri m_address;
    web_proxy_mode_internal m_mode;
    web::credentials m_credentials;
};

}

#endif
