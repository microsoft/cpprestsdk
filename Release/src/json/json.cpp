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
* json.cpp
*
* HTTP Library: JSON parser and writer
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

utility::ostream_t& web::json::operator << (utility::ostream_t &os, const web::json::value &val)
{
    val.serialize(os);
    return os;
}

utility::istream_t& web::json::operator >> (utility::istream_t &is, json::value &val)
{
    val = json::value::parse(is);
    return is;
}

#pragma region json::value Constructors

web::json::value::value() : 
    m_value(utility::details::make_unique<web::json::details::_Null>())
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Null)
#endif
    { }

web::json::value::value(int value) : 
    m_value(utility::details::make_unique<web::json::details::_Number>(value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Number)
#endif
    { }

web::json::value::value(double value) : 
    m_value(utility::details::make_unique<web::json::details::_Number>(value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Number)
#endif
    { }

web::json::value::value(bool value) : 
    m_value(utility::details::make_unique<web::json::details::_Boolean>(value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Boolean)
#endif
    { }

web::json::value::value(utility::string_t value) : 
    m_value(utility::details::make_unique<web::json::details::_String>(std::move(value)))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::String)
#endif
    { }

web::json::value::value(const utility::char_t* value) : 
    m_value(utility::details::make_unique<web::json::details::_String>(utility::string_t(value)))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::String)
#endif
    { }

web::json::value::value(const value &other) : 
    m_value(other.m_value->_copy_value())
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(other.m_kind)
#endif
    { }

web::json::value &web::json::value::operator=(const value &other)
{
    if(this != &other)
    {
        m_value = std::unique_ptr<details::_Value>(other.m_value->_copy_value());
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        m_kind = other.m_kind;
#endif
    }
    return *this;
}
web::json::value::value(value &&other) : 
    m_value(std::move(other.m_value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(other.m_kind)
#endif
{}

web::json::value &web::json::value::operator=(web::json::value &&other)
{
    if(this != &other)
    {
        m_value.swap(other.m_value);
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        m_kind = other.m_kind;
#endif
    }
    return *this;
}

web::json::value::element_vector web::json::details::_Value::s_elements;

#pragma endregion
