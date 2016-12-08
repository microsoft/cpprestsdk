/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* casalens.h: Listener code: Given a location/postal code, the listener queries different services
* for weather, things to do: events, movie and pictures and returns it to the client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "stdafx.h"
#include "cpprest\http_listener.h"
#include "cpprest/json.h"
#include "cpprest/filestream.h"
#include "cpprest/containerstream.h"
#include "cpprest/producerconsumerstream.h"
#include "cpprest/http_listener.h"
#include "cpprest/http_client.h"
#pragma warning ( push )
#pragma warning ( disable : 4457 )
#include <agents.h>
#pragma warning ( pop )
#include <locale>
#include <ctime>

//
// Credentials class to manage the service URLs, key name and api-keys.
//
class casalens_creds
{
public:
    static const utility::string_t events_url;
    static const utility::string_t movies_url;
    static const utility::string_t images_url;
    static const utility::string_t weather_url;
    static const utility::string_t bmaps_url;
    static const utility::string_t gmaps_url;

    static const utility::string_t events_keyname;
    static const utility::string_t events_key;
    static const utility::string_t movies_keyname;
    static const utility::string_t movies_key;
    static const utility::string_t images_keyname;
    static const utility::string_t images_key;
    static const utility::string_t bmaps_keyname;
    static const utility::string_t bmaps_key;
};

//
// This class implements the CasaLens server functionality.
// It maintains a http_listener, registers to listen for requests.
// Also implements the handlers to respond to requests and methods to fetch 
// and aggregate data from different services.
//
class CasaLens
{
public:
    CasaLens() {}
    CasaLens(utility::string_t url);

    pplx::task<void> open();
    pplx::task<void> close();

    void handle_get(web::http::http_request message);
    void handle_post(web::http::http_request message);

private:

    // Error handlers
    static void handle_error(pplx::task<void>& t);
    pplx::task<web::json::value> handle_exception(pplx::task<web::json::value>& t, const utility::string_t& field_name);

    // methods to fetch data from the services
    pplx::task<web::json::value> get_events(const std::string& post_code);
    pplx::task<web::json::value> get_weather(const std::string& post_code);
    pplx::task<web::json::value> get_pictures(const std::string& location, const std::string& count);
    pplx::task<web::json::value> get_movies(const std::string& post_code);

    // Utility functions
    bool is_number(const std::string& s);
    std::wstring get_date();

    void fetch_data(web::http::http_request message, const std::string& postal_code, const std::string& location);
    void get_data(web::http::http_request message, const std::string& location);

    static const int num_events = 5;
    static const int num_images = 5;
    static const int num_movies = 5;

    static const utility::string_t events_json_key;
    static const utility::string_t movies_json_key;
    static const utility::string_t weather_json_key;
    static const utility::string_t images_json_key;
    static const utility::string_t error_json_key;

    // Lock to the in memory cache (m_data)
    concurrency::reader_writer_lock m_rwlock;

    // key: City name or postal code
    // Value: JSON response data to be sent to clients
    // We are caching the data for multiple requests.
    std::map<std::string, web::json::value> m_data;

    // m_htmlcontentmap contains data about the html contents of the website, their mime types
    // key: relative URI path of the HTML content being requested
    // value: Tuple where:
    // Element1: relative path on the disk of the file being requested
    // Element2: Mime type/content type of the file
    std::map<utility::string_t, std::tuple<utility::string_t, utility::string_t>> m_htmlcontentmap; 

    // HTTP listener 
    web::http::experimental::listener::http_listener m_listener;   
};