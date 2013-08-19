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

#pragma warning(disable : 4127) // allow expressions like while(true) pass 
using namespace web; using namespace utility;
using namespace web::json;
using namespace utility::conversions;

//
// Template trickery: allow the definition of string literals that can be used within a template
// parameterized by character type
//
struct string_literal_holder
{
    const char *n;
    const utf16char *w;
};
#ifdef _MS_WINDOWS
#define declare_string_literal(name, value) string_literal_holder name = {value, L##value}
#else
#define declare_string_literal(name, value) string_literal_holder name = {value, u##value}
#endif

template <typename CharType>
struct string_literal_extractor
{
    static const utf16char* get(const string_literal_holder& st) { return st.w; }
};

template <>
struct string_literal_extractor<char>
{
    static const char* get(const string_literal_holder& st) { return st.n; }
};

template <typename CharType>
struct string_literal_comparer
{
    static bool is_equal(const utf16string& str, const string_literal_holder& sh) { return str == sh.w; }
};

template <>
struct string_literal_comparer<char>
{
    static bool is_equal(const std::string& str, const string_literal_holder& sh) { return str == sh.n; }
};

// Serialization

#ifdef _MS_WINDOWS
void web::json::value::serialize(std::ostream& stream) const
{ 
    m_value->format(stream); 
}
void web::json::value::format(std::basic_string<wchar_t> &string) const 
{
    m_value->format(string); 
}
#endif

void web::json::value::serialize(utility::ostream_t &stream) const 
{
    m_value->format(stream); 
}
void web::json::value::format(std::basic_string<char>& string) const
{ 
    m_value->format(string); 
}

#ifdef _MS_WINDOWS
void web::json::details::_Object::format(std::basic_string<wchar_t>& os) const
{
    os.push_back(L'{');
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os.push_back(L',');

        os.push_back(L'"');
        auto _str = reinterpret_cast<web::json::details::_String*>(iter->first.m_value.get());
        if (_str->is_wide())
        {
            os.append(*_str->m_wstring.get());
        }
        else
        {
            os.append(utility::conversions::to_basic_string<wchar_t>::perform(_str->as_utf16_string()));
        }
        os.append(L"\":");
        iter->second.format(os);
        first = false;
    }
    os.push_back(L'}');
}

void web::json::details::_Object::format(std::basic_ostream<wchar_t>& os) const
{
    os << L'{';
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os << L',';
        os << L'"'; 
        auto _str = reinterpret_cast<web::json::details::_String*>(iter->first.m_value.get());
        if (_str->is_wide())
        {
            os << *_str->m_wstring.get();
        }
        else
        {
            os << utility::conversions::to_basic_string<wchar_t>::perform(_str->as_utf16_string());
        }
        os << L"\":";
        iter->second.serialize(os);
        first = false;
    }
    os << L'}';
}

void web::json::details::_Array::format(std::basic_string<wchar_t>& os) const
{
    os.push_back(L'[');
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os.push_back(L',');
        iter->second.format(os);
        first = false;
    }
    os.push_back(L']');
}

void web::json::details::_Array::format(std::basic_ostream<wchar_t>& os) const
{
    os << L'[';
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os << L',';
        iter->second.serialize(os);
        first = false;
    }
    os << L']';
}
#endif

void web::json::details::_Object::format(std::basic_string<char>& os) const
{
    os.push_back('{');
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os.push_back(',');

        os.push_back('"');
        auto _str = reinterpret_cast<web::json::details::_String*>(iter->first.m_value.get());
        if (!_str->is_wide())
        {
            os.append(*_str->m_string.get());
        }
        else
        {
            os.append(utility::conversions::to_basic_string<char>::perform(_str->as_utf8_string()));
        }
        os.append("\":");
        iter->second.format(os);
        first = false;
    }
    os.push_back('}');
}

void web::json::details::_Object::format(std::basic_ostream<char>& os) const
{
    os << '{';
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os << ',';
        os << '"'; 
        auto _str = reinterpret_cast<web::json::details::_String*>(iter->first.m_value.get());
        if (!_str->is_wide())
        {
            os << *_str->m_string.get();
        }
        else
        {
            os << utility::conversions::to_basic_string<char>::perform(_str->as_utf8_string());
        }
        os << "\":";
        iter->second.serialize(os);
        first = false;
    }
    os << '}';
}

void web::json::details::_Array::format(std::basic_string<char>& os) const
{
    os.push_back('[');
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os.push_back(',');
        iter->second.format(os);
        first = false;
    }
    os.push_back(']');
}

void web::json::details::_Array::format(std::basic_ostream<char>& os) const
{
    os << '[';
    bool first = true;
    for (auto iter = m_elements.begin(); iter != m_elements.end(); iter++)
    {
        if (!first) os << ',';
        iter->second.serialize(os);
        first = false;
    }
    os << ']';
}

int _hexval [] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
                   -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

template<typename CharType>
CharType get_unescaped_char(typename std::basic_string<CharType>::iterator & inputIter)
{
    CharType ch;
    switch ( ch = *(++inputIter))
    {
    case '\"': 
    case '\\': 
    case '/': 
        break;
    case 'b':
        ch = '\b';
        break;
    case 'f':
        ch = '\f';
        break;
    case 'r':
        ch = '\r';
        break;
    case 'n':
        ch = '\n';
        break;
    case 't':
        ch = '\t';
        break;
    case 'u':
    {
        // A four-hexdigit unicode character
        int decoded = 0;
        for (int i = 0; i < 4; i++)
        {
            CharType c = *(++inputIter);
            _ASSERTE(isxdigit((unsigned char)(c)));
            int val = _hexval[c];
            decoded |= (val << (4*(3-i)));
        }
        ch = static_cast<CharType>(decoded & 0xFFFF);
        break;
    }
    default:
        throw json::json_exception(_XPLATSTR("invalid escape character in string literal"));
    }
    return ch;
}

// Unescape string in-place
template<typename CharType>
void unescape_string(std::basic_string<CharType>& escaped)
{
    auto outputIter = escaped.begin();

    // The outer loop advances the iterators until we have found an escape character
    for(auto inputIter = outputIter; inputIter != escaped.end(); ++inputIter )
    {
        CharType ch = (CharType)*inputIter;
        if ( ch == '\\' )
        {
            // We've found an escape character. Unescape it, and copy into the output stream
            // Scan the rest of the string and copy every character (unscaping it if necessary) into the ouput stream
            ch = get_unescaped_char<CharType>(inputIter);
            *outputIter++ = ch;
            ++inputIter;
            for(; inputIter != escaped.end(); ++inputIter )
            {
                ch = (CharType)*inputIter;
                if ( ch == '\\' )
                {
                    ch = get_unescaped_char<CharType>(inputIter);
                }
                *outputIter++ = ch;
            }
            escaped.resize(outputIter - escaped.begin());
            return;
        }
        ++outputIter;
    }
}

template<typename CharType>
std::basic_string<CharType> escape_string(const std::basic_string<CharType>& escaped)
{
    std::basic_string<CharType> tokenString;

    for (auto iter = escaped.begin(); iter != escaped.end(); iter++ )
    {
        CharType ch = (CharType)*iter;

        switch(ch)
        {
            // Insert the escape character.
            case '\"': 
                tokenString.push_back('\\');
                tokenString.push_back('\"');
                break;
            case '\\': 
                tokenString.push_back('\\');
                tokenString.push_back('\\');
                break;
            case '\b':
                tokenString.push_back('\\');
                tokenString.push_back('b');
                break;
            case '\f':
                tokenString.push_back('\\');
                tokenString.push_back('f');
                break;
            case '\r':
                tokenString.push_back('\\');
                tokenString.push_back('r');
                break;
            case '\n':
                tokenString.push_back('\\');
                tokenString.push_back('n');
                break;
            case '\t':
                tokenString.push_back('\\');
                tokenString.push_back('t');
                break;
            default:
                tokenString.push_back(ch);
                break;
        }
    }

    return tokenString;
}

void web::json::details::_String::format(std::basic_string<char>& stream) const
{
    stream.push_back('"');
    stream.append(escape_string(as_utf8_string())).push_back('"');
}

#ifdef _MS_WINDOWS
void web::json::details::_String::format(std::basic_string<wchar_t>& stream) const
{
    stream.push_back(L'"');
    stream.append(escape_string(as_utf16_string())).push_back(L'"');
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

web::json::details::_Object::_Object(const _Object& other):web::json::details::_Value(other)
{
    m_elements = other.m_elements;
}

web::json::value::value_type json::value::type() const { return m_value->type(); }

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
    return false;
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

#pragma endregion

namespace web {
namespace json
{
namespace details
{

//
// JSON Parsing
//
template <typename CharType>
class JSON_Parser
{
public:
    JSON_Parser() 
        : m_currentLine(1),
          m_eof(std::char_traits<CharType>::eof()),
          m_currentColumn(1),
          m_currentParsingDepth(0)
    {
        declare_string_literal(n, "null");
        declare_string_literal(t, "true");
        declare_string_literal(f, "false");
        m_null = n;
        m_true = t;
        m_false = f;
    }

    struct Location
    {
        size_t m_line;
        size_t m_column;
    };

    struct Token
    {
        enum Kind
        {
            TKN_EOF,

            TKN_OpenBrace,
            TKN_CloseBrace,
            TKN_OpenBracket,
            TKN_CloseBracket,
            TKN_Comma,
            TKN_Colon,
            TKN_StringLiteral,
            TKN_NumberLiteral,
            TKN_IntegerLiteral,
            TKN_BooleanLiteral,
            TKN_NullLiteral,
            TKN_Comment
        };

        Token() : kind(TKN_EOF) {}
        Token(Kind kind) : kind(kind) {}

        Kind kind;
        std::basic_string<CharType> spelling;

        typename JSON_Parser<CharType>::Location start;
        typename JSON_Parser<CharType>::Location end;
    };

    void GetNextToken(Token &);

    web::json::value ParseValue(typename JSON_Parser<CharType>::Token &first)
    {
        auto _value = _ParseValue(first);
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        auto type = _value->type();
#endif
        return web::json::value(std::move(_value)
#ifdef ENABLE_JSON_VALUE_VISUALIZER
            ,type
#endif
            );
    }

protected:
    virtual CharType NextCharacter() = 0;
    virtual CharType PeekCharacter() = 0;

    virtual bool CompleteKeyword(const CharType *expected, size_t expected_size, typename Token::Kind kind, Token &token);
    virtual bool CompleteComment(CharType first, Token &token);
    virtual bool CompleteNumberLiteral(CharType first, Token &token);
    virtual bool CompleteStringLiteral(CharType first, Token &token);

private:

    std::unique_ptr<web::json::details::_Value> _ParseValue(typename JSON_Parser<CharType>::Token &first);
    std::unique_ptr<web::json::details::_Object> _ParseObject(typename JSON_Parser<CharType>::Token &tkn);
    std::unique_ptr<web::json::details::_Array> _ParseArray(typename JSON_Parser<CharType>::Token &tkn);

    JSON_Parser& operator=(const JSON_Parser&);

    CharType EatWhitespace();

    void CreateToken(typename JSON_Parser<CharType>::Token& tk, typename Token::Kind kind, Location &start)
    {
        tk.kind = kind;
        tk.start = start; 
        tk.end = start; 
        tk.end.m_column++;
        tk.spelling.clear();
    }

    void CreateToken(typename JSON_Parser<CharType>::Token& tk, typename Token::Kind kind, Location &start, Location &end)
    {
        tk.kind = kind;
        tk.start = start; 
        tk.end = end; 
        tk.spelling.clear();
    }

    void CreateToken(typename JSON_Parser<CharType>::Token& tk, typename Token::Kind kind)
    {
        tk.kind = kind;
        tk.start.m_line = m_currentLine; 
        tk.start.m_column = m_currentColumn; 
        tk.end = tk.start; 
        tk.spelling.clear();
    }

#ifdef _MS_WINDOWS
    __declspec(noreturn)
#else
    __attribute__((noreturn))
#endif
    void CreateError(const typename JSON_Parser<CharType>::Token &tk, const utility::string_t &message);

protected:

    size_t m_currentLine;
    size_t m_currentColumn;
    size_t m_currentParsingDepth;
    static const size_t maxParsingDepth = 128;

    typename std::char_traits<CharType>::int_type m_eof;

private:

    string_literal_holder m_null;
    string_literal_holder m_true;
    string_literal_holder m_false;
};

template <typename CharType>
class JSON_StreamParser : public JSON_Parser<CharType>
    {
public:
    JSON_StreamParser(std::basic_istream<CharType> &stream) 
        : m_streambuf(stream.rdbuf())
    {
    }

protected:

    virtual CharType NextCharacter();
    virtual CharType PeekCharacter();

private:
    typename std::basic_streambuf<CharType, std::char_traits<CharType>>* m_streambuf;
};

template <typename CharType>
class JSON_StringParser : public JSON_Parser<CharType>
{
public:
    JSON_StringParser(const std::basic_string<CharType>& string) 
        : m_string(string),
          m_position(&string[0])
    {
        m_startpos = m_position;
        m_endpos = m_position+string.size();
    }

protected:

    virtual CharType NextCharacter();
    virtual CharType PeekCharacter();

    virtual bool CompleteKeyword(const CharType *expected, size_t expected_size, typename JSON_Parser<CharType>::Token::Kind kind, typename JSON_Parser<CharType>::Token &token);
    virtual bool CompleteComment(CharType first, typename JSON_Parser<CharType>::Token &token);
    virtual bool CompleteNumberLiteral(CharType first, typename JSON_Parser<CharType>::Token &token);
    virtual bool CompleteStringLiteral(CharType first, typename JSON_Parser<CharType>::Token &token);

private:
    const CharType* m_position;
    const CharType* m_startpos;
    const CharType* m_endpos;

    const std::basic_string<CharType>& m_string;
};


template <typename CharType>
void JSON_Parser<CharType>::CreateError(const typename JSON_Parser<CharType>::Token &tk, const utility::string_t &message)
{
    utility::ostringstream_t os;
    os << message << _XPLATSTR(" near line ") << tk.start.m_line << _XPLATSTR(", column ") << tk.start.m_column;
    throw web::json::json_exception(os.str().c_str());
}

template <typename CharType>
CharType JSON_StreamParser<CharType>::NextCharacter()
{
    CharType ch = (CharType)m_streambuf->sbumpc();

    if ( ch == this->m_eof ) return (CharType)ch;

    if ( ch == '\n' )
    {
        this->m_currentLine += 1;
        this->m_currentColumn = 0;
    }
    else
    {
        this->m_currentColumn += 1;
    }

    return (CharType)ch;
}

template <typename CharType>
CharType JSON_StreamParser<CharType>::PeekCharacter()
{
    return (CharType)m_streambuf->sgetc();
}

template <typename CharType>
CharType JSON_StringParser<CharType>::NextCharacter()
{
    if ( m_position == m_endpos ) return (CharType)this->m_eof;

    CharType ch = *m_position;
    m_position += 1;

    if ( m_position == m_endpos ) return (CharType)ch;

    if ( ch == '\n' )
    {
        this->m_currentLine += 1;
        this->m_currentColumn = 0;
    }
    else
    {
        this->m_currentColumn += 1;
    }

    return (CharType)ch;
}

template <typename CharType>
CharType JSON_StringParser<CharType>::PeekCharacter()
{
    if ( m_position == m_endpos ) return (CharType)this->m_eof;

    return (CharType)*m_position;
}

//
// Consume whitespace characters and return the first non-space character or EOF
//
template <typename CharType>
CharType JSON_Parser<CharType>::EatWhitespace()
{
   CharType ch = NextCharacter();

   while ( ch != this->m_eof && iswspace((int)ch) )
   {
       ch = NextCharacter();
   }

   return ch;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteKeyword(const CharType *expected, size_t expected_size, typename Token::Kind kind, Token &token)
{
    UNREFERENCED_PARAMETER(expected_size);

    token.spelling.push_back(expected[0]);

    const CharType *ptr = expected+1;
    CharType ch = NextCharacter();

    while ( ch != this->m_eof && *ptr != '\0')
    {
        if ( ch != *ptr ) 
        {
            return false;
        }
        ptr++;

        token.spelling.push_back(ch);

        if ( *ptr == '\0' ) break;

        ch = NextCharacter();
    }

    token.kind = kind;
    token.end.m_column = m_currentColumn;
    token.end.m_line = m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_StringParser<CharType>::CompleteKeyword(const CharType *expected, size_t expected_size, typename JSON_Parser<CharType>::Token::Kind kind, typename JSON_Parser<CharType>::Token &token)
{
    const CharType *ptr = expected+1;
    CharType ch = JSON_StringParser<CharType>::NextCharacter();

    while ( ch != this->m_eof && *ptr != '\0')
    {
        if ( ch != *ptr ) 
        {
            return false;
        }
        ptr++;

        if ( *ptr == '\0' ) break;

        ch = JSON_StringParser<CharType>::NextCharacter();
    }

    token.spelling.resize(expected_size);
    memcpy(&token.spelling[0], expected, (expected_size)*sizeof(CharType));

    token.kind = kind;
    token.end.m_column = this->m_currentColumn;
    token.end.m_line = this->m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteNumberLiteral(CharType first, Token &token)
{
    //
    // First, find the boundaries of the literal. Then, validate that it has the right format.
    //
    token.spelling.push_back(first);

    bool could_be_integer = true;

    CharType ch = first;

    while ( ch != this->m_eof)
    {
        ch = PeekCharacter();

        switch ( ch )
        {
        default:
            goto boundary_found;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'e':
        case 'E':
        case '.':
        case '-':
        case '+':
            break;
        }

        token.spelling.push_back(ch);
        ch = NextCharacter();
    }

boundary_found:

    size_t idx = 0;
    size_t end = token.spelling.size();

    if ( token.spelling[idx] == '-' )
    {
        idx = 1;
    }

    if ( token.spelling[idx] == '0' )
    {
        if ( idx == end-1) goto done;

        switch(token.spelling[idx+1])
        {
        default:
            return false;
        case '.':
            idx += 2;
            could_be_integer = false;
            goto found_decimal_point;
        }
    }

    switch ( token.spelling[idx] )
    {
    default:
        return false;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        idx += 1;
        break;
    }

    for (; idx < end; idx++)
    {
        switch ( token.spelling[idx] )
        {
        default:
            return false;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        case '.':
            idx += 1;
            could_be_integer = false;
            goto found_decimal_point;
        case 'e':
        case 'E':
            idx += 1;
            could_be_integer = false;
            goto found_exponent;
        }
    }

    goto done;

found_decimal_point:

    if ( idx == end ) return false;

    for (; idx < end; idx++)
    {
        switch ( token.spelling[idx] )
        {
        default:
            return false;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        case 'e':
        case 'E':
            idx += 1;
            goto found_exponent;
        }
    }

    goto done;

found_exponent:

    if ( idx == end ) return false;

    if ( token.spelling[idx] == '-' || token.spelling[idx] == '+' )
    {
        idx += 1;
    }

    if ( idx == end ) return false;

    for (; idx < end; idx++)
    {
        switch ( token.spelling[idx] )
        {
        default:
            return false;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        }
    }

done:
    token.kind = could_be_integer ? Token::TKN_IntegerLiteral : Token::TKN_NumberLiteral;
    token.end.m_column = m_currentColumn;
    token.end.m_line = m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_StringParser<CharType>::CompleteNumberLiteral(CharType first, typename JSON_Parser<CharType>::Token &token)
{
    //
    // First, find the boundaries of the literal. Then, validate that it has the right format.
    //
    auto first_pos = m_position-1;

    bool could_be_integer = true;

    CharType ch = first;

    while ( ch != this->m_eof)
    {
        ch = JSON_StringParser<CharType>::PeekCharacter();

        switch ( ch )
        {
        default:
            goto boundary_found;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'e':
        case 'E':
        case '.':
        case '-':
        case '+':
            break;
        }

        ch = JSON_StringParser<CharType>::NextCharacter();
    }

boundary_found:

    // This is the index of the first character **not** in the literal.
    auto last_pos = m_position;

    auto idx = first_pos;

    if ( idx[0] == '-' )
    {
        idx += 1;
    }

    if ( idx[0] == '0' )
    {
        if ( idx == last_pos-1) goto done;

        switch(idx[1])
        {
        default:
            return false;
        case '.':
            idx += 2;
            could_be_integer = false;
            goto found_decimal_point;
        }
    }

    switch ( idx[0] )
    {
    default:
        return false;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        idx += 1;
        break;
    }

    for (; idx < last_pos; idx++)
    {
        switch ( idx[0] )
        {
        default:
            return false;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        case '.':
            idx += 1;
            could_be_integer = false;
            goto found_decimal_point;
        case 'e':
        case 'E':
            idx += 1;
            could_be_integer = false;
            goto found_exponent;
        }
    }

    goto done;

found_decimal_point:

    if ( idx == last_pos ) return false;

    for (; idx < last_pos; idx++)
    {
        switch ( idx[0] )
        {
        default:
            return false;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        case 'e':
        case 'E':
            idx += 1;
            goto found_exponent;
        }
    }

    goto done;

found_exponent:

    if ( idx == last_pos ) return false;

    if ( idx[0] == '-' || idx[0] == '+' )
    {
        idx += 1;
    }

    if ( idx == last_pos ) return false;

    for (; idx < last_pos; idx++)
    {
        switch ( idx[0] )
        {
        default:
            return false;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        }
    }

done:
    token.spelling.resize(last_pos-first_pos);
    memcpy(&token.spelling[0], first_pos, (last_pos-first_pos)*sizeof(CharType));

    token.kind = could_be_integer ? (JSON_Parser<CharType>::Token::TKN_IntegerLiteral) : (JSON_Parser<CharType>::Token::TKN_NumberLiteral);
    token.end.m_column = this->m_currentColumn;
    token.end.m_line = this->m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteComment(CharType first, Token &token)
{
    // We already found a '/' character as the first of a token -- what kind of comment is it?

    UNREFERENCED_PARAMETER(first);

    CharType ch = NextCharacter();

    if ( ch == this->m_eof || (ch != '/' && ch != '*') )
        return false;

    if ( ch == '/' )
    {
        // Line comment -- look for a newline or EOF to terminate.

        ch = NextCharacter();

        while ( ch != this->m_eof && ch != '\n')
        {
            token.spelling.push_back(ch);
            ch = NextCharacter();
        }
    }
    else
    {
        // Block comment -- look for a terminating "*/" sequence.

        ch = NextCharacter();

        while ( true )
        {
            if ( ch == this->m_eof )
                return false;

            if ( ch == '*' )
            {
                CharType ch1 = PeekCharacter();

                if ( ch1 == this->m_eof )
                    return false;

                if ( ch1 == '/' )
                {
                    // Consume the character
                    NextCharacter();
                    break;
                }

                token.spelling.push_back(ch);
                ch = ch1;
            }

            token.spelling.push_back(ch);
            ch = NextCharacter();
        }
    }

    token.kind = Token::TKN_Comment;
    token.end.m_column = m_currentColumn;
    token.end.m_line = m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_StringParser<CharType>::CompleteComment(CharType first, typename JSON_Parser<CharType>::Token &token)
{
    // This function is specialized for the string parser, since we can be slightly more
    // efficient in copying data from the input to the token: do a memcpy() rather than
    // one character at a time.

    // first is expected to contain "\""
    UNREFERENCED_PARAMETER(first);

    CharType ch = JSON_StringParser<CharType>::NextCharacter();

    if ( ch == this->m_eof || (ch != '/' && ch != '*') )
        return false;

    auto start = m_position;
    auto end   = m_position;

    if ( ch == '/' )
    {
        // Line comment -- look for a newline or EOF to terminate.

        ch = JSON_StringParser<CharType>::NextCharacter();

        while ( ch != this->m_eof && ch != '\n')
        {
            end = m_position;
            ch = JSON_StringParser<CharType>::NextCharacter();
        }
    }
    else
    {
        // Block comment -- look for a terminating "*/" sequence.

        ch = JSON_StringParser<CharType>::NextCharacter();

        while ( true )
        {
            if ( ch == this->m_eof )
                return false;

            if ( ch == '*' )
            {
                ch = JSON_StringParser<CharType>::PeekCharacter();

                if ( ch == this->m_eof )
                    return false;

                if ( ch == '/' )
                {
                    // Consume the character
                    JSON_StringParser<CharType>::NextCharacter();
                    end = m_position-2;
                    break;
                }

            }

            end = m_position;
            ch = JSON_StringParser<CharType>::NextCharacter();
        }
    }

    token.spelling.resize(end-start);
    if ( token.spelling.size() > 0 )
        memcpy(&token.spelling[0], start, (end-start)*sizeof(CharType));

    token.kind = JSON_Parser<CharType>::Token::TKN_Comment;
    token.end.m_column = this->m_currentColumn;
    token.end.m_line = this->m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteStringLiteral(CharType first, Token &token)
{
    UNREFERENCED_PARAMETER(first);

    CharType ch = NextCharacter();

    while ( ch != this->m_eof && ch != '"')
    {
        if ( ch == '\\' )
        {          
            token.spelling.push_back(ch);

            switch ( ch = NextCharacter() )
            {
            case '\"': 
            case '\\': 
            case '/': 
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            case 'u':
            {
                // A four-hexdigit unicode character
                for (int i = 0; i < 4; i++)
                {
                    token.spelling.push_back(ch);
                    ch = NextCharacter();
                    if ( !isxdigit((unsigned char)(ch))) return false;
                }
                break;
            }
            default:
                return false;
            }
        }
        else if ( ch > CharType(0x0) && ch < CharType(0x20) )
        {
            return false;
        }

        token.spelling.push_back(ch);

        ch = NextCharacter();
    }

    if ( ch == '"' )
    {
        token.kind = Token::TKN_StringLiteral;
        token.end.m_column = m_currentColumn;
        token.end.m_line = m_currentLine;
    }
    else
    {
        return false;
    }

    return true;
}

template <typename CharType>
bool JSON_StringParser<CharType>::CompleteStringLiteral(CharType first, typename JSON_Parser<CharType>::Token &token)
{
    // This function is specialized for the string parser, since we can be slightly more
    // efficient in copying data from the input to the token: do a memcpy() rather than
    // one character at a time.

    // first is expected to contain "\""
    UNREFERENCED_PARAMETER(first);

    auto start = m_position;

    CharType ch = JSON_StringParser<CharType>::NextCharacter();

    while ( ch != this->m_eof && ch != '"')
    {
        if ( ch == '\\' )
        {
            m_position = start;
            return JSON_Parser<CharType>::CompleteStringLiteral(first, token);
        }
        else if ( ch > CharType(0x0) && ch < CharType(0x20) )
        {
            return false;
        }

        ch = JSON_StringParser<CharType>::NextCharacter();
    }

    if ( ch == '"' )
    {
        token.spelling.resize(m_position-start-1);
        if ( token.spelling.size() > 0 )
            memcpy(&token.spelling[0], start, (m_position-start-1)*sizeof(CharType));

        token.kind = JSON_Parser<CharType>::Token::TKN_StringLiteral;
        token.end.m_column = this->m_currentColumn;
        token.end.m_line = this->m_currentLine;
    }
    else
    {
        return false;
    }

    return true;
}

template <typename CharType>
void JSON_Parser<CharType>::GetNextToken(typename JSON_Parser<CharType>::Token& result)
{
try_again:
    CharType ch = EatWhitespace();

    CreateToken(result, Token::TKN_EOF);

    if ( ch == this->m_eof ) return;

    switch ( ch )
    {
    case '{':
    case '[':
        {
            if(++m_currentParsingDepth >= JSON_Parser<CharType>::maxParsingDepth)
            {
                CreateError(result, _XPLATSTR("Nesting too deep!"));
                break;
            }

            typename JSON_Parser<CharType>::Token::Kind tk = ch == '{' ? Token::TKN_OpenBrace : Token::TKN_OpenBracket;
            CreateToken(result, tk, result.start);
            break;
        }
    case '}':
    case ']':
        {
            if((signed int)(--m_currentParsingDepth) < 0)
            {
                CreateError(result, _XPLATSTR("Mismatched braces!"));
                break;
            }

            typename JSON_Parser<CharType>::Token::Kind tk = ch == '}' ? Token::TKN_CloseBrace : Token::TKN_CloseBracket;
            CreateToken(result, tk, result.start);
            break;
        }
    case ',':
        CreateToken(result, Token::TKN_Comma, result.start);
        break;

    case ':':
        CreateToken(result, Token::TKN_Colon, result.start);
        break;

    case 't':
        if ( !CompleteKeyword(string_literal_extractor<CharType>::get(m_true), 4, Token::TKN_BooleanLiteral, result) ) CreateError(result, _XPLATSTR("Malformed literal"));
        break;
    case 'f':
        if ( !CompleteKeyword(string_literal_extractor<CharType>::get(m_false), 5, Token::TKN_BooleanLiteral, result) ) CreateError(result, _XPLATSTR("Malformed literal"));
        break;
    case 'n':
        if ( !CompleteKeyword(string_literal_extractor<CharType>::get(m_null), 4, Token::TKN_NullLiteral, result) ) CreateError(result, _XPLATSTR("Malformed literal"));
        break;
    case '/':
        if ( !CompleteComment(ch, result) ) CreateError(result, _XPLATSTR("Malformed comment"));
        // For now, we're ignoring comments.
        goto try_again;
    case '"':
        if ( !CompleteStringLiteral(ch, result) ) CreateError(result, _XPLATSTR("Malformed string literal"));
        break;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if ( !CompleteNumberLiteral(ch, result) ) CreateError(result, _XPLATSTR("Malformed numeric literal"));
        break;
    default:
        CreateError(result, _XPLATSTR("Malformed token"));
        break;
    }
}

template <typename CharType>
std::unique_ptr<web::json::details::_Object> JSON_Parser<CharType>::_ParseObject(typename JSON_Parser<CharType>::Token &tkn)
{
    GetNextToken(tkn);

    auto obj = utility::details::make_unique<web::json::details::_Object>();

    if ( tkn.kind != JSON_Parser<CharType>::Token::TKN_CloseBrace ) 
    {
        while ( true )
        {
            // State 1: New field or end of object, looking for field name or closing brace

            std::basic_string<CharType> fieldName;
        
            switch ( tkn.kind )
            {
            case JSON_Parser<CharType>::Token::TKN_StringLiteral:
                fieldName = std::move(tkn.spelling);
                break;
            default:
                goto error;
            }

            GetNextToken(tkn);

            // State 2: Looking for a colon.

            if ( tkn.kind != JSON_Parser<CharType>::Token::TKN_Colon ) goto done;

            GetNextToken(tkn);

            // State 3: Looking for an expression.

            auto fieldValue = _ParseValue(tkn);

#ifdef ENABLE_JSON_VALUE_VISUALIZER
            auto type = fieldValue->type();
#endif

            obj->m_elements.push_back(std::make_pair(
                json::value::string(to_string_t(fieldName)),
#ifdef ENABLE_JSON_VALUE_VISUALIZER
                json::value(std::move(fieldValue), type)));
#else
                json::value(std::move(fieldValue))));
#endif

            // State 4: Looking for a comma or a closing brace

            switch (tkn.kind)
            {
            case JSON_Parser<CharType>::Token::TKN_Comma:
                GetNextToken(tkn);
                break;
            case JSON_Parser<CharType>::Token::TKN_CloseBrace:
                goto done;
            default:
                goto error;
            }
        }
    }

done:

    GetNextToken(tkn);

    return obj;

error:

    CreateError(tkn, _XPLATSTR("Malformed object literal"));
}

template <typename CharType>
std::unique_ptr<web::json::details::_Array> JSON_Parser<CharType>::_ParseArray(typename JSON_Parser<CharType>::Token &tkn)
{
    GetNextToken(tkn);

    auto result = utility::details::make_unique<web::json::details::_Array>();

    if ( tkn.kind != JSON_Parser<CharType>::Token::TKN_CloseBracket ) 
    {
        int index = 0;

        while ( true )
        {
            // State 1: Looking for an expression.

            json::value elementValue = ParseValue(tkn);

            result->m_elements.push_back(std::make_pair<json::value,json::value>(json::value::number(index++), std::move(elementValue)));

            // State 4: Looking for a comma or a closing bracket

            switch (tkn.kind)
            {
            case JSON_Parser<CharType>::Token::TKN_Comma:
                GetNextToken(tkn);
                break;
            case JSON_Parser<CharType>::Token::TKN_CloseBracket:
                goto done;
            default:
                goto error;
            }
        }

    }
     
done:
    GetNextToken(tkn);

    return result;

error:

    CreateError(tkn, _XPLATSTR("Malformed array literal"));
}

template <typename CharType>
std::unique_ptr<web::json::details::_Value> JSON_Parser<CharType>::_ParseValue(typename JSON_Parser<CharType>::Token &tkn)
{
    switch (tkn.kind)
    {
        case JSON_Parser<CharType>::Token::TKN_OpenBrace:
            return _ParseObject(tkn);

        case JSON_Parser<CharType>::Token::TKN_OpenBracket:
            return _ParseArray(tkn);

        case JSON_Parser<CharType>::Token::TKN_StringLiteral:
            {
                unescape_string(tkn.spelling);
                auto value = utility::details::make_unique<web::json::details::_String>(std::move(tkn.spelling));
                GetNextToken(tkn);
                return std::move(value);
            }

        case JSON_Parser<CharType>::Token::TKN_IntegerLiteral:
            {
                std::unique_ptr<web::json::details::_Number> value;

                try
                {
                    std::basic_istringstream<CharType> is(std::move(tkn.spelling));
                    int32_t i;
                    is >> i;
                    value = utility::details::make_unique<web::json::details::_Number>(i);
                }
                catch(...)
                {
                    std::basic_istringstream<CharType> is(std::move(tkn.spelling));
                    double d;
                    is >> d;
                    value = utility::details::make_unique<web::json::details::_Number>(d);
                }

                GetNextToken(tkn);
                return std::move(value);
            }

        case JSON_Parser<CharType>::Token::TKN_NumberLiteral:  
            {
                std::basic_istringstream<CharType> is(std::move(tkn.spelling));
                double d;
                is >> d;
                auto value = utility::details::make_unique<web::json::details::_Number>(d);
                GetNextToken(tkn);
                return std::move(value);
            }
        case JSON_Parser<CharType>::Token::TKN_BooleanLiteral:
            {
                auto value = utility::details::make_unique<web::json::details::_Boolean>(string_literal_comparer<CharType>::is_equal(tkn.spelling,m_true));
                GetNextToken(tkn);
                return std::move(value);
            }
        case JSON_Parser<CharType>::Token::TKN_NullLiteral:
            {
                auto value = utility::details::make_unique<web::json::details::_Null>();
                GetNextToken(tkn);
                return std::move(value);
            }

        default:
            CreateError(tkn, _XPLATSTR("Unexpected token"));
    }
}

}}}

static web::json::value _parse_stream(utility::istream_t &stream)
{
    web::json::details::JSON_StreamParser<utility::char_t> parser(stream);

    web::json::details::JSON_Parser<utility::char_t>::Token tkn;
    parser.GetNextToken(tkn);

    auto value = parser.ParseValue(tkn);
    if ( tkn.kind != web::json::details::JSON_Parser<utility::char_t>::Token::TKN_EOF )
    {
        throw web::json::json_exception(_XPLATSTR("Left-over characters in stream after parsing a JSON value."));
    }
    return value;
}

#ifdef _MS_WINDOWS
static web::json::value _parse_narrow_stream(std::istream &stream)
{
    web::json::details::JSON_StreamParser<char> parser(stream);

    web::json::details::JSON_StreamParser<char>::Token tkn;
    parser.GetNextToken(tkn);

    auto value = parser.ParseValue(tkn);

    if ( tkn.kind != web::json::details::JSON_Parser<char>::Token::TKN_EOF )
    {
        throw web::json::json_exception(_XPLATSTR("Left-over characters in stream after parsing a JSON value."));
    }
    return value;
}
#endif

static web::json::value _parse_string(utility::string_t str)
{
    web::json::details::JSON_StringParser<utility::char_t> parser(str);

    web::json::details::JSON_Parser<utility::char_t>::Token tkn;
    parser.GetNextToken(tkn);

    auto value = parser.ParseValue(tkn);

    if ( tkn.kind != web::json::details::JSON_Parser<utility::char_t>::Token::TKN_EOF )
    {
        throw web::json::json_exception(_XPLATSTR("Left-over characters in stream after parsing a JSON value."));
    }
    return value;
}

web::json::value web::json::value::parse(utility::string_t value)
{
    return _parse_string(std::move(value));
}

web::json::value web::json::value::parse(utility::istream_t &stream)
{
    return _parse_stream(stream);
}

#ifdef _MS_WINDOWS
web::json::value web::json::value::parse(std::istream& stream)
{
    return _parse_narrow_stream(stream);
}
#endif
