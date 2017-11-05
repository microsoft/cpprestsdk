/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Builder for constructing URIs.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

namespace web
{

uri_builder &uri_builder::append_path(const utility::string_t &path, bool is_encode)
{
    if(path.empty() || path == _XPLATSTR("/"))
    {
        return *this;
    }

    auto encoded_path = is_encode ? uri::encode_uri(path, uri::components::path) : path;
    auto thisPath = this->path();
    if(thisPath.empty() || thisPath == _XPLATSTR("/"))
    {
        if(encoded_path.front() != _XPLATSTR('/'))
        {
            set_path(_XPLATSTR("/") + encoded_path);
        }
        else
        {
            set_path(encoded_path);
        }
    }
    else if(thisPath.back() == _XPLATSTR('/') && encoded_path.front() == _XPLATSTR('/'))
    {
        thisPath.pop_back();
        set_path(thisPath + encoded_path);
    }
    else if(thisPath.back() != _XPLATSTR('/') && encoded_path.front() != _XPLATSTR('/'))
    {
        set_path(thisPath + _XPLATSTR("/") + encoded_path);
    }
    else
    {
        // Only one slash.
        set_path(thisPath + encoded_path);
    }
    return *this;
}

uri_builder &uri_builder::append_query(const utility::string_t &query, bool is_encode)
{
    if(query.empty())
    {
        return *this;
    }

    auto encoded_query = is_encode ? uri::encode_uri(query, uri::components::query) : query;
    auto thisQuery = this->query();
    if (thisQuery.empty())
    {
        this->set_query(encoded_query);
    }
    else if(thisQuery.back() == _XPLATSTR('&') && encoded_query.front() == _XPLATSTR('&'))
    {
        thisQuery.pop_back();
        this->set_query(thisQuery + encoded_query);
    }
    else if(thisQuery.back() != _XPLATSTR('&') && encoded_query.front() != _XPLATSTR('&'))
    {
        this->set_query(thisQuery + _XPLATSTR("&") + encoded_query);
    }
    else
    {
        // Only one ampersand.
        this->set_query(thisQuery + encoded_query);
    }
    return *this;
}

uri_builder &uri_builder::set_port(const utility::string_t &port)
{
    utility::istringstream_t portStream(port);
    portStream.imbue(std::locale::classic());
    int port_tmp;
    portStream >> port_tmp;
    if (portStream.fail() || portStream.bad())
    {
        throw std::invalid_argument("invalid port argument, must be non empty string containing integer value");
    }
    m_uri.m_port = port_tmp;
    return *this;
}

uri_builder &uri_builder::append(const http::uri &relative_uri)
{
    append_path(relative_uri.path());
    append_query(relative_uri.query());
    this->set_fragment(this->fragment() + relative_uri.fragment());
    return *this;
}

utility::string_t uri_builder::to_string() const
{
    return to_uri().to_string();
}

uri uri_builder::to_uri() const
{
    return uri(m_uri);
}

bool uri_builder::is_valid()
{
    return uri::validate(m_uri.join());
}

void uri_builder::append_query_encode_impl(const utility::string_t & name, const utf8string & value)
{
    utility::string_t encodedQuery = uri::encode_query_impl(utility::conversions::to_utf8string(name));
    encodedQuery.append(_XPLATSTR("="));
    encodedQuery.append(uri::encode_query_impl(value));

    // The query key value pair was already encoded by us or the user separately.
    append_query(encodedQuery, false);
}

void uri_builder::append_query_no_encode_impl(const utility::string_t & name, const utility::string_t & value)
{
    append_query(name + _XPLATSTR("=") + value, false);
}

} // namespace web
