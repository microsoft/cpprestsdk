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


using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;


//
// Token allowing your client to access an associated Dropbox account.
// Token is obtained following oauth2 authorization process.
// See Dropbox Core API for information on the oauth2 endpoints:
// https://www.dropbox.com/developers/core/docs
//
static const string_t s_dropbox_token(U(""));


static void dropbox_client()
{
    http_client_config config;
    config.set_oauth2(oauth2_config(s_dropbox_token));
    http_client api(U("https://api.dropbox.com/1/"), config);
    http_client content(U("https://api-content.dropbox.com/1/"), config);

    ucout << "Requesting account information:" << std::endl;
    ucout << "Information: " << api.request(methods::GET, U("account/info")).get().extract_json().get() << std::endl;
    ucout << "Requesting directory listing of sandbox '/':" << std::endl;
    ucout << "Listing: " << api.request(methods::GET, U("metadata/sandbox/")).get().extract_json().get() << std::endl;
    ucout << "Getting 'hello_world.txt' metadata:" << std::endl;
    ucout << "Metadata: " << api.request(methods::GET, U("metadata/sandbox/hello_world.txt")).get().extract_json().get() << std::endl;

    ucout << "Downloading 'hello_world.txt' file contents (text):" << std::endl;
    string_t content_string = content.request(methods::GET, "files/sandbox/hello_world.txt").get().extract_string().get();
    ucout << "Contents: '" << content_string << "'" << std::endl;
    ucout << "Downloading 'test_image.jpg' file contents (binary):" << std::endl;
    std::vector<unsigned char> content_vector = content.request(methods::GET, "files/sandbox/test_image.jpg").get().extract_vector().get();
    ucout << "Contents size: " << (content_vector.size() / 1024) << "KiB" << std::endl;

    ucout << "Uploading 'test_put.txt' file with contents 'Testing POST' (text):" << std::endl;
    ucout << "Response: "
            << content.request(methods::POST, "files_put/sandbox/test_put.txt", "Testing POST").get().extract_string().get()
            << std::endl;
    ucout << "Uploading 'test_image_copy.jpg' (copy of 'test_image.jpg'):" << std::endl;
    ucout << "Response: "
            << content.request(methods::PUT, "files_put/sandbox/test_image_copy.jpg",
                    concurrency::streams::bytestream::open_istream(std::move(content_vector))).get().extract_string().get()
            << std::endl;

    ucout << "Deleting uploaded file 'test_put.txt':" << std::endl;
    ucout << "Response: " << api.request(methods::POST, "fileops/delete?root=sandbox&path=test_put.txt").get().extract_string().get() << std::endl;
    ucout << "Deleting uploaded file 'test_image_copy.jpg':" << std::endl;
    ucout << "Response: " << api.request(methods::POST, "fileops/delete?root=sandbox&path=test_image_copy.jpg").get().extract_string().get() << std::endl;
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
    ucout << "Running oauth2 sample..." << std::endl;
    if (has_key(U("Dropbox"), s_dropbox_token))
    {
        dropbox_client();
    }
    ucout << "Done." << std::endl;
    return 0;
}

