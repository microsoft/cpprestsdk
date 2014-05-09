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

#include "stdafx.h"
#include <thread>
#include <chrono>

#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt) // Windows desktop
#include <windows.h>
#include <Shellapi.h>
#endif

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::details;
using namespace web::http::client;

#include "cpprest/http_listener.h"
#include "cpprest/http_helpers.h"
using namespace web::http::experimental::listener;

// Set to 1 to run an extensive client sample.
#define EXTENSIVE 0


class oauth2_code_listener
{
public:
    oauth2_code_listener(uri listen_uri, utility::string_t state) :
        m_listener(utility::details::make_unique<http_listener>(listen_uri)),
        m_state(state)
    {
        m_listener->support([&](http::http_request request) -> void
        {
            auto query = uri::split_query(request.request_uri().query());
            auto code_query = query.find(U("code"));
            auto state_query = query.find(U("state"));
            if ((code_query == query.end()) || (state_query == query.end())
                    || (code_query->second.empty()) || (state_query->second.empty())
                    || (m_state != state_query->second))
            {
                request.reply(status_codes::NotFound, U("Not found."));
            }
            else
            {
                request.reply(status_codes::OK, U("Ok."));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                m_tce.set(code_query->second);
            }
        });
        m_listener->open().wait();
    }

    ~oauth2_code_listener()
    {
        m_listener->close().wait();
    }

    pplx::task<utility::string_t> listen_for_code()
    {
        return pplx::create_task(m_tce);
    }

private:
    std::unique_ptr<http_listener> m_listener;
    pplx::task_completion_event<utility::string_t> m_tce;
    utility::string_t m_state;
};


static const utility::string_t s_dropbox_key(U(""));
static const utility::string_t s_dropbox_secret(U(""));

static const utility::string_t s_linkedin_key(U(""));
static const utility::string_t s_linkedin_secret(U(""));

static const utility::string_t s_facebook_key(U(""));
static const utility::string_t s_facebook_secret(U(""));

static const utility::string_t s_live_key(U(""));
static const utility::string_t s_live_secret(U(""));

// TODO: Generate state per authorization request?
static const utility::string_t s_state(U("1234ABCD"));


static void open_browser(utility::string_t auth_uri)
{
    ucout << "Opening browser in URI:" << std::endl;
    ucout << auth_uri << std::endl;
#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt) // Windows desktop
    auto r = ShellExecute(NULL, "open", conversions::utf16_to_utf8(auth_uri).c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(_MS_WINDOWS) && defined(__cplusplus_winrt) // Windows RT
#elif defined(__APPLE__)
#else
    // TODO: This is for Linux/X11 only.
    string_t browser_cmd(U("xdg-open \"") + auth_uri + U("\""));
    system(browser_cmd.c_str());
#endif
}

static string_t code_from_localhost_listener(oauth2_config cfg)
{
    utility::string_t auth_code;
    {
#if 1
        oauth2_code_listener listener(cfg.redirect_uri(), s_state);
#else
        uri_builder ub(cfg.redirect_uri());
        ub.set_host(U("localhost"));
        oauth2_code_listener listener(ub.to_uri(), s_state);
#endif
        open_browser(cfg.build_authorization_uri(s_state));
        auth_code = listener.listen_for_code().get();
    }
    return auth_code;
}

static void dropbox_client()
{
    oauth2_config dropbox_cfg(s_dropbox_key, s_dropbox_secret,
            U("https://www.dropbox.com/1/oauth2/authorize"),
            U("https://api.dropbox.com/1/oauth2/token"),
            U("http://localhost:8889/"));
    dropbox_cfg.fetch_token(code_from_localhost_listener(dropbox_cfg)).wait();

    http_client_config config;
    config.set_oauth2(dropbox_cfg);
    http_client api(U("https://api.dropbox.com/1/"), config);
    http_client content(U("https://api-content.dropbox.com/1/"), config);

    ucout << "Requesting account information:" << std::endl;
    ucout << "Information: " << api.request(methods::GET, U("account/info")).get().extract_json().get() << std::endl;
#if EXTENSIVE
    ucout << "Requesting directory listing of sandbox '/':" << std::endl;
    ucout << "Listing: " << api.request(methods::GET, U("metadata/sandbox/")).get().extract_json().get() << std::endl;
    ucout << "Getting 'hello_world.txt' metadata:" << std::endl;
    ucout << "Metadata: " << api.request(methods::GET, U("metadata/sandbox/hello_world.txt")).get().extract_json().get() << std::endl;

    ucout << "Downloading 'hello_world.txt' file contents (text):" << std::endl;
    string_t content_string = content.request(methods::GET, U("files/sandbox/hello_world.txt")).get().extract_string().get();
    ucout << "Contents: '" << content_string << "'" << std::endl;
    ucout << "Downloading 'test_image.jpg' file contents (binary):" << std::endl;
    std::vector<unsigned char> content_vector = content.request(methods::GET, U("files/sandbox/test_image.jpg")).get().extract_vector().get();
    ucout << "Contents size: " << (content_vector.size() / 1024) << "KiB" << std::endl;

    ucout << "Uploading 'test_put.txt' file with contents 'Testing POST' (text):" << std::endl;
    ucout << "Response: "
            << content.request(methods::POST, U("files_put/sandbox/test_put.txt"), U("Testing POST")).get().extract_string().get()
            << std::endl;
    ucout << "Uploading 'test_image_copy.jpg' (copy of 'test_image.jpg'):" << std::endl;
    ucout << "Response: "
            << content.request(methods::PUT, U("files_put/sandbox/test_image_copy.jpg"),
                    concurrency::streams::bytestream::open_istream(std::move(content_vector))).get().extract_string().get()
            << std::endl;

    ucout << "Deleting uploaded file 'test_put.txt':" << std::endl;
    ucout << "Response: " << api.request(methods::POST, U("fileops/delete?root=sandbox&path=test_put.txt")).get().extract_string().get() << std::endl;
    ucout << "Deleting uploaded file 'test_image_copy.jpg':" << std::endl;
    ucout << "Response: " << api.request(methods::POST, U("fileops/delete?root=sandbox&path=test_image_copy.jpg")).get().extract_string().get() << std::endl;
#endif
}

static void linkedin_client()
{
    oauth2_config linkedin_cfg(s_linkedin_key, s_linkedin_secret,
            U("https://www.linkedin.com/uas/oauth2/authorization"),
            U("https://www.linkedin.com/uas/oauth2/accessToken"),
            U("http://localhost:8888/"));
    // LinkedIn doesn't use bearer auth.
    linkedin_cfg.set_bearer_auth(false);
    // It also uses "oauth2_access_token" instead of the normal "access_token" key.
    linkedin_cfg.set_access_token_key(U("oauth2_access_token"));

    linkedin_cfg.fetch_token(code_from_localhost_listener(linkedin_cfg), false).wait();

    http_client_config config;
    config.set_oauth2(linkedin_cfg);
    http_client api(U("https://api.linkedin.com/v1/people/"), config);

    ucout << "Requesting account information:" << std::endl;
    ucout << "Information: " << api.request(methods::GET, U("~?format=json")).get().extract_json().get() << std::endl;
#if EXTENSIVE
#endif
}

static void live_client()
{
    oauth2_config live_cfg(s_live_key, s_live_secret,
        U("https://login.live.com/oauth20_authorize.srf"),
        U("https://login.live.com/oauth20_token.srf"),
        // Live can't use localhost redirect, so we map localhost to a fake domain name:
        // 127.0.0.1    testhost.local
        U("http://testhost.local:8890/"));
    // Scope "wl.basic" allows getting user information.
    live_cfg.set_scope(U("wl.basic"));
    live_cfg.fetch_token(code_from_localhost_listener(live_cfg)).wait();

    http_client_config config;
    config.set_oauth2(live_cfg);

    http_client api(U("https://apis.live.net/v5.0/"), config);
    ucout << "Requesting account information:" << std::endl;
    ucout << api.request(methods::GET, U("me")).get().extract_json().get() << std::endl;
}

#ifdef _MS_WINDOWS
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    ucout << "Running oauth2 sample..." << std::endl;

    linkedin_client();
    dropbox_client();
    live_client();

    ucout << "Done." << std::endl;
    return 0;
}

