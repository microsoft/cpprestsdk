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

*/
#include "stdafx.h"

#if defined(_WIN32) && !defined(__cplusplus_winrt)
// Extra includes for Windows desktop.
#include <windows.h>
#include <Shellapi.h>
#endif

#include "cpprest/http_listener.h"
#include "cpprest/http_client.h"
#include "cpprest/oauth2.h"

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::oauth2;
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

static const web::uri s_local_uri(U("http://localhost:8888/"));

//
// Utility method to open browser on Windows, OS X and Linux systems.
//
static void open_browser(utility::string_t auth_uri)
{
#if defined(_WIN32) && !defined(__cplusplus_winrt)
    auto r = ShellExecuteW(NULL, L"open", auth_uri.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    string_t browser_cmd(U("open \"") + auth_uri + U("\""));
    system(browser_cmd.c_str());
#else
    string_t browser_cmd(U("xdg-open \"") + auth_uri + U("\""));
    system(browser_cmd.c_str());
#endif
}

struct
{
    void set(const pplx::task_completion_event<web::uri>& tce)
    {
        std::lock_guard<std::mutex> lk(m_lock);
        m_tce = tce;
    }
    pplx::task_completion_event<web::uri> get()
    {
        std::lock_guard<std::mutex> lk(m_lock);
        return m_tce;
    }

private:
    pplx::task_completion_event<web::uri> m_tce;
    std::mutex m_lock;
} s_browser_uri;

struct local_redirect_listener
{
    local_redirect_listener()
        : m_listener(s_local_uri)
    {
        m_listener.support([](http_request req)
        {
            s_browser_uri.get().set(req.absolute_uri());

            req.reply(status_codes::OK);
        });
        m_listener.open().wait();
    }

    http_listener m_listener;
};

web::uri open_browser_callback(const web::uri& auth_uri)
{
    ucout << "Opening browser in URI:" << std::endl;
    ucout << auth_uri.to_string() << std::endl;

    pplx::task_completion_event<web::uri> tce;

    s_browser_uri.set(tce);
    open_browser(auth_uri.to_string());

    return pplx::create_task(tce).get();
}

void dropbox_session_sample()
{
    auto oauth_session = auth_code_grant_flow(
        s_dropbox_key,
        U("https://www.dropbox.com/1/oauth2/authorize"),
        s_local_uri);

    auto redirected = open_browser_callback(oauth_session.uri());

    http_client_config config;
    config.set_credentials(web::credentials(s_dropbox_key, s_dropbox_secret));
    http_client token_client(U("https://api.dropbox.com/1/oauth2/token"), config);

    oauth2_shared_token auth = oauth_session.complete(redirected, token_client).get();

    http_client api(U("https://api.dropbox.com/1/"));
    api.add_handler(auth.create_pipeline_stage());

    ucout << "Requesting account information:" << std::endl;
    ucout << "Information: " << api.request(methods::GET, U("account/info")).get().extract_json().get() << std::endl;
}

void linkedin_session_sample()
{
    http_client_config config;
    config.set_credentials(web::credentials(s_linkedin_key, s_linkedin_secret));

    http_client token_client(U("https://www.linkedin.com/uas/oauth2/accessToken"), config);

    auto flow = auth_code_grant_flow(
        s_linkedin_key,
        U("https://www.linkedin.com/uas/oauth2/authorization"),
        s_local_uri);
    auto redirected = open_browser_callback(flow.uri());
    auto auth = flow.complete(
        redirected,
        token_client,
        client_credentials_mode::request_body).get();

    http_client api(U("https://api.linkedin.com/v1/people/"));
    api.add_handler(auth.create_pipeline_stage(U("oauth2_access_token")));

    ucout << "Requesting account information:" << std::endl;
    ucout << "Information: " << api.request(methods::GET, U("~?format=json")).get().extract_json().get() << std::endl;
}

void live_session_sample()
{
    http_client_config config;
    config.set_credentials(web::credentials(s_live_key, s_live_secret));

    http_client token_client(U("https://login.live.com/oauth20_token.srf"), config);

    auto flow = auth_code_grant_flow(
        s_live_key,
        U("https://login.live.com/oauth20_authorize.srf"),
        s_local_uri,
        U("wl.basic"));
    auto redirected = open_browser_callback(flow.uri());
    auto auth = flow.complete(
        redirected,
        token_client).get();

    http_client api(U("https://apis.live.net/v5.0/"));
    api.add_handler(auth.create_pipeline_stage());

    ucout << "Requesting account information:" << std::endl;
    ucout << api.request(methods::GET, U("me")).get().extract_json().get() << std::endl;
}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    ucout << "Running OAuth 2.0 client sample..." << std::endl;
    local_redirect_listener listener;

    if (!s_linkedin_key.empty())
        linkedin_session_sample();
    if (!s_dropbox_key.empty())
        dropbox_session_sample();
    if (!s_live_key.empty())
        live_session_sample();

    if (s_linkedin_key.empty() && s_dropbox_key.empty() && s_live_key.empty())
    {
        ucout << "No client_id/client_secret pairs were set.\n"
            "Please change the hardcoded client information strings according the instructions in the Oauth2Client.cpp source file." << std::endl;
    }

    ucout << "Done." << std::endl;
    return 0;
}
