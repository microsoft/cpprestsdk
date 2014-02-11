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

using namespace web;

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

web::json::value::value(int32_t value) : 
    m_value(utility::details::make_unique<web::json::details::_Number>(value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Number)
#endif
    { }

web::json::value::value(uint32_t value) : 
    m_value(utility::details::make_unique<web::json::details::_Number>(value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Number)
#endif
    { }

web::json::value::value(int64_t value) : 
    m_value(utility::details::make_unique<web::json::details::_Number>(value))
#ifdef ENABLE_JSON_VALUE_VISUALIZER
    ,m_kind(value::Number)
#endif
    { }

web::json::value::value(uint64_t value) : 
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

#pragma endregion


#pragma region Static Factories

web::json::value web::json::value::null()
{
    return web::json::value();
}

web::json::value web::json::value::number(double value)
{
    return web::json::value(value);
}

web::json::value web::json::value::number(int32_t value)
{
    return web::json::value(value);
}

web::json::value web::json::value::boolean(bool value)
{
    return web::json::value(value);
}

web::json::value web::json::value::string(utility::string_t value)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_String>(std::move(value));
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::String
#endif
            );
}

#ifdef _MS_WINDOWS
web::json::value web::json::value::string(const std::string &value)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_String>(utility::conversions::to_utf16string(value));
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::String
#endif
            );
}
#endif

web::json::value web::json::value::object()
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Object>();
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::Object
#endif
            );
}

web::json::value web::json::value::object(web::json::value::field_map fields)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Object>(std::move(fields));
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::Object
#endif
            );
}

web::json::value web::json::value::array()
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Array>();
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::Array
#endif
            );
}

web::json::value web::json::value::array(size_t size)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Array>(size);
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::Array
#endif
            );
}

web::json::value web::json::value::array(json::value::array_vector elements)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Array>(std::move(elements));
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::Array
#endif
            );
}

#pragma endregion

web::json::number web::json::value::as_number() const
{
    return m_value->as_number();
}

double web::json::value::as_double() const
{
    return m_value->as_double();
}
 
int web::json::value::as_integer() const
{
    return m_value->as_integer();
}

bool web::json::value::as_bool() const
{
    return m_value->as_bool();
}

json::array& web::json::value::as_array()
{
    return m_value->as_array();
}

const json::array& web::json::value::as_array() const
{
    return m_value->as_array();
}

const json::value::object_vector& web::json::value::as_object() const
{
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

struct has_escape_chars_impl {
  static const char* escapes8;
  static const wchar_t* escapes16;

  static bool impl(const std::string& s) {
    return s.find_first_of(escapes8) != std::string::npos;
  }
  static bool impl(const std::wstring& s) {
    return s.find_first_of(escapes16) != std::wstring::npos;
  }
};

const char* has_escape_chars_impl::escapes8 = "\"\\\b\f\r\n\t";
const wchar_t* has_escape_chars_impl::escapes16 = L"\"\\\b\f\r\n\t";

bool web::json::details::_String::has_escape_chars(const _String &str)
{
    return has_escape_chars_impl::impl(str.m_string);
}

web::json::details::_Object::_Object(const _Object& other):web::json::details::_Value(other)
{
    m_elements = other.m_elements;
}

web::json::value::value_type json::value::type() const { return m_value->type(); }

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

json::value& web::json::details::_Object::index(const utility::string_t &key)
{
    map_fields();

    auto whre = m_fields.find(key);

    if ( whre == m_fields.end() )
    {
        // Insert a new entry.
        m_elements.emplace_back(json::value::string(key), json::value::null());
        const size_t index = m_elements.size() - 1;
        m_fields.emplace(key, index);
        return m_elements[index].second;
    }

    return m_elements[whre->second].second;
}

const json::value& web::json::details::_Object::cnst_index(const utility::string_t &key) const
{
    const_cast<web::json::details::_Object*>(this)->map_fields();

    auto whre = m_fields.find(key);

    if ( whre == m_fields.end() )
        throw json::json_exception(_XPLATSTR("invalid field name"));

    return m_elements[whre->second].second;
}

bool web::json::details::_Object::has_field(const utility::string_t &key) const
{
    const_cast<web::json::details::_Object*>(this)->map_fields();
    return m_fields.find(key) != m_fields.end();
}

json::value web::json::details::_Object::get_field(const utility::string_t &key) const
{
    const_cast<web::json::details::_Object*>(this)->map_fields();

    auto whre = m_fields.find(key);

    if ( whre == m_fields.end() )
        return json::value();

    return m_elements[whre->second].second;
}

void web::json::details::_Object::map_fields()
{
    if ( m_fields.size() == 0 )
    {
        size_t index = 0;
        for (auto iter = m_elements.begin(); iter != m_elements.end(); ++iter, ++index)
        {
            m_fields.emplace(iter->first.as_string(), index);
        }
    }
}

utility::string_t json::value::to_string() const { return m_value->to_string(); }

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
    UNREACHABLE;
}

web::json::value& web::json::value::operator [] (const utility::string_t &key)
{
    if ( this->is_null() )
    {
        m_value.reset(new web::json::details::_Object());
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        m_kind = value::Object;
#endif
    }
    return m_value->index(key);
}

const web::json::value& web::json::value::operator [] (const utility::string_t &key) const
{
    if ( this->is_null() )
    {
        auto _nc_this = const_cast<web::json::value*>(this);
        _nc_this->m_value.reset(new web::json::details::_Object());
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        _nc_this->m_kind = value::Object;
#endif
    }
    return m_value->cnst_index(key);
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

const web::json::value& web::json::value::operator[](size_t index) const
{
    if ( this->is_null() )
    {
        auto _nc_this = const_cast<web::json::value*>(this);
        _nc_this->m_value.reset(new web::json::details::_Array());
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        _nc_this->m_kind = value::Array;
#endif
    }
    return m_value->cnst_index(index);
}
