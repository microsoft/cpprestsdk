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
* json_parsing.cpp
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

int _hexval [] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
                   -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

namespace web {
namespace json
{
namespace details
{

//
// JSON Parsing
//

template <typename Token>
#ifdef _MS_WINDOWS
    __declspec(noreturn)
#else
    __attribute__((noreturn))
#endif
void CreateError(const Token &tk, const utility::string_t &message)
{
    utility::ostringstream_t os;
    os << _XPLATSTR("* Line ") << tk.start.m_line << _XPLATSTR(", Column ") << tk.start.m_column << _XPLATSTR(" Syntax error: ") << message;
    throw web::json::json_exception(os.str().c_str());
}

    
template <typename CharType>
class JSON_Parser
{
public:
    JSON_Parser() 
        : m_currentLine(1),
          m_eof(std::char_traits<CharType>::eof()),
          m_currentColumn(1),
          m_currentParsingDepth(0)
    { }

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

        Kind kind;
        std::basic_string<CharType> spelling;

        typename JSON_Parser<CharType>::Location start;
        typename JSON_Parser<CharType>::Location end;

        union
        {
            double double_val;
            bool boolean_val;
            bool has_unescape_symbol;
        };
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

    virtual bool CompleteComment(Token &token);
    virtual bool CompleteStringLiteral(Token &token);
    bool handle_unescape_char(Token &token);

private:

    bool CompleteNumberLiteral(CharType first, Token &token);
    bool CompleteKeyword(const CharType *expected, typename Token::Kind kind, Token &token);
    bool CompleteKeywordTrue(Token &token);
    bool CompleteKeywordFalse(Token &token);
    bool CompleteKeywordNull(Token &token);
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

    void CreateToken(typename JSON_Parser<CharType>::Token& tk, typename Token::Kind kind)
    {
        tk.kind = kind;
        tk.start.m_line = m_currentLine; 
        tk.start.m_column = m_currentColumn; 
        tk.end = tk.start; 
        tk.spelling.clear();
    }

protected:

    size_t m_currentLine;
    size_t m_currentColumn;
    size_t m_currentParsingDepth;
#ifndef __APPLE__
    static const size_t maxParsingDepth = 128;
#else
    static const size_t maxParsingDepth = 32;
#endif
    typename std::char_traits<CharType>::int_type m_eof;
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
        : m_position(&string[0])
    {
        m_startpos = m_position;
        m_endpos = m_position+string.size();
    }

protected:

    virtual CharType NextCharacter();
    virtual CharType PeekCharacter();

    virtual bool CompleteComment(typename JSON_Parser<CharType>::Token &token);
    virtual bool CompleteStringLiteral(typename JSON_Parser<CharType>::Token &token);

private:
    bool finish_parsing_string_with_unescape_char(typename JSON_Parser<CharType>::Token &token);
    const CharType* m_position;
    const CharType* m_startpos;
    const CharType* m_endpos;
};


template <typename CharType>
CharType JSON_StreamParser<CharType>::NextCharacter()
{
    CharType ch = (CharType) m_streambuf->sbumpc();

    if (ch == '\n')
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
    if (m_position == m_endpos)
        return (CharType)this->m_eof;

    CharType ch = *m_position;
    m_position += 1;

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
bool JSON_Parser<CharType>::CompleteKeyword(const CharType *expected, typename Token::Kind kind, Token &token)
{
    token.spelling = expected[0];   // We need only the first char to tell literals apart (true, false and null)

    const CharType *ptr = expected+1;
    CharType ch = NextCharacter();

    while ( ch != this->m_eof )
    {
        if ( ch != *ptr )
        {
            return false;
        }
        ++ptr;

        if ( *ptr == '\0' )
        {
            token.kind = kind;
            token.end.m_column = m_currentColumn;
            token.end.m_line = m_currentLine;

            return true;
        }

        ch = NextCharacter();
    }
    return false;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteKeywordTrue(Token &token)
{
    if (NextCharacter() != 'r')
        return false;
    if (NextCharacter() != 'u')
        return false;
    if (NextCharacter() != 'e')
        return false;
    token.kind = Token::TKN_BooleanLiteral;
    token.end.m_column = m_currentColumn;
    token.end.m_line = m_currentLine;
    token.boolean_val = true;
    return true;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteKeywordFalse(Token &token)
{
    if (NextCharacter() != 'a')
        return false;
    if (NextCharacter() != 'l')
        return false;
    if (NextCharacter() != 's')
        return false;
    if (NextCharacter() != 'e')
        return false;
    token.kind = Token::TKN_BooleanLiteral;
    token.end.m_column = m_currentColumn;
    token.end.m_line = m_currentLine;
    token.boolean_val = false;
    return true;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteKeywordNull(Token &token)
{
    if (NextCharacter() != 'u')
        return false;
    if (NextCharacter() != 'l')
        return false;
    if (NextCharacter() != 'l')
        return false;
    token.kind = Token::TKN_NullLiteral;
    token.end.m_column = m_currentColumn;
    token.end.m_line = m_currentLine;
    return true;
}


template <typename CharType>
bool JSON_Parser<CharType>::CompleteNumberLiteral(CharType first, Token &token)
{
    bool minus;
    double value;
    bool decimal = false;
    bool could_be_integer = true;
    int after_decimal = 0;    // counts digits after the decimal

    // Exponent and related flags
    int exponent = 0;
    bool has_exponent = false;
    bool exponent_minus = false;

    if (first == CharType('-'))
    {
        minus = true;

        first = NextCharacter();
    }
    else
    {
        minus = false;
    }

    if (first < CharType('0') || first > CharType('9'))
        return false;

    value = first - CharType('0');

    CharType ch = PeekCharacter();

    //Check for two (or more) zeros at the begining
    if (first == CharType('0') && ch == CharType('0'))
        return false;

    while (ch != this->m_eof)
    {
        // Digit encountered?
        if (ch >= CharType('0') && ch <= CharType('9'))
        {
            value *= 10;
            value += ch - CharType('0');
            if (decimal)
                after_decimal++;

            NextCharacter();
            ch = PeekCharacter();
        }
        // Decimal dot?
        else if (ch == CharType('.'))
        {
            if (decimal)
                return false;

            decimal = true;
            could_be_integer = false;

            NextCharacter();
            ch = PeekCharacter();

            // Check that the following char is a digit
            if (ch < CharType('0') || ch > CharType('9'))
            return false;

            // Parse it
            value *= 10;
            value += ch - CharType('0');
            after_decimal = 1;

            NextCharacter();
            ch = PeekCharacter();
        }
        // Exponent?
        else if (ch == CharType('E') || ch == CharType('e'))
        {
            could_be_integer = false;

            NextCharacter();
            ch = PeekCharacter();

            // Check for the exponent sign
            if (ch == CharType('+'))
            {
                NextCharacter();
                ch = PeekCharacter();
            }
            else if (ch == CharType('-'))
            {
                exponent_minus = true;
                NextCharacter();
                ch = PeekCharacter();
            }

            // First number of the exponent
            if (ch >= CharType('0') && ch <= CharType('9'))
            {
                exponent = ch - CharType('0');

                NextCharacter();
                ch = PeekCharacter();
            }
            else return false;

            // The rest of the exponent
            while (ch >= CharType('0') && ch <= CharType('9'))
            {
                exponent *= 10;
                exponent += ch - CharType('0');

                NextCharacter();
                ch = PeekCharacter();
            }

            // The peeked character is not a number, so we can break from the loop and construct the number
            break;
        }
        else
        {
            // Not expected number character?
            break;
        }
    };

    if (minus)
        value = -value;

    if (has_exponent)
    {
        if (exponent_minus)
            exponent = -exponent;

        if (decimal)
            exponent -= after_decimal;

        token.double_val = value * pow(double(10), exponent);
    }
    else
    {
        token.double_val = value / pow(double(10), after_decimal);
    }

    token.kind = could_be_integer ? (JSON_Parser<CharType>::Token::TKN_IntegerLiteral) : (JSON_Parser<CharType>::Token::TKN_NumberLiteral);
    token.end.m_column = this->m_currentColumn;
    token.end.m_line = this->m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteComment(Token &token)
{
    // We already found a '/' character as the first of a token -- what kind of comment is it?

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
bool JSON_StringParser<CharType>::CompleteComment(typename JSON_Parser<CharType>::Token &token)
{
    // This function is specialized for the string parser, since we can be slightly more
    // efficient in copying data from the input to the token: do a memcpy() rather than
    // one character at a time.

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
inline bool JSON_Parser<CharType>::handle_unescape_char(Token &token)
{
    // This function converts unescape character pairs (e.g. "\t") into their ASCII or UNICODE representations (e.g. tab sign)
    // Also it handles \u + 4 hexadecimal digits
    CharType ch = NextCharacter();
    switch (ch)
    {
        case '\"':
            token.spelling.push_back('\"');
            return true;
        case '\\':
            token.spelling.push_back('\\');
            return true;
        case '/':
            token.spelling.push_back('/');
            return true;
        case 'b':
            token.spelling.push_back('\b');
            return true;
        case 'f':
            token.spelling.push_back('\f');
            return true;
        case 'r':
            token.spelling.push_back('\r');
            return true;
        case 'n':
            token.spelling.push_back('\n');
            return true;
        case 't':
            token.spelling.push_back('\t');
            return true;
        case 'u':
        {
            // A four-hexdigit unicode character
            int decoded = 0;
            for (int i = 0; i < 4; ++i)
            {
                ch = NextCharacter();
                if (!isxdigit((unsigned char) (ch)))
                    return false;

                int val = _hexval[ch];
                _ASSERTE(val != -1);

                // Add the input char to the decoded number
                decoded |= (val << (4 * (3 - i)));
            }

            // Construct the character based on the decoded number
            ch = static_cast<CharType>(decoded & 0xFFFF);
            token.spelling.push_back(ch);
            return true;
        }
        default:
            return false;
    }
}

template <typename CharType>
bool JSON_Parser<CharType>::CompleteStringLiteral(Token &token)
{
    CharType ch = NextCharacter();
    while ( ch != '"' )
    {
        if ( ch == '\\' )
        {
            handle_unescape_char(token);
        }
        else if (ch >= CharType(0x0) && ch < CharType(0x20))
        {
            return false;
        }
        else
        {
            if (ch == this->m_eof)
                return false;

            token.spelling.push_back(ch);
        }
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
bool JSON_StringParser<CharType>::CompleteStringLiteral(typename JSON_Parser<CharType>::Token &token)
{
    // This function is specialized for the string parser, since we can be slightly more
    // efficient in copying data from the input to the token: do a memcpy() rather than
    // one character at a time.

    auto start = m_position;
    token.has_unescape_symbol = false;

    CharType ch = JSON_StringParser<CharType>::NextCharacter();

    while (ch != '"')
    {
        if (ch == this->m_eof)
            return false;

        if (ch == '\\')
        {
            token.spelling.resize(m_position - start - 1);
            if (token.spelling.size() > 0)
                memcpy(&token.spelling[0], start, (m_position - start - 1)*sizeof(CharType));

            token.has_unescape_symbol = true;

            return finish_parsing_string_with_unescape_char(token);
        }
        else if (ch >= CharType(0x0) && ch < CharType(0x20))
        {
            return false;
        }

        ch = JSON_StringParser<CharType>::NextCharacter();
    }

    token.spelling.resize(m_position - start - 1);
    if (token.spelling.size() > 0)
        memcpy(&token.spelling[0], start, (m_position - start - 1)*sizeof(CharType));

    token.kind = JSON_Parser<CharType>::Token::TKN_StringLiteral;
    token.end.m_column = this->m_currentColumn;
    token.end.m_line = this->m_currentLine;

    return true;
}

template <typename CharType>
bool JSON_StringParser<CharType>::finish_parsing_string_with_unescape_char(typename JSON_Parser<CharType>::Token &token)
{
    // This function handles parsing the string when an unescape character is encountered.
    // It is called once the part before the unescape char is copied to the token.spelling string

    CharType ch;

    if (!JSON_StringParser<CharType>::handle_unescape_char(token))
        return false;

    while ((ch = JSON_StringParser<CharType>::NextCharacter()) != '"')
    {
        if (ch == '\\')
        {
            if (!JSON_StringParser<CharType>::handle_unescape_char(token))
                return false;
        }
        else
        {
            if (ch == this->m_eof)
                return false;

            token.spelling.push_back(ch);
        }
    }

    token.kind = JSON_StringParser<CharType>::Token::TKN_StringLiteral;
    token.end.m_column = this->m_currentColumn;
    token.end.m_line = this->m_currentLine;

    return true;
}

template <typename CharType>
void JSON_Parser<CharType>::GetNextToken(typename JSON_Parser<CharType>::Token& result)
{
try_again:
    CharType ch = EatWhitespace();

    CreateToken(result, Token::TKN_EOF);

    if (ch == this->m_eof) return;

    switch (ch)
    {
    case '{':
    case '[':
        {
            if(++m_currentParsingDepth > JSON_Parser<CharType>::maxParsingDepth)
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
        if (!CompleteKeywordTrue(result)) CreateError(result, _XPLATSTR("Malformed literal"));
        break;
    case 'f':
        if (!CompleteKeywordFalse(result)) CreateError(result, _XPLATSTR("Malformed literal"));
        break;
    case 'n':
        if (!CompleteKeywordNull(result)) CreateError(result, _XPLATSTR("Malformed literal"));
        break;
    case '/':
        if ( !CompleteComment(result) ) CreateError(result, _XPLATSTR("Malformed comment"));
        // For now, we're ignoring comments.
        goto try_again;
    case '"':
        if ( !CompleteStringLiteral(result) ) CreateError(result, _XPLATSTR("Malformed string literal"));
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
                GetNextToken(tkn);

                return result;
            default:
                CreateError(tkn, _XPLATSTR("Malformed array literal"));
            }
        }
    }

    GetNextToken(tkn);

    return result;
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
                auto value = utility::details::make_unique<web::json::details::_String>(std::move(tkn.spelling), tkn.has_unescape_symbol);
                GetNextToken(tkn);
                return std::move(value);
            }

        case JSON_Parser<CharType>::Token::TKN_IntegerLiteral:
            {
                auto value = utility::details::make_unique<web::json::details::_Number>(int32_t(tkn.double_val));
                GetNextToken(tkn);
                return std::move(value);
            }

        case JSON_Parser<CharType>::Token::TKN_NumberLiteral:  
            {
                auto value = utility::details::make_unique<web::json::details::_Number>(tkn.double_val);
                GetNextToken(tkn);
                return std::move(value);
            }
        case JSON_Parser<CharType>::Token::TKN_BooleanLiteral:
            {
                auto value = utility::details::make_unique<web::json::details::_Boolean>(tkn.boolean_val);
                GetNextToken(tkn);
                return std::move(value);
            }
        case JSON_Parser<CharType>::Token::TKN_NullLiteral:
            {
                GetNextToken(tkn);
                return std::move(utility::details::make_unique<web::json::details::_Null>());
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
        web::json::details::CreateError(tkn, _XPLATSTR("Left-over characters in stream after parsing a JSON value"));
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
        web::json::details::CreateError(tkn, _XPLATSTR("Left-over characters in stream after parsing a JSON value"));
    }
    return value;
}
#endif

web::json::value web::json::value::parse(const utility::string_t& str)
{
    web::json::details::JSON_StringParser<utility::char_t> parser(str);

    web::json::details::JSON_Parser<utility::char_t>::Token tkn;
    parser.GetNextToken(tkn);

    auto value = parser.ParseValue(tkn);

    if (tkn.kind != web::json::details::JSON_Parser<utility::char_t>::Token::TKN_EOF)
    {
        web::json::details::CreateError(tkn, _XPLATSTR("Left-over characters in stream after parsing a JSON value"));
    }
    return value;
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
