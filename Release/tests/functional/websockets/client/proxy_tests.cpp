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
* proxy_tests.cpp
*
* Tests cases for covering proxies using websocket_client
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(__cplusplus_winrt) || !defined(_M_ARM)

using namespace web::websockets;
using namespace web::websockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(proxy_tests)
{

#ifdef __cplusplus_winrt
TEST_FIXTURE(uri_address, no_proxy_options_on_winrt)
{
    websocket_client_config config;
    config.set_proxy(web::web_proxy::use_auto_discovery);
    websocket_client client(config);
    VERIFY_THROWS(client.connect(m_uri).wait(), websocket_exception);
}
#endif

#ifndef __cplusplus_winrt
// Can't specify a proxy with WinRT implementation.
TEST_FIXTURE(uri_address, proxy_with_credentials, "Ignore:Android", "390")
{
    web::web_proxy proxy(U("http://netproxy.redmond.corp.microsoft.com"));
    web::credentials cred(U("artur"), U("fred")); // relax, this is not my real password
    proxy.set_credentials(cred);
    websocket_client_config config;
    config.set_proxy(proxy);

    websocket_client client(config);

    try
    {
        client.connect(U("wss://echo.websocket.org/")).wait();
        const auto text = std::string("hello");
        websocket_outgoing_message msg;
        msg.set_utf8_message(text);
        client.send(msg).wait();
        auto response = client.receive().get();
        VERIFY_ARE_EQUAL(text, response.extract_string().get());
        client.close().wait();
    }
    catch (websocket_exception const& e)
    {
        if (e.error_code().value() == 12007)
        {
            // The above "netproxy.redmond.corp.microsoft.com" is an internal site not generally accessible.
            // This will cause a failure to resolve the URL.
            // This is ok.
            return;
        }
        else if (e.error_code().value() == 9 || e.error_code().value() == 5)
        {
            // Timer expired case, since this is an outside test don't fail due to timing out.
            return;
        }
        throw;
    }
}
#endif

} // SUITE(proxy_tests)

}}}}

#endif
