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
* HTTP Library: JSON parser and writer
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;

extern bool json::details::g_keep_json_object_unsorted = false;

void json::keep_object_element_order(bool keep_order)
{
    json::details::g_keep_json_object_unsorted = keep_order;
}

web::json::value::value() :
    value(std::unique_ptr<web::json::details::_Value>())
{ }

web::json::value::value(int32_t v) :
    value(utility::details::make_unique<web::json::details::_Number>(v))
{ }

web::json::value::value(uint32_t v) :
    value(utility::details::make_unique<web::json::details::_Number>(v))
{ }

web::json::value::value(int64_t v) :
    value(utility::details::make_unique<web::json::details::_Number>(v))
{ }

web::json::value::value(uint64_t v) :
    value(utility::details::make_unique<web::json::details::_Number>(v))
{ }

web::json::value::value(double v) :
    value(utility::details::make_unique<web::json::details::_Number>(v))
{ }

web::json::value::value(bool v) :
    value(utility::details::make_unique<web::json::details::_Boolean>(v))
{ }

web::json::value::value(std::string v) :
    value(utility::details::make_unique<web::json::details::_String>(std::move(v)))
{ }

web::json::value::value(std::string v, bool has_escape_chars) :
    value(utility::details::make_unique<web::json::details::_String>(std::move(v), has_escape_chars))
{ }

web::json::value::value(const value &other) :
    value(other.m_value ? other.m_value->_copy_value() : std::unique_ptr<json::details::_Value>(nullptr))
{ }

web::json::value &web::json::value::operator=(const value &other)
{
    if(this != &other)
    {
        if (other.m_value)
            m_value = other.m_value->_copy_value();
        else
            m_value.reset();
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        m_kind = other.m_kind;
#endif
    }
    return *this;
}

web::json::value::value(value &&other) CPPREST_NOEXCEPT :
    value(std::move(other.m_value))
{}

web::json::value &web::json::value::operator=(web::json::value &&other) CPPREST_NOEXCEPT
{
    if(this != &other)
    {
        m_value.swap(other.m_value);
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        std::swap(m_kind, other.m_kind);
#endif
    }
    return *this;
}

web::json::value web::json::value::object(std::vector<std::pair<std::string, value>> fields, bool keep_order)
{
    return web::json::value(utility::details::make_unique<details::_Object>(std::move(fields), keep_order));
}

web::json::value web::json::value::array(std::vector<value> elements)
{
    return web::json::value(utility::details::make_unique<details::_Array>(std::move(elements)));
}

const web::json::number& web::json::value::as_number() const
{
    if (!m_value)
        throw json_exception("not a number");
    return m_value->as_number();
}

double web::json::value::as_double() const
{
    if (!m_value)
        throw json_exception("not a number");
    return m_value->as_double();
}

int web::json::value::as_integer() const
{
    if (!m_value)
        throw json_exception("not a number");
    return m_value->as_integer();
}

bool web::json::value::as_bool() const
{
    if (!m_value)
        throw json_exception("not a boolean");
    return m_value->as_bool();
}

json::array& web::json::value::as_array()
{
    if (!m_value)
        throw json_exception("not an array");
    return m_value->as_array();
}

const json::array& web::json::value::as_array() const
{
    if (!m_value)
        throw json_exception("not an array");
    return m_value->as_array();
}

json::object& web::json::value::as_object()
{
    if (!m_value)
        throw json_exception("not an object");
    return m_value->as_object();
}

const json::object& web::json::value::as_object() const
{
    if (!m_value)
        throw json_exception("not an object");
    return m_value->as_object();
}

bool web::json::number::is_int32() const
{
    switch (m_type)
    {
    case signed_type : return m_intval >= std::numeric_limits<int32_t>::min() && m_intval <= std::numeric_limits<int32_t>::max();
    case unsigned_type : return m_uintval <= std::numeric_limits<int32_t>::max();
    case double_type :
    default :
        return false;
    }
}

bool web::json::number::is_uint32() const
{
    switch (m_type)
    {
    case signed_type : return m_intval >= 0 && m_intval <= std::numeric_limits<uint32_t>::max();
    case unsigned_type : return m_uintval <= std::numeric_limits<uint32_t>::max();
    case double_type :
    default :
        return false;
    }
}

bool web::json::number::is_int64() const
{
    switch (m_type)
    {
    case signed_type : return true;
    case unsigned_type : return m_uintval <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    case double_type :
    default :
        return false;
    }
}

bool web::json::details::_String::has_escape_chars(const _String &str)
{
    return std::any_of(std::begin(str.m_string), std::end(str.m_string), [](char const x)
    {
        if (x <= 31) { return true; }
        if (x == '"') { return true; }
        if (x == '\\') { return true; }
        return false;
    });
}

web::json::value::value_type json::value::type() const { return m_value ? m_value->type() : json::value::Null; }

bool json::value::is_integer() const
{
    if(!is_number())
    {
        return false;
    }
    return m_value->is_integer();
}

bool json::value::is_double() const
{
    if(!is_number())
    {
        return false;
    }
    return m_value->is_double();
}

json::value& web::json::details::_Object::index(const std::string &key)
{
    return m_object[key];
}

bool web::json::details::_Object::has_field(const std::string &key) const
{
    return m_object.find(key) != m_object.end();
}

bool json::value::operator==(const json::value &other) const
{
    if (this->m_value.get() == other.m_value.get())
        return true;
    if (this->type() != other.type())
        return false;

    switch(this->type())
    {
    case Null:
        return true;
    case Number:
        return this->as_number() == other.as_number();
    case Boolean:
        return this->as_bool() == other.as_bool();
    case String:
        return this->as_string() == other.as_string();
    case Object:
        return static_cast<const json::details::_Object*>(this->m_value.get())->is_equal(static_cast<const json::details::_Object*>(other.m_value.get()));
    case Array:
        return static_cast<const json::details::_Array*>(this->m_value.get())->is_equal(static_cast<const json::details::_Array*>(other.m_value.get()));
    }
    __assume(0);
}

void web::json::value::erase(size_t index)
{
    return this->as_array().erase(index);
}

void web::json::value::erase(const std::string& key)
{
    return this->as_object().erase(key);
}

// at() overloads
web::json::value& web::json::value::at(size_t index)
{
    return this->as_array().at(index);
}

const web::json::value& web::json::value::at(size_t index) const
{
    return this->as_array().at(index);
}

web::json::value& web::json::value::at(const std::string& key)
{
    return this->as_object().at(key);
}

const web::json::value& web::json::value::at(const std::string& key) const
{
    return this->as_object().at(key);
}

web::json::value& web::json::value::operator [] (const std::string& key)
{
    if ( this->is_null() )
    {
        m_value.reset(new web::json::details::_Object(details::g_keep_json_object_unsorted));
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        m_kind = value::Object;
#endif
    }
    return m_value->index(key);
}

web::json::value& web::json::value::operator[](size_t index)
{
    if ( this->is_null() )
    {
        m_value.reset(new web::json::details::_Array());
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        m_kind = value::Array;
#endif
    }
    return m_value->index(index);
}

// Remove once VS 2013 is no longer supported.
#if defined(_WIN32) && _MSC_VER < 1900
static web::json::details::json_error_category_impl instance;
#endif
const web::json::details::json_error_category_impl& web::json::details::json_error_category()
{
#if !defined(_WIN32) || _MSC_VER >= 1900
    static web::json::details::json_error_category_impl instance;
#endif
    return instance;
}
