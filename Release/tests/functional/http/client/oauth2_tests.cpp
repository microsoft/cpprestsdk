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
* Test cases for oauth2.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/details/http_helpers.h"
#include "cpprest/oauth2.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::details;
using namespace web::http::oauth2;
using namespace utility;
using namespace concurrency;

using namespace tests::functional::http::utilities;
extern utility::string_t _to_base64(const unsigned char *ptr, size_t size);

namespace tests { namespace functional { namespace http { namespace client
{

SUITE(oauth2_tests)
{

TEST(oauth2_token_accessors)
{
    oauth2_token t;
    VERIFY_IS_FALSE(t.is_valid_access_token());

    auto access = U("b%20456");
    auto refresh = U("a%123");
    auto type = U("b%20456");
    auto scope = U("ad.ww xyz");
    auto expires = 123;

    t.set_access_token(access);
    t.set_refresh_token(refresh);
    t.set_token_type(type);
    t.set_scope(scope);
    t.set_expires_in(expires);

    VERIFY_IS_TRUE(t.is_valid_access_token());
    VERIFY_ARE_EQUAL(access, t.access_token());
    VERIFY_ARE_EQUAL(refresh, t.refresh_token());
    VERIFY_ARE_EQUAL(type, t.token_type());
    VERIFY_ARE_EQUAL(scope, t.scope());
    VERIFY_ARE_EQUAL(expires, t.expires_in());
}

TEST(oauth2_shared_token_accessors)
{
    oauth2_token tok(U("accesstoken"));
    tok.set_scope(U("wl.basic"));

    oauth2_shared_token stok(tok);
    VERIFY_ARE_EQUAL(U("accesstoken"), stok.token().access_token());
    VERIFY_ARE_EQUAL(U("wl.basic"), stok.token().scope());

    tok.set_access_token(U("newtoken"));
    tok.set_scope(U(""));

    stok.set_token(tok);
    VERIFY_ARE_EQUAL(U("newtoken"), stok.token().access_token());
    VERIFY_ARE_EQUAL(U(""), stok.token().scope());
}

TEST(oauth2_auth_code_grant_flow_uri)
{
    auto flow = auth_code_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("xyz"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());
    VERIFY_ARE_EQUAL(query_map.at(U("response_type")), U("code"));
    VERIFY_ARE_EQUAL(query_map.at(U("client_id")), U("s6BhdRkqt3"));
    VERIFY_ARE_EQUAL(query_map.at(U("scope")), U("xyz"));
    VERIFY_ARE_EQUAL(query_map.at(U("redirect_uri")), U("https%3A%2F%2Fclient%2Eexample%2Ecom%2Fcb"));
    VERIFY_IS_FALSE(query_map.at(U("state")).empty());

    VERIFY_ARE_EQUAL(flowuri.authority().to_string(), U("http://server.example.com/"));
    VERIFY_ARE_EQUAL(flowuri.path(), U("/authorize"));
}

TEST(oauth2_implicit_grant_flow_uri)
{
    auto flow = implicit_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("xyz"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());
    VERIFY_ARE_EQUAL(query_map.at(U("response_type")), U("token"));
    VERIFY_ARE_EQUAL(query_map.at(U("client_id")), U("s6BhdRkqt3"));
    VERIFY_ARE_EQUAL(query_map.at(U("scope")), U("xyz"));
    VERIFY_ARE_EQUAL(query_map.at(U("redirect_uri")), U("https%3A%2F%2Fclient%2Eexample%2Ecom%2Fcb"));
    VERIFY_IS_FALSE(query_map.at(U("state")).empty());

    VERIFY_ARE_EQUAL(flowuri.authority().to_string(), U("http://server.example.com/"));
    VERIFY_ARE_EQUAL(flowuri.path(), U("/authorize"));
}

TEST(oauth2_auth_code_grant_flow_complete)
{
    auto flow = auth_code_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("r_fullprofile r_emailaddress w_share"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());

    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3"), U("gX1fBat3bV")));

    http_client client(U("http://server.example.com/token"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Basic czZCaGRSa3F0MzpnWDFmQmF0M2JW"));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=authorization_code&code=SplxlOBeZQQYbYS6WxSbIA&redirect_uri=https%3A%2F%2Fclient%2Eexample%2Ecom%2Fcb&scope=r_fullprofile%20r_emailaddress%20w_share");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA",
"token_type":"bearer",
"expires_in":3600,
"refresh_token":"tGzv3JOkF0XG5Qx2TlKWIA",
"example_parameter":"example_value"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    auto tok = flow.complete(
        U("https://client.example.com/cb?code=SplxlOBeZQQYbYS6WxSbIA&state=") + query_map[U("state")],
        client).get();

    VERIFY_ARE_EQUAL(tok.access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(tok.refresh_token(), U("tGzv3JOkF0XG5Qx2TlKWIA"));
    VERIFY_ARE_EQUAL(tok.scope(), U("r_fullprofile r_emailaddress w_share"));
    VERIFY_ARE_EQUAL(tok.expires_in(), 3600);
    VERIFY_ARE_EQUAL(tok.token_type(), U("bearer"));
}

TEST(oauth2_auth_code_grant_flow_complete_throws)
{
    auto flow = auth_code_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("xyz"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());

    http_client null_client(U("http://0.0.0.0"));

    null_client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_IS_FALSE(true);
        return pplx::task_from_result(http_response());
    });

    auto invalid_state_uri = U("https://client.example.com/cb?code=SplxlOBeZQQYbYS6WxSbIA&state=0");
    VERIFY_THROWS(flow.complete(invalid_state_uri, null_client), oauth2_exception);
    auto missing_code_uri = U("https://client.example.com/cb?error=access_denied&state=") + query_map[U("state")];
    VERIFY_THROWS(flow.complete(missing_code_uri, null_client), oauth2_exception);

    http_client error_client(U("http://0.0.0.0"));
    error_client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        http_response resp;
        resp.set_body(R"(
{
"error":"invalid_client"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    auto redirect_uri = U("https://client.example.com/cb?code=SplxlOBeZQQYbYS6WxSbIA&state=") + query_map[U("state")];
    auto t = flow.complete(redirect_uri, error_client); // The initial call should not throw.
    VERIFY_THROWS(t.get(), oauth2_exception);
}

TEST(oauth2_implicit_grant_flow_complete)
{
    auto flow = implicit_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("xyz"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());

    auto tok = flow.complete(
        U("https://client.example.com/cb#access_token=2YotnFZFEjr1zCsicMWpAA&expires_in=3600&token_type=bearer&state=") + query_map[U("state")]);

    VERIFY_ARE_EQUAL(tok.access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(tok.refresh_token(), U(""));
    VERIFY_ARE_EQUAL(tok.scope(), U("xyz"));
    VERIFY_ARE_EQUAL(tok.expires_in(), 3600);
    VERIFY_ARE_EQUAL(tok.token_type(), U("bearer"));
}

TEST(oauth2_implicit_grant_flow_complete_change_scope)
{
    auto flow = implicit_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("xyz"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());

    auto tok = flow.complete(
        U("https://client.example.com/cb#scope=abc&access_token=2YotnFZFEjr1zCsicMWpAA&expires_in=3600&token_type=bearer&state=") + query_map[U("state")]);

    VERIFY_ARE_EQUAL(tok.access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(tok.refresh_token(), U(""));
    VERIFY_ARE_EQUAL(tok.scope(), U("abc"));
    VERIFY_ARE_EQUAL(tok.expires_in(), 3600);
    VERIFY_ARE_EQUAL(tok.token_type(), U("bearer"));
}

TEST(oauth2_implicit_grant_flow_complete_throws)
{
    auto flow = implicit_grant_flow(U("s6BhdRkqt3"), U("http://server.example.com/authorize"), U("https://client.example.com/cb"), U("xyz"));
    auto flowuri = flow.uri();
    auto query_map = uri::split_query(flowuri.query());

    auto invalid_state_uri = U("https://client.example.com/cb#access_token=2YotnFZFEjr1zCsicMWpAA&expires_in=3600&token_type=bearer&state=0");
    VERIFY_THROWS(flow.complete(invalid_state_uri), oauth2_exception);
    auto unsupported_token_type_uri = U("https://client.example.com/cb#access_token=2YotnFZFEjr1zCsicMWpAA&expires_in=3600&token_type=none&state=") + query_map[U("state")];
    VERIFY_THROWS(flow.complete(unsupported_token_type_uri), oauth2_exception);
    auto missing_access_token_uri = U("https://client.example.com/cb#expires_in=3600&token_type=bearer&state=") + query_map[U("state")];
    VERIFY_THROWS(flow.complete(missing_access_token_uri), oauth2_exception);
}

TEST(oauth2_resource_owner_creds_grant_flow)
{
    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3"), U("gX1fBat3bV")));
    auto user_creds = web::credentials(U("johndoe"), U("A3ddj3w"));

    http_client client(U("http://server.example.com/token"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.method(), methods::POST);
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Basic czZCaGRSa3F0MzpnWDFmQmF0M2JW"));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=password&username=johndoe&password=A3ddj3w&scope=xyz");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA",
"token_type":"bearer",
"expires_in":3600,
"refresh_token":"tGzv3JOkF0XG5Qx2TlKWIA",
"example_parameter":"example_value"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    auto token = resource_owner_creds_grant_flow(client, user_creds, U("xyz")).get();
    VERIFY_ARE_EQUAL(token.access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
}

TEST(oauth2_extension_grant_flow)
{
    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3"), U("gX1fBat3bV")));

    http_client client(U("http://server.example.com/token"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.method(), methods::POST);
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Basic czZCaGRSa3F0MzpnWDFmQmF0M2JW"));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Asaml2-bearer&assertion=PEFzc2VydGlvbiBJc3N1ZUluc3RhbnQ9IjIwMTEtMDU&scope=wl%2Ebasic");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    uri_builder req_body;
    req_body.append_query(U("grant_type"), U("urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Asaml2-bearer"), false);
    req_body.append_query(U("assertion"), U("PEFzc2VydGlvbiBJc3N1ZUluc3RhbnQ9IjIwMTEtMDU"));

    auto token = extension_grant_flow(req_body, client, U("wl.basic")).get();
    VERIFY_ARE_EQUAL(token.access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(token.scope(), U("wl.basic"));
}

TEST(oauth2_client_credentials_mode_request_body)
{
    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3."), U("gX1fBat3bV.")));

    http_client client(U("https://www.linkedin.com/uas/oauth2/accessToken"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.method(), methods::POST);
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_IS_FALSE(req.headers().has(header_names::authorization));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=code&code=AAA&client_id=s6BhdRkqt3%2E&client_secret=gX1fBat3bV%2E");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    uri_builder req_body;
    req_body.append_query(U("grant_type"), U("code"));
    req_body.append_query(U("code"), U("AAA"));

    auto token = extension_grant_flow(req_body, client, U(""), client_credentials_mode::request_body).get();
}

TEST(oauth2_extension_grant_flow_server_provided_scope)
{
    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3"), U("gX1fBat3bV")));

    http_client client(U("http://server.example.com/token"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.method(), methods::POST);
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Basic czZCaGRSa3F0MzpnWDFmQmF0M2JW"));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Asaml2-bearer&assertion=PEFzc2VydGlvbiBJc3N1ZUluc3RhbnQ9IjIwMTEtMDU");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA",
"scope":"wl%2Abasic"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    uri_builder req_body;
    req_body.append_query(U("grant_type"), U("urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Asaml2-bearer"), false);
    req_body.append_query(U("assertion"), U("PEFzc2VydGlvbiBJc3N1ZUluc3RhbnQ9IjIwMTEtMDU"));

    auto token = extension_grant_flow(req_body, client).get();
    VERIFY_ARE_EQUAL(token.access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(token.scope(), U("wl%2Abasic"));
}

TEST(oauth2_set_token_from_refresh_token)
{
    oauth2_token tok_value;
    tok_value.set_access_token(U("A"));
    tok_value.set_refresh_token(U("tGzv3JOkF0XG5Qx2TlKWIA"));
    tok_value.set_expires_in(3600);
    oauth2_shared_token tok(tok_value);

    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3"), U("gX1fBat3bV")));

    http_client client(U("http://server.example.com/token"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.method(), methods::POST);
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Basic czZCaGRSa3F0MzpnWDFmQmF0M2JW"));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=refresh_token&refresh_token=tGzv3JOkF0XG5Qx2TlKWIA&scope=wl%2Ebasic");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA",
"token_type":"bearer"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    tok.set_token_via_refresh_token(client, U("wl.basic")).get();
    VERIFY_ARE_EQUAL(tok.token().access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(tok.token().refresh_token(), U(""));
    VERIFY_ARE_EQUAL(tok.token().scope(), U("wl.basic"));
    VERIFY_ARE_EQUAL(tok.token().expires_in(), oauth2_token::undefined_expiration);
}

TEST(oauth2_set_token_from_refresh_token_preserve_scope)
{
    oauth2_token tok_value;
    tok_value.set_access_token(U("A"));
    tok_value.set_refresh_token(U("tGzv3JOkF0XG5Qx2TlKWIA"));
    tok_value.set_expires_in(3600);
    tok_value.set_scope(U("wl.basic"));
    oauth2_shared_token tok(tok_value);

    http_client_config config;
    config.set_credentials(web::credentials(U("s6BhdRkqt3"), U("gX1fBat3bV")));

    http_client client(U("http://server.example.com/token"), config);

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.method(), methods::POST);
        VERIFY_ARE_EQUAL(req.headers().content_type(), U("application/x-www-form-urlencoded; charset=utf-8"));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Basic czZCaGRSa3F0MzpnWDFmQmF0M2JW"));
        VERIFY_ARE_EQUAL(req.extract_utf8string(true).get(), "grant_type=refresh_token&refresh_token=tGzv3JOkF0XG5Qx2TlKWIA");

        http_response resp;
        resp.set_body(R"(
{
"access_token":"2YotnFZFEjr1zCsicMWpAA",
"token_type":"bearer"
}
        )", "application/json;charset=UTF-8");

        return pplx::task_from_result(resp);
    });

    tok.set_token_via_refresh_token(client).get();
    VERIFY_ARE_EQUAL(tok.token().access_token(), U("2YotnFZFEjr1zCsicMWpAA"));
    VERIFY_ARE_EQUAL(tok.token().refresh_token(), U(""));
    VERIFY_ARE_EQUAL(tok.token().scope(), U("wl.basic"));
    VERIFY_ARE_EQUAL(tok.token().expires_in(), oauth2_token::undefined_expiration);
}

TEST(oauth2_set_token_from_refresh_token_empty_refresh_token)
{
    oauth2_token tok_value;
    tok_value.set_access_token(U("A"));
    tok_value.set_refresh_token(U(""));
    oauth2_shared_token tok(tok_value);

    http_client null_client(U("http://0.0.0.0"));
    null_client.add_handler([](http_request, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_IS_TRUE(false);
        return pplx::task_from_result(http_response());
    });

    VERIFY_THROWS(tok.set_token_via_refresh_token(null_client), oauth2_exception);
}

TEST(oauth2_pipeline_stage)
{
    http_client client(U("http://example.com/"));

    oauth2_shared_token tok(oauth2_token(U("mF_9.B5f-4.1JqM")));

    client.add_handler(tok.create_pipeline_stage());

    client.add_handler([&](http_request req, std::shared_ptr<http_pipeline_stage>)
    {
        VERIFY_ARE_EQUAL(req.absolute_uri().query(), U(""));
        VERIFY_ARE_EQUAL(req.headers()[header_names::authorization], U("Bearer mF_9.B5f-4.1JqM"));

        http_response resp;
        resp.set_body("valid");

        return pplx::task_from_result(resp);
    });

    VERIFY_ARE_EQUAL(client.request(methods::GET).get().extract_string().get(), U("valid"));
}

} // SUITE(oauth2_tests)
}}}}
