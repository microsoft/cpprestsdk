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

uri_builder &uri_builder::append(const http::uri &relative_uri)
{
    append_path(relative_uri.path());
    append_query(relative_uri.query());
    this->set_fragment(this->fragment() + relative_uri.fragment());
    return *this;
}

utility::string_t uri_builder::to_string()
{
    return to_uri().to_string();
}

uri uri_builder::to_uri()
{
    return uri(m_uri);
}

bool uri_builder::is_valid()
{
    return uri::validate(m_uri.join());
}

} // namespace web

