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
* Oauth1Client.cpp : Defines the entry point for the console application
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

/*

INSTRUCTIONS

This sample performs authorization on various OAuth 1.0 services and
then requests basic user information.

This sample is for Windows Desktop, OS X and Linux.
Execute with administrator privileges.

Set the app key & secret strings below (i.e. s_linkedin_key, s_linkedin_secret, etc.)
To get key & secret, register an app in the corresponding service.

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
// LinkedIn tokens. Fill the following based on information here:
// https://developer.linkedin.com/documents/quick-start-guide
//
static const string_t s_linkedin_key(U(""));
static const string_t s_linkedin_secret(U(""));

// TODO: to be removed
static const string_t s_linkedin_user_token(U(""));
static const string_t s_linkedin_user_token_secret(U(""));


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

#if 0
class oauth1_code_listener
{
public:
    oauth1_code_listener(
        uri listen_uri,
        oauth1_config& config) :
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
                catch (oauth1_exception& e)
                {
                    ucout << "Error: " << e.what() << std::endl;
                    request.reply(status_codes::NotFound, U("Not found."));
                }
            });
        });
        m_listener->open().wait();
    }

    ~oauth1_code_listener()
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
    oauth1_config& m_config;
    pplx::extensibility::critical_section_t m_resplock;
};
#endif


static void linkedin_client()
{
    http_client_config config;
    config.set_oauth1(oauth1_config(s_linkedin_key, s_linkedin_secret,
            oauth1_token(s_linkedin_user_token, s_linkedin_user_token_secret),
            oauth1_methods::hmac_sha1));
    http_client c(U("http://api.linkedin.com/"), config);

    ucout << "Requesting user information:" << std::endl;
    auto user_json = c.request(methods::GET, U("v1/people/~?format=json")).get().extract_json().get();
    ucout << "Got following JSON:" << std::endl;
    ucout << user_json.serialize().c_str() << std::endl;
}

static int has_key(string_t client_name, string_t key)
{
    if (key.empty())
    {
        ucout << "Skipped " << client_name.c_str() << " client. Please supply tokens for the client." << std::endl;
        return 0;
    }
    else
    {
        ucout << "Running " << client_name.c_str() << " client." << std::endl;
        return 1;
    }
}

#ifdef _MS_WINDOWS
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    ucout << "Running oauth1 sample..." << std::endl;
    if (has_key(U("LinkedIn"), s_linkedin_key))
    {
        linkedin_client();
    }
    ucout << "Done." << std::endl;
    return 0;
}

