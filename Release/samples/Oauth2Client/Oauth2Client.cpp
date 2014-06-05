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
* Oauth2Client.cpp : Defines the entry point for the console application
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

/*

INSTRUCTIONS

This sample performs authorization code grant flow on various OAuth 2.0
services and then requests basic user information.

This sample is for Windows Desktop, OS X and Linux.
Execute with administrator privileges.

Set the app key & secret strings below (i.e. s_dropbox_key, s_dropbox_secret, etc.)
To get key & secret, register an app in the corresponding service.

Set following entry in the hosts file:
127.0.0.1    testhost.local

*/
#include "stdafx.h"

#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt)
// Extra includes for Windows desktop.
#include <windows.h>
#include <Shellapi.h>
#endif

#include "cpprest/http_listener.h"
#include "cpprest/http_helpers.h"

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::details;
using namespace web::http::client;
using namespace web::http::experimental::listener; 


//
// Set key & secret pair to enable session for that service.
//
static const utility::string_t s_dropbox_key(U(""));
static const utility::string_t s_dropbox_secret(U(""));

static const utility::string_t s_linkedin_key(U(""));
static const utility::string_t s_linkedin_secret(U(""));

static const utility::string_t s_live_key(U(""));
static const utility::string_t s_live_secret(U(""));


static void open_browser(utility::string_t auth_uri)
{
#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt)
    // NOTE: Windows desktop only.
    auto r = ShellExecute(NULL, "open", conversions::utf16_to_utf8(auth_uri).c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    // NOTE: OS X only.
    string_t browser_cmd(U("open \"") + auth_uri + U("\""));
    system(browser_cmd.c_str());
#else
    // NOTE: Linux/X11 only.
    string_t browser_cmd(U("xdg-open \"") + auth_uri + U("\""));
    system(browser_cmd.c_str());
#endif
}


class oauth2_code_listener
{
public:
    oauth2_code_listener(
        uri listen_uri,
        oauth2_config& config) :
            m_listener(utility::details::make_unique<http_listener>(listen_uri)),
            m_config(config)
    {
        m_listener->support([this](http::http_request request) -> void
        {
            pplx::extensibility::scoped_critical_section_t lck(m_resplock);
            m_config.token_from_redirected_uri(request.request_uri()).then([this,request](pplx::task<void> token_task) -> void
            {
                try
                {
                        token_task.wait();
                        request.reply(status_codes::OK, U("Ok."));
                        m_tce.set();
                }
                catch (oauth2_exception& e)
                {
                    ucout << "Error: " << e.what() << std::endl;
                    request.reply(status_codes::NotFound, U("Not found."));
                }
            });
        });
        m_listener->open().wait();
    }

    ~oauth2_code_listener()
    {
        m_listener->close().wait();
    }

    pplx::task<void> listen_for_code()
    {
        return pplx::create_task(m_tce);
    }

private:
    std::unique_ptr<http_listener> m_listener;
    pplx::task_completion_event<void> m_tce;
    oauth2_config& m_config;
    pplx::extensibility::critical_section_t m_resplock;
};


class oauth2_session_sample
{
public:
    oauth2_session_sample(utility::string_t name,
        utility::string_t client_key,
        utility::string_t client_secret,
        utility::string_t auth_endpoint,
        utility::string_t token_endpoint,
        utility::string_t redirect_uri) :
            m_oauth2_config(client_key,
                client_secret,
                auth_endpoint,
                token_endpoint,
                redirect_uri),
            m_name(name)
    {
#if defined(_MS_WINDOWS) || defined(__APPLE__)
        m_listener = utility::details::make_unique<oauth2_code_listener>(redirect_uri, m_oauth2_config);
#else
        uri_builder ub(redirect_uri);
        ub.set_host(U("localhost"));
        m_listener = utility::details::make_unique<oauth2_code_listener>(ub.to_uri(), m_oauth2_config);
#endif
    }

    void run()
    {
        if (is_enabled())
        {
            ucout << "Running " << m_name.c_str() << " session..." << std::endl;

            if (m_oauth2_config.token().access_token().empty())
            {
                authorization_code_flow().wait();
                m_http_config.set_oauth2(m_oauth2_config);
            }

            run_internal();
        }
        else
        {
            ucout << "Skipped " << m_name.c_str() << " session sample because app key or secret is empty. Please see instructions." << std::endl;
        }
    }

protected:
    virtual void run_internal() = 0;

    pplx::task<void> authorization_code_flow()
    {
        open_browser_auth();
        return m_listener->listen_for_code();
    }

    http_client_config m_http_config;
    oauth2_config m_oauth2_config;

private:
    bool is_enabled() const
    {
        return !m_oauth2_config.client_key().empty() && !m_oauth2_config.client_secret().empty();
    }

    void open_browser_auth()
    {
        auto auth_uri(m_oauth2_config.build_authorization_uri());
        ucout << "Opening browser in URI:" << std::endl;
        ucout << auth_uri << std::endl;
        open_browser(auth_uri);
    }

    utility::string_t m_name;
    std::unique_ptr<oauth2_code_listener> m_listener;
};


class dropbox_session_sample : public oauth2_session_sample
{
public:
    dropbox_session_sample() :
        oauth2_session_sample(U("Dropbox"),
            s_dropbox_key,
            s_dropbox_secret,
            U("https://www.dropbox.com/1/oauth2/authorize"),
            U("https://api.dropbox.com/1/oauth2/token"),
            U("http://localhost:8889/"))
    {
    }

protected:
    void run_internal() override
    {
        http_client api(U("https://api.dropbox.com/1/"), m_http_config);
        ucout << "Requesting account information:" << std::endl;
        ucout << "Information: " << api.request(methods::GET, U("account/info")).get().extract_json().get() << std::endl;
    }
};


class linkedin_session_sample : public oauth2_session_sample
{
public:
    linkedin_session_sample() :
        oauth2_session_sample(U("LinkedIn"),
            s_linkedin_key,
            s_linkedin_secret,
            U("https://www.linkedin.com/uas/oauth2/authorization"),
            U("https://www.linkedin.com/uas/oauth2/accessToken"),
            U("http://localhost:8888/"))
    {
        // LinkedIn doesn't use bearer auth.
        m_oauth2_config.set_bearer_auth(false);
        // Also doesn't use HTTP Basic for token endpoint authentication.
        m_oauth2_config.set_http_basic_auth(false);
        // Also doesn't use the common "access_token", but "oauth2_access_token".
        m_oauth2_config.set_access_token_key(U("oauth2_access_token"));
    }

protected:
    void run_internal() override
    {
        http_client api(U("https://api.linkedin.com/v1/people/"), m_http_config);
        ucout << "Requesting account information:" << std::endl;
        ucout << "Information: " << api.request(methods::GET, U("~?format=json")).get().extract_json().get() << std::endl;
    }

};


class live_session_sample : public oauth2_session_sample
{
public:
    live_session_sample() :
        oauth2_session_sample(U("Live"),
            s_live_key,
            s_live_secret,
            U("https://login.live.com/oauth20_authorize.srf"),
            U("https://login.live.com/oauth20_token.srf"),
            U("http://testhost.local:8890/"))
    {
        // Scope "wl.basic" allows getting user information.
        m_oauth2_config.set_scope(U("wl.basic"));
    }

protected:
    void run_internal() override
    {
        http_client api(U("https://apis.live.net/v5.0/"), m_http_config);
        ucout << "Requesting account information:" << std::endl;
        ucout << api.request(methods::GET, U("me")).get().extract_json().get() << std::endl;
    }
};


#ifdef _MS_WINDOWS
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    ucout << "Running OAuth 2.0 client sample..." << std::endl;

    linkedin_session_sample linkedin;
    dropbox_session_sample  dropbox;
    live_session_sample     live;

    linkedin.run();
    dropbox.run();
    live.run();

    ucout << "Done." << std::endl;
    return 0;
}
