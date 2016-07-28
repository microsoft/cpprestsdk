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
* bingrequest.cpp - Simple cmd line application that makes an HTTP GET request to bing searching and outputting
*       the resulting HTML response body into a file.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace utility;
using namespace web::http;
using namespace web::http::client;
using namespace concurrency::streams;

/* Can pass proxy information via environment variable http_proxy.
   Example:
   Linux:   export http_proxy=http://192.1.8.1:8080
 */
web::http::client::http_client_config client_config_for_proxy()
{
    web::http::client::http_client_config client_config;

    if(const char* env_http_proxy = std::getenv("http_proxy")) {
        uri proxy_uri(utility::conversions::to_string_t(env_http_proxy));
        web::web_proxy proxy(proxy_uri);
        client_config.set_proxy(proxy);
    }

    return client_config;
}


#ifdef _WIN32
int wmain(int argc, wchar_t *args[])
#else
int main(int argc, char *args[])
#endif
{
    if(argc != 3)
    {
        printf("Usage: BingRequest.exe search_term output_file\n");
        return -1;
    }
    const string_t searchTerm = args[1];
    const string_t outputFileName = args[2];

    // Open a stream to the file to write the HTTP response body into.
    auto fileBuffer = std::make_shared<streambuf<uint8_t>>();
    file_buffer<uint8_t>::open(outputFileName, std::ios::out).then([=](streambuf<uint8_t> outFile) -> pplx::task<http_response>
    {
        *fileBuffer = outFile; 

        // Create an HTTP request.
        // Encode the URI query since it could contain special characters like spaces.
        http_client client(U("http://www.bing.com/"), client_config_for_proxy());
        return client.request(methods::GET, uri_builder(U("/search")).append_query(U("q"), searchTerm).to_string());
    })

    // Write the response body into the file buffer.
    .then([=](http_response response) -> pplx::task<size_t>
    {
        printf("Response status code %u returned.\n", response.status_code());

        return response.body().read_to_end(*fileBuffer);
    })

    // Close the file buffer.
    .then([=](size_t)
    {
        return fileBuffer->close();
    })

    // Wait for the entire response body to be written into the file.
    .wait();

    return 0;
}
