/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* utility classes used by the different web:: clients
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "cpprest/asyncrt_utils.h"

namespace web
{
namespace details
{

#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
#if defined(__cplusplus_winrt)
class winrt_encryption
{
public:
    winrt_encryption() {}
    _ASYNCRTIMP winrt_encryption(const utility::wstring &data);
    _ASYNCRTIMP secure_unique_wstring decrypt() const;
private:
    ::pplx::task<Windows::Storage::Streams::IBuffer ^> m_buffer;
};
#else
class win32_encryption
{
public:
    win32_encryption() {}
    _ASYNCRTIMP win32_encryption(const utility::secure_wstring &data);
    _ASYNCRTIMP ~win32_encryption();
    _ASYNCRTIMP utility::secure_unique_wstring decrypt() const;
private:
    utility::vector<wchar_t, utility::secure_allocator<wchar_t>> m_buffer;
    size_t m_numCharacters;
};
#endif
#endif
}

/// <summary>
/// Represents a set of user credentials (user name and password) to be used
/// for authentication.
/// </summary>
class credentials
{
public:
    /// <summary>
    /// Constructs an empty set of credentials without a user name or password.
    /// </summary>
    credentials() {}

    /// <summary>
    /// Constructs credentials from given user name and password.
    /// </summary>
    /// <param name="username">User name as a string.</param>
    /// <param name="password">Password as a string.</param>
    credentials(utility::wstring username, const utility::secure_wstring &password) :
        m_username(std::move(username)),
        m_password(password)
    {}

    /// <summary>
    /// The user name associated with the credentials.
    /// </summary>
    /// <returns>A string containing the user name.</returns>
    const utility::wstring &username() const { return m_username; }

    /// <summary>
    /// The password for the user name associated with the credentials.
    /// </summary>
    /// <returns>A string containing the password.</returns>
    CASABLANCA_DEPRECATED("This API is deprecated for security reasons to avoid unnecessary password copies stored in plaintext.")
    utility::secure_unique_wstring password() const
    {
#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
        return m_password.decrypt();
#else
        return m_password;
#endif
    }

    /// <summary>
    /// Checks if credentials have been set
    /// </summary>
    /// <returns><c>true</c> if user name and password is set, <c>false</c> otherwise.</returns>
    bool is_set() const { return !m_username.empty(); }

    utility::secure_unique_wstring _internal_decrypt() const
    {
        // Encryption APIs not supported on XP
#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
        return m_password.decrypt();
#else
        return utility::make_unique<utility::secure_string_t, utility::secure_unique_ptr_deleter<utility::secure_string_t>>(m_password));
#endif
    }

    utility::secure_unique_string to_base64() {
        utility::wstring userpass = m_username + _XPLATSTR(":") + _internal_decrypt()->c_str();
        auto&& u8_userpass = utility::conversions::to_utf8string(userpass);
        utility::vector<unsigned char, utility::allocator<unsigned char>> credentials_buffer(u8_userpass.begin(), u8_userpass.end());
        return utility::make_unique<utility::secure_string, utility::secure_unique_ptr_deleter<utility::secure_string>>(utility::conversions::to_utf8string(utility::conversions::to_base64(credentials_buffer)).c_str());
    }

private:
    utility::wstring m_username;

#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
#if defined(__cplusplus_winrt)
    details::winrt_encryption m_password;
#else
    details::win32_encryption m_password;
#endif
#else
    utility::secure_wstring m_password;
#endif
};

/// <summary>
/// web_proxy represents the concept of the web proxy, which can be auto-discovered,
/// disabled, or specified explicitly by the user.
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
    web::uri m_address;
    web_proxy_mode_internal m_mode;
    web::credentials m_credentials;
};

}
