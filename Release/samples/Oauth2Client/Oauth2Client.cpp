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


class oauth2_authorizer
{
public:
    oauth2_authorizer(utility::string_t client_key, utility::string_t client_secret,
            utility::string_t auth_endpoint, utility::string_t token_endpoint,
            utility::string_t redirect_uri) :
                m_client_key(client_key),
                m_client_secret(client_secret),
                m_auth_endpoint(auth_endpoint),
                m_token_endpoint(token_endpoint),
                m_redirect_uri(redirect_uri)
    {
    }

    utility::string_t get_authorization_uri(utility::string_t state)
    {
        // TODO: Use "token" here for implicit flow. This would get token directly without auth code.
        const string_t response_type(U("code"));
        uri_builder u(m_auth_endpoint);
        u.append_query(U("response_type"), response_type);
        u.append_query(U("client_id"), m_client_key);
        u.append_query(U("redirect_uri"), m_redirect_uri);
        u.append_query(U("state"), state);

        // Note: this is only needed for Live client
        u.append_query(U("scope"), "wl.signin");

        return u.to_string();
    }

    pplx::task<utility::string_t> obtain_token(utility::string_t authorization_code, bool do_http_basic_auth=true)
    {
#if 0
        // FIXME: Windows http_client cannot determine HTTP auth type for some reason on some hosts.
        // We don't need to determine it since oauth2 uses HTTP basic auth by default.
        http_client_config config;
        if (do_http_basic_auth)
        {
            config.set_credentials(credentials(uri::encode_data_string(m_client_key), uri::encode_data_string(m_client_secret)));
        }
        http_client token_client(m_token_endpoint, config);

        utility::string_t request_body(U("grant_type=authorization_code&code=") + uri::encode_data_string(authorization_code)
                + U("&redirect_uri=") + uri::encode_data_string(m_redirect_uri));
        if (!do_http_basic_auth)
        {
            request_body += U("&client_id=") + uri::encode_data_string(m_client_key)
                + U("&client_secret=") + uri::encode_data_string(m_client_secret);
        }

        return token_client.request(methods::POST, U(""), request_body, mime_types::application_x_www_form_urlencoded)
#else
        http_client token_client(m_token_endpoint);
        http_request request;
        request.set_method(methods::POST);
        request.set_request_uri(U(""));

        utility::string_t request_body(U("grant_type=authorization_code&code=") + uri::encode_data_string(authorization_code)
                + U("&redirect_uri=") + uri::encode_data_string(m_redirect_uri));
        if (!do_http_basic_auth)
        {
            request_body += U("&client_id=") + uri::encode_data_string(m_client_key)
                + U("&client_secret=") + uri::encode_data_string(m_client_secret);
        }
        else
        {
            // Add HTTP basic auth header.
#if defined(_MS_WINDOWS)
            // On Windows we need to convert utf16 to utf8..
            std::string creds_str(conversions::utf16_to_utf8(
                uri::encode_data_string(m_client_key) + U(":") + uri::encode_data_string(m_client_secret)
            ));
#else
            utility::string_t creds_str(uri::encode_data_string(m_client_key) + U(":") + uri::encode_data_string(m_client_secret));
#endif
            std::vector<unsigned char> creds_vec(creds_str.c_str(), creds_str.c_str() + creds_str.length());
            request.headers().add(U("Authorization"), U("Basic ") + conversions::to_base64(std::move(creds_vec)));
        }
        request.set_body(request_body, mime_types::application_x_www_form_urlencoded);

        return token_client.request(request)  
#endif
        .then([](http_response response)
        {
            return response.extract_json();
        })
        .then([](json::value token_json)
        {
            ucout << token_json.serialize() << std::endl;
            return token_json[U("access_token")].as_string();
        });
    }

    const utility::string_t& redirect_uri() const { return m_redirect_uri; }

private:
    utility::string_t m_client_key;
    utility::string_t m_client_secret;
    utility::string_t m_auth_endpoint;
    utility::string_t m_token_endpoint;
    utility::string_t m_redirect_uri;
};

class oauth2_code_listener
{
public:
    oauth2_code_listener(uri listen_uri, utility::string_t state) :
        m_listener(new http_listener(listen_uri)),
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
                m_tce.set(U("FAIL"));
            }
            else
            {
                m_tce.set(code_query->second);
            }
            request.reply(status_codes::OK);
        });
        m_listener->open().wait();
    }

    ~oauth2_code_listener()
    {
        m_listener->close().wait();
        delete m_listener;
    }

    pplx::task<utility::string_t> listen_for_code()
    {
        return pplx::create_task(m_tce);
    }

private:
    http_listener *m_listener;
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
#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt) // Windows desktop
    auto r = ShellExecute(NULL, "open", conversions::utf16_to_utf8(auth_uri).c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(_MS_WINDOWS) && defined(__cplusplus_winrt) // Windows RT
#elif defined(__APPLE__)
#else
    // TODO: This is for Linux/X11 only.
    string_t browser_cmd(U("xdg-open \"") + auth_uri + U("\""));
    ucout << "Opening browser with following command:" << std::endl;
    ucout << browser_cmd << std::endl;
    system(browser_cmd.c_str());
#endif
}

static string_t dropbox_authorize()
{
    oauth2_authorizer authorizer(s_dropbox_key, s_dropbox_secret,
            U("https://www.dropbox.com/1/oauth2/authorize"),
            U("https://api.dropbox.com/1/oauth2/token"),
            U("http://localhost:8888/"));

    utility::string_t auth_code;
    {
        oauth2_code_listener listener(authorizer.redirect_uri(), s_state);
        open_browser(authorizer.get_authorization_uri(s_state));
        auth_code = listener.listen_for_code().get();
    }

    return authorizer.obtain_token(auth_code).get();
}

static string_t linkedin_authorize()
{
    oauth2_authorizer authorizer(s_linkedin_key, s_linkedin_secret,
            U("https://www.linkedin.com/uas/oauth2/authorization"),
            U("https://www.linkedin.com/uas/oauth2/accessToken"),
            U("http://localhost:8888/"));

    utility::string_t auth_code;
    {
        oauth2_code_listener listener(authorizer.redirect_uri(), s_state);
        open_browser(authorizer.get_authorization_uri(s_state));
        auth_code = listener.listen_for_code().get();
    }

    return authorizer.obtain_token(auth_code, false).get();
}

static string_t  live_authorize()
{
	oauth2_authorizer authorizer(s_live_key, s_live_secret,
		U("https://login.live.com/oauth20_authorize.srf"),
		U("https://login.live.com/oauth20_token.srf"),
		U("http://mydomain987564728.com:80/"));  // must be a real domain name

	utility::string_t auth_code;
	{
		oauth2_code_listener listener(authorizer.redirect_uri(), s_state);
		open_browser(authorizer.get_authorization_uri(s_state));
		auth_code = listener.listen_for_code().get();
	}

	return authorizer.obtain_token(auth_code).get();
}

static void dropbox_client()
{
    oauth2_config dropbox_cfg(dropbox_authorize());
//    ucout << "Dropbox token: " <<  dropbox_cfg.token() << std::endl;

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
    oauth2_config linkedin_cfg(linkedin_authorize());
    // LinkedIn doesn't use bearer auth header or the standard "access_token" key.
    linkedin_cfg.set_bearer_auth(false);
    linkedin_cfg.set_access_token_key(U("oauth2_access_token"));
//    ucout << "LinkedIn token: " <<  linkedin_cfg.token() << std::endl;

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
    oauth2_config live_cfg(live_authorize());

    http_client_config config;
    config.set_oauth2(live_cfg);

    http_client api(U("https://apis.live.net/v5.0"), config);
    auto x = api.request(methods::GET, U("me?access_token=wl.signin")).get();
}

#ifdef _MS_WINDOWS
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    ucout << "Running oauth2 sample..." << std::endl;

//    linkedin_client();
//    dropbox_client();
    live_client();

    ucout << "Done." << std::endl;
    return 0;
}

