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
* datafetcher.cpp: Listener code: Given a location/postal code, the listener queries different services
* for weather, things to do: events, movie and pictures and returns it to the client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "casalens.h"

using namespace utility;
using namespace concurrency;
using namespace concurrency::streams;

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental;
using namespace web::http::experimental::listener;

// In case of any failure to fetch data from a service, 
// create a json object with "error" key and value containing the exception details.
pplx::task<json::value> CasaLens::handle_exception(pplx::task<json::value>& t, const utility::string_t& field_name)
{
    try
    {
        t.get();
    }
    catch(const std::exception& ex)
    {
        json::value error_json = json::value::object();
        error_json[field_name] = json::value::object();
        error_json[field_name][error_json_key] = json::value::string(utility::conversions::to_string_t(ex.what()));
        return pplx::task_from_result<json::value>(error_json);
    }

    return t;
}

// Given postal code, query eventful service and obtain num_events upcoming events at that location.
// Returns a task of json::value of event data
// JSON result Format:
// {"events":["title":"event1","url":"http://event1","starttime":"ddmmyy",..]}
pplx::task<json::value> CasaLens::get_events(const utility::string_t& postal_code)
{
    uri_builder ub(casalens_creds::events_url + postal_code);

    ub.append_query(casalens_creds::events_keyname, casalens_creds::events_key);
    auto event_url = ub.to_string();

    http_client ev_client(event_url);
    return ev_client.request(methods::GET, event_url).then([](http_response resp)
    {
        return resp.extract_json();
    }).then([](json::value event_json)
    {
        json::value event_result_node = json::value::object();

        if (!event_json[U("events")][U("event")].is_null())
        {
            event_result_node[U("events")] = json::value::array();

            int i = 0;
            for(auto& iter:event_json[U("events")][U("event")])
            {
                auto event = iter.second;
                event_result_node[events_json_key][i][U("title")] = event[U("title")];
                event_result_node[events_json_key][i][U("url")] = event[U("url")];
                event_result_node[events_json_key][i][U("starttime")] = event[U("start_time")];
                event_result_node[events_json_key][i][U("description")] = event[U("description")];
                event_result_node[events_json_key][i][U("url")] = event[U("url")];
                event_result_node[events_json_key][i][U("venue_address")] = json::value::string(event[U("venue_address")].as_string() + U(" ") + event[U("city_name")].as_string());

                if (i++ > num_events)
                    break;
            }
        }
        else
        {
            // Event data is null, we hit an error.
            event_result_node[events_json_key] = json::value::object();
            event_result_node[events_json_key][error_json_key] = event_json[U("events")][U("description")];
        }

        return event_result_node;
    }).then([=](pplx::task<json::value> t)
    {
        return handle_exception(t, events_json_key);
    });
}

// Query openweathermap API and obtain weather information at the given location.
// Returns a task of json::value containing the weather data.
// Format:
// {"weather":{"temperature":"49","pressure":"xxx"..}}
pplx::task<json::value> CasaLens::get_weather(const utility::string_t& postal_code)
{
    utility::string_t weather_url(casalens_creds::weather_url);

    uri_builder ub_weather(weather_url.append(postal_code));
    ub_weather.append_query(U("units"), U("imperial"));

    http_client weather_client(ub_weather.to_string());
    return weather_client.request(methods::GET).then([](http_response resp)
    {
        return resp.extract_string(); 
    }).then([](utility::string_t weather_str)
    {
        json::value weather_json = json::value::parse(weather_str);

        auto& j = weather_json[U("list")][0][U("main")];

        json::value weather_result_node = json::value::object();
        weather_result_node[weather_json_key] = json::value::object();
        auto& w = weather_result_node[U("weather")];
        w[U("temperature")] = j[U("temp")];
        w[U("pressure")] = j[U("pressure")];
        w[U("temp_min")] = j[U("temp_min")];
        w[U("temp_max")] = j[U("temp_max")];
        w[U("image")] = json::value::string(U("http://openweathermap.org/img/w/") + weather_json[U("list")][0][U("weather")][0][U("icon")].as_string() + U(".png"));
        w[U("description")] = weather_json[U("list")][0][U("weather")][0][U("description")];
        return weather_result_node;
    }).then([=](pplx::task<json::value> t)
    {
        return handle_exception(t, weather_json_key);
    });
}

// Query bing images to fetch some image URLs of the given location
// Returns a task of json::Value
// JSON result format:
// {"images":["url1", "url2"]}
pplx::task<json::value> CasaLens::get_pictures(const utility::string_t& location, const utility::string_t& count)
{
    http_client_config config;
    credentials cred(casalens_creds::images_keyname, casalens_creds::images_key);
    config.set_credentials(cred);
    utility::string_t bing_url(casalens_creds::images_url);
    uri_builder ub_bing(bing_url);
    ub_bing.append_query(U("Query"), U("'") + location + U("'"));
    ub_bing.append_query(U("$top"), count);
    ub_bing.append_query(U("ImageFilters"), U("'Size:Medium'"));

    http_client bing_client(ub_bing.to_string(), config);
    return bing_client.request(methods::GET).then([](http_response resp)
    {
        return resp.extract_json();
    }).then([](json::value image_json)
    {
        json::value image_result_node = json::value::object();
        image_result_node[images_json_key] = json::value::array();

        int i = 0;
        for(auto& iter:image_json[U("d")][U("results")])
        {
            auto image = iter.second;
            image_result_node[images_json_key][i] = image[U("MediaUrl")];
            if (i++ > num_images)
                break;
        }
        return image_result_node;
    }).then([=](pplx::task<json::value> t)
    {
        return handle_exception(t, images_json_key);
    });
}

// Get the current date
std::wstring CasaLens::get_date()
{
    time_t t = time(0);
    struct tm now;
    std::wostringstream date;
    if (0 == localtime_s(&now, &t))
    {
        date << (now.tm_year + 1900) << '-' 
            << (now.tm_mon + 1) << '-'
            <<  now.tm_mday;
    }
    return date.str();
}

// Query tmsapi and fetch current movie showtimes at local theaters, for the given postal code
// Also quert bing images for movie posters 
// Returns a task of JSON value
// JSON result format: 
// "movies":[{"title":"abc","theatre":[{"name":"theater1","datetime":["dd-mm-yyThh:mm"]},{"name":"theater2","datetime":["ddmmyy"]}],"poster":"img-url"}}]}..
pplx::task<json::value> CasaLens::get_movies(const utility::string_t& postal_code)
{
    uri_builder ub_movie(casalens_creds::movies_url);

    ub_movie.append_query(U("startDate"), get_date());
    ub_movie.append_query(U("zip"), postal_code);
    ub_movie.append_query(casalens_creds::movies_keyname, casalens_creds::movies_key);
    ub_movie.append_query(U("imageSize"), U("Sm"));

    http_client tms_client(ub_movie.to_string());
    return tms_client.request(methods::GET).then([](http_response resp)
    {
        return resp.extract_json();
    }).then([](json::value movie_json)
    {
        // From the data obtained from tmsapi, construct movie JSON value to be sent to the client
        // We will collect data for 5 movies, and 5 showtime info per movie.
        json::value movie_result_node = json::value::object();

        if (movie_json.size())
        {
            auto temp = json::value::array();
            int i = 0;
            for(auto iter = movie_json.cbegin(); iter != movie_json.cend() && i < num_movies; iter++, i++)
            {
                auto& showtimes = iter->second[U("showtimes")];

                int j = 0;
                int showtime_index = 0;
                int theater_index = -1;
                utility::string_t current_theater;
                temp[i][U("title")] =  iter->second[U("title")];

                for (auto iter2 = showtimes.cbegin(); iter2 != showtimes.cend() &&  j < num_movies; iter2++,j++)
                {
                    auto theater = iter2->second[U("theatre")][U("name")].as_string();
                    if (0 != theater.compare(current_theater)) // new theater
                    {
                        temp[i][U("theatre")][++theater_index][U("name")] = json::value::string(theater); // Add theater entry
                        showtime_index = 0;
                        current_theater = theater;
                    }
                    temp[i][U("theatre")][theater_index][U("datetime")][showtime_index++] = iter2->second[U("dateTime")]; // Update the showtime for same theater
                }
            }
            movie_result_node[movies_json_key] = std::move(temp);
        }
        else
        {
            movie_result_node[movies_json_key] = json::value::object();
            movie_result_node[movies_json_key][error_json_key] = json::value::string(U("Failed to fetch movie data"));
        }

        return pplx::task_from_result(movie_result_node);
    }).then([=](json::value movie_result)
    {
        try
        {
            // For every movie, obtain movie poster URL from bing
            std::vector<utility::string_t> movie_list;
            std::vector<pplx::task<json::value>> poster_tasks;
            auto date = get_date();
            std::wstring year = date.substr(0, date.find(U("-"))); 

            for(auto& iter:movie_result[movies_json_key])
            {
                auto title = iter.second[U("title")].as_string();
                auto searchStr = title + U(" ") + year + U(" new movie poster");
                poster_tasks.push_back(get_pictures(searchStr, U("1")));
                movie_list.push_back(title);
            }

            pplx::when_all(poster_tasks.begin(), poster_tasks.end()).wait();
            json::value resp_data = json::value::object();

            for(unsigned int i = 0; i < poster_tasks.size(); i++)
            {
                auto jval = poster_tasks[i].get();
                auto poster_url = jval.begin()->second[0];
                movie_result[movies_json_key][i][U("poster")] = poster_url;
            }
        }
        catch(...)
        {
            // Ignore any failure in fetching movie posters. Just return
            // the movie data to the client.
        }
        return pplx::task_from_result(movie_result);
    }).then([=](pplx::task<json::value> t)
    {
        return handle_exception(t, movies_json_key);
    });
}

bool CasaLens::is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](char c) { return !std::isdigit(c, std::locale()); }) == s.end();
}

void CasaLens::fetch_data(http_request message, const std::wstring& postal_code, const std::wstring& location)
{
    json::value resp_data;

    try
    {
        m_rwlock.lock_read();
        resp_data = m_data[postal_code];
        m_rwlock.unlock();

        if (resp_data.is_null())
        {
            std::vector<pplx::task<json::value>> tasks;

            tasks.push_back(get_events(postal_code));
            tasks.push_back(get_weather(postal_code));
            tasks.push_back(get_pictures(location, U("4")));
            tasks.push_back(get_movies(postal_code));

            pplx::when_all(tasks.begin(), tasks.end()).wait();
            resp_data = json::value::object();

            for(auto& iter:tasks)
            {
                auto jval = iter.get();
                auto key = jval.begin()->first.as_string();
                resp_data[key] = jval.begin()->second;
            }

            m_rwlock.lock();
            m_data[postal_code] = resp_data;
            m_rwlock.unlock();
        }

        // Reply with the aggregated JSON data
        message.reply(status_codes::OK, resp_data).then(std::bind(&handle_error, std::placeholders::_1));
    }
    catch(...)
    {
        message.reply(status_codes::InternalError).then(std::bind(&handle_error, std::placeholders::_1));
    }
}

// Check if the input text is a number or string.
// If string => city name, use bing maps API to obtain the postal code for that city
// number => postal code, use google maps API to obtain city name (location data) for that postal code.
// then call fetch_data to query different services, aggregate movie, images, events, weather etc for that city and respond to the request.
void CasaLens::get_data(http_request message, const std::wstring& input_text)
{
    if (!is_number(utility::conversions::to_utf8string(input_text)))
    {
        std::wstring bing_maps_url(casalens_creds::bmaps_url);
        uri_builder maps_builder;
        maps_builder.append_query(U("locality"), input_text);
        maps_builder.append_query(casalens_creds::bmaps_keyname, casalens_creds::bmaps_key);
        auto s = maps_builder.to_string();
        http_client bing_client(bing_maps_url);
        bing_client.request(methods::GET, s).then([=](http_response resp) 
        {
            return resp.extract_json();
        }).then([=](json::value& maps_result) mutable
        {
            auto coordinates = maps_result[U("resourceSets")][0][U("resources")][0][U("point")];
            auto lattitude = coordinates[U("coordinates")][0].to_string();
            auto longitude = coordinates[U("coordinates")][1].to_string();
            uri_builder ub;
            ub.append_path(lattitude + U(",") + longitude).append_query(casalens_creds::bmaps_keyname, casalens_creds::bmaps_key);
            auto s = ub.to_string();
            return bing_client.request(methods::GET, s);
        }).then([](http_response resp)
        {
            return resp.extract_json();
        }).then([=](json::value& maps_result)
        {
            auto postal_code = maps_result[U("resourceSets")][0][U("resources")][0][U("address")][U("postalCode")].as_string();
            fetch_data(message, postal_code, input_text);            
        }).then([=](pplx::task<void> t)
        {
            try
            {
                t.get();
            }
            catch(...)
            {
                message.reply(status_codes::InternalError, U("Failed to fetch the postal code"));
            }
        });
    }
    else // get location from postal code
    {
        http_client client(casalens_creds::gmaps_url);

        uri_builder ub;
        ub.append_query(U("address"), input_text);
        ub.append_query(U("sensor"), U("false"));

        client.request(methods::GET, ub.to_string()).then([](http_response resp)
        {
            return resp.extract_json();
        }).then([=](json::value jval)
        {
            auto locationstr =  jval[U("results")][0][U("address_components")][1][U("long_name")].as_string();
            fetch_data(message, input_text, locationstr);
        }).then([=](pplx::task<void> t)
        {
            try
            {
                t.get();
            }
            catch(...)
            {
                message.reply(status_codes::InternalError, U("Failed to fetch the location from postal code"));
            }
        });
    }
    return;
}
