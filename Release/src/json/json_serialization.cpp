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
* json_serialization.cpp
*
* HTTP Library: JSON parser and writer
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#pragma warning(disable : 4127) // allow expressions like while(true) pass 
using namespace web;
using namespace web::json;
using namespace utility;
using namespace utility::conversions;


//
// JSON Serialization
//

#ifdef _MS_WINDOWS
void web::json::value::serialize(std::ostream& stream) const
{ 
    // This has better performance than writing directly to stream.
    std::string str;
    m_value->serialize_impl(str);
    stream << str; 
}
void web::json::value::format(std::basic_string<wchar_t> &string) const 
{
    m_value->format(string); 
}
#endif

void web::json::value::serialize(utility::ostream_t &stream) const 
{
    // This has better performance than writing directly to stream.
    utility::string_t str;
    m_value->serialize_impl(str);
    stream << str;  
}
void web::json::value::format(std::basic_string<char>& string) const
{ 
    m_value->format(string); 
}

template<typename CharType>
std::basic_string<CharType> escape_string(const std::basic_string<CharType>& escaped)
{
    // Another opportunity for potentially better performance here is to
    // loop looking for the first character that needs to be escaped. Then
    // copy over the portion before, escape the character, and then continue searching.
    // This could be done using find_first_of.
    std::basic_string<CharType> tokenString = escaped;

    for (auto iter = tokenString.begin(); iter != tokenString.end(); ++iter)
    {
        switch(*iter)
        {
            // Insert the escape character.
        case '\"': 
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, '\"');
            break;
        case '\\': 
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, '\\');
            break;
        case '\b':
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, 'b');
            break;
        case '\f':
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, 'f');
            break;
        case '\r':
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, 'r');
            break;
        case '\n':
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, 'n');
            break;
        case '\t':
            tokenString.replace(iter, iter + 1, 1, '\\');
            iter = tokenString.insert(++iter, 't');
            break;
        }
    }

    return tokenString;
}

void web::json::details::_String::format(std::basic_string<char>& str) const
{
    str.push_back('"');

    if(is_wide())
    {
        if(m_has_escape_char)
        {
            str.append(escape_string(utf16_to_utf8(*m_wstring)));
        }
        else
        {
            str.append(utf16_to_utf8(*m_wstring));   
        }
    }
    else
    {
        if(m_has_escape_char)
        {
            str.append(escape_string(*m_string));
        }
        else
        {
            str.append(*m_string);
        }
    }

    str.push_back('"');
}

void web::json::details::_Number::format(std::basic_string<char>& stream) const 
{
    if(m_was_int)
    {
        // #digits + 1 to avoid loss + 1 for the sign + 1 for null terminator.
        const size_t tempSize = std::numeric_limits<int32_t>::digits10 + 3;
        char tempBuffer[tempSize];
#ifdef _MS_WINDOWS

        _itoa_s(m_intval, tempBuffer, tempSize, 10);
        const auto numChars = strnlen_s(tempBuffer, tempSize);
#else
        const auto numChars = std::snprintf(tempBuffer, tempSize, "%i", m_intval);
#endif
        stream.append(tempBuffer, numChars);
    }
    else
    {
        // #digits + 1 to avoid loss + 1 for the sign + 1 for decimal point + 5 for exponent (e+xxx)
        const size_t tempSize = std::numeric_limits<double>::digits10 + 8;
        char tempBuffer[tempSize];
#ifdef _MS_WINDOWS
        const auto numChars = sprintf_s(tempBuffer, tempSize, "%.*g", std::numeric_limits<double>::digits10, m_value);
#else
        const auto numChars = std::snprintf(tempBuffer, tempSize, "%.*g", std::numeric_limits<double>::digits10, m_value);
#endif
        stream.append(tempBuffer, numChars);
    }
}

#ifdef _MS_WINDOWS

void web::json::details::_String::format(std::basic_string<wchar_t>& str) const
{
    str.push_back(L'"');

    if(is_wide())
    {
        if(m_has_escape_char)
        {
            str.append(escape_string(*m_wstring));
        }
        else
        {
            str.append(*m_wstring);   
        }
    }
    else
    {
        if(m_has_escape_char)
        {
            str.append(utf8_to_utf16(*m_string));
        }
        else
{
            str.append(escape_string(utf8_to_utf16(*m_string)));
        }
    }

    str.push_back(L'"');
}

void web::json::details::_Number::format(std::basic_string<wchar_t>& stream) const
{
    if(m_was_int)
    {
        // #digits + 1 to avoid loss + 1 for the sign + 1 for null terminator.
        const size_t tempSize = std::numeric_limits<int32_t>::digits10 + 3;
        wchar_t tempBuffer[tempSize];
        _itow_s(m_intval, tempBuffer, tempSize, 10);
        stream.append(tempBuffer, wcsnlen_s(tempBuffer, tempSize));
    }
    else
    {
        // #digits + 1 to avoid loss + 1 for the sign + 1 for decimal point + 5 for exponent (e+xxx)
        const size_t tempSize = std::numeric_limits<double>::digits10 * 8;
        wchar_t tempBuffer[tempSize];
        const int numChars = swprintf_s(tempBuffer, tempSize, L"%.*g", std::numeric_limits<double>::digits10, m_value);
        stream.append(tempBuffer, numChars);
    }
}

#endif

utility::string_t web::json::details::_String::as_string() const
{
#ifdef _UTF16_STRINGS
    return as_utf16_string();
#else
    return as_utf8_string();
#endif
}

std::string web::json::details::_String::as_utf8_string() const
{
    if(is_wide())
    {
        return utf16_to_utf8(*m_wstring.get());
    }
    else
    {
        return *m_string.get();
    }
}

utf16string web::json::details::_String::as_utf16_string() const
{
    if(is_wide())
    {
        return *m_wstring.get();
    }
    else
    {
        return utf8_to_utf16(*m_string.get());
    }
}

bool web::json::details::_String::has_escape_chars(const _String &str)
{
	if (str.is_wide())
	{
#ifdef _MS_WINDOWS
        const wchar_t *escapes = L"\"\\\b\f\r\n\t";
#else
        const utf16char *escapes = u"\"\\\b\f\r\n\t";
#endif
		return str.m_wstring->find_first_of(escapes) != std::wstring::npos;
	}
	else
	{
		const char *escapes = "\"\\\b\f\r\n\t";
		return str.m_string->find_first_of(escapes) != std::string::npos;
	}
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
        m_elements.push_back(std::make_pair(json::value::string(key), json::value::null()));
        size_t index = m_elements.size()-1;
        m_fields[key] = index;
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
            m_fields[iter->first.as_string()] = index;
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
        return this->as_double() == other.as_double();
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

double web::json::value::as_double() const
{
    return m_value->as_double();
}
 
int32_t web::json::value::as_integer() const
{
    return m_value->as_integer();
}
 
bool web::json::value::as_bool() const
{
    return m_value->as_bool();
}

utility::string_t web::json::value::as_string() const
{
    return m_value->as_string();
}

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
web::json::value web::json::value::string(std::string value)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_String>(std::move(value));
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

web::json::value web::json::value::object(const web::json::value::field_map &fields)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Object>(fields);
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

web::json::value web::json::value::array(const json::value::element_vector &elements)
{
    std::unique_ptr<details::_Value> ptr = utility::details::make_unique<details::_Array>(elements);
    return web::json::value(std::move(ptr)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,value::Array
#endif
            );
}

#pragma endregion