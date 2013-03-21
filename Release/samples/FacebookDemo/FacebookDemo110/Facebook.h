// Facebook.h
// see blog post at http://aka.ms/FacebookCppRest
// for a detailed walkthrough of this sample

#pragma once
#include <string>
#include <http_client.h>

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
