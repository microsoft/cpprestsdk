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
* Facebook.h - Simple client for connecting to facebook. See blog post 
* at http://aka.ms/FacebookCppRest for a detailed walkthrough of this sample
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once
#include <string>
#include <cpprest/http_client.h>

class facebook_client {
public:
	static facebook_client& instance(); // Singleton
	pplx::task<void> login(std::wstring scopes);
	pplx::task<web::json::value> get(std::wstring path);
	web::http::uri_builder base_uri(bool absolute = false);

private:
	facebook_client(): 
	raw_client(L"https://graph.facebook.com/"),
	signed_in(false) {}

	pplx::task<void> full_login(std::wstring scopes);

	std::wstring token_;
	bool signed_in;
	web::http::client::http_client raw_client;
};
