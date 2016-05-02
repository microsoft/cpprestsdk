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
* HTTP Library: JSON parser
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <cstdlib>

#if defined(_MSC_VER)
#pragma warning(disable : 4127) // allow expressions like while(true) pass
#endif
using namespace web;
using namespace web::json;
using namespace web::json::details;
using namespace utility;
using namespace utility::conversions;

namespace web { namespace json { namespace details { namespace
{

std::array<signed char,128> _hexval = {{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                          0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
                                         -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                         -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }};

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

#if defined(_MSC_VER)
    __declspec(noreturn)
#else
    __attribute__((noreturn))
#endif
    void throw_exception() const
    {
        std::ostringstream os;
        os.imbue(std::locale());
        os << "* Line " << start.m_line << ", Column " << start.m_column << " Syntax error: " << json::details::json_error_category().message(m_error);
        throw web::json::json_exception(os.str());
    }

    Kind kind = TKN_EOF;
    std::string string_val;

    Location start;

    union
    {
        double double_val;
        int64_t int64_val;
        uint64_t uint64_val;
        bool boolean_val;
        bool has_unescape_symbol;
    };

    bool signed_number;

    json::details::json_error m_error = json::details::json_error::no_error;
};


//
// JSON Parsing
//

class JSON_Parser
{
public:
    JSON_Parser()
        : m_currentLine(1),
          m_currentColumn(1),
          m_currentParsingDepth(0)
    { }

    JSON_Parser(const JSON_Parser&) = delete;
    JSON_Parser& operator=(const JSON_Parser&) = delete;

    void GetNextToken(Token &);

    web::json::value ParseValue(Token &first)
    {
#ifndef _WIN32
        utility::details::scoped_c_thread_locale locale;
#endif

        return ParseValue_inner(first);
    }

protected:
    using int_type = std::char_traits<char>::int_type;
    virtual int_type NextCharacter() = 0;
    virtual int_type PeekCharacter() = 0;

    virtual bool CompleteComment(Token &token);
    virtual bool CompleteStringLiteral(Token &token);
    bool handle_unescape_char(Token &token);

private:

    bool CompleteNumberLiteral(char first, Token &token);
    bool ParseInt64(char first, uint64_t& value);
    bool CompleteKeywordTrue(Token &token);
    bool CompleteKeywordFalse(Token &token);
    bool CompleteKeywordNull(Token &token);
    value ParseValue_inner(Token &first);
    value ParseObject(Token &tkn);
    value ParseArray(Token &tkn);

    int_type EatWhitespace();

    void CreateToken(Token& tk, Token::Kind kind, Location &start)
    {
        tk.kind = kind;
        tk.start = start;
        tk.string_val.clear();
    }

    void CreateToken(Token& tk, Token::Kind kind)
    {
        tk.kind = kind;
        tk.start.m_line = m_currentLine;
        tk.start.m_column = m_currentColumn;
        tk.string_val.clear();
    }

protected:

    size_t m_currentLine;
    size_t m_currentColumn;
    size_t m_currentParsingDepth;

// The DEBUG macro is defined in XCode but we don't in our CMakeList
// so for now we will keep the same on debug and release. In the future
// this can be increase on release if necessary.
#if defined(__APPLE__)
    static const size_t maxParsingDepth = 32;
#else
    static const size_t maxParsingDepth = 128;
#endif
};

std::char_traits<char>::int_type eof()
{
    return std::char_traits<char>::eof();
}

template <typename CharType>
class JSON_StreamParser;

template <>
class JSON_StreamParser<char> final : public JSON_Parser
    {
public:
    JSON_StreamParser(std::istream &stream)
        : m_streambuf(stream.rdbuf())
    {
    }

protected:

    virtual JSON_Parser::int_type NextCharacter() override
    {
        auto ch = m_streambuf->sbumpc();

        if (ch == '\n')
        {
            this->m_currentLine += 1;
            this->m_currentColumn = 0;
        }
        else
        {
            this->m_currentColumn += 1;
        }

        return ch;
    }

    virtual JSON_Parser::int_type PeekCharacter() override
    {
        return m_streambuf->sgetc();
    }

private:
    std::streambuf* m_streambuf;
};

#if !defined(_LIBCPP_VERSION)
template <>
class JSON_StreamParser<utf16char> final : public JSON_Parser
{
public:
    JSON_StreamParser(std::basic_istream<utf16char> &stream)
        : m_streambuf(stream.rdbuf())
    {
    }

protected:

    virtual JSON_Parser::int_type NextCharacter() override
    {
        ensure_utf8_buffer();
        if (m_eof)
            return std::char_traits<char>::eof();
        auto ch = m_utf8_buffer.back();
        m_utf8_buffer.pop_back();

        if (ch == '\n')
        {
            this->m_currentLine += 1;
            this->m_currentColumn = 0;
        }
        else
        {
            this->m_currentColumn += 1;
        }

        return ch;
    }

    virtual JSON_Parser::int_type PeekCharacter() override
    {
        ensure_utf8_buffer();
        if (m_eof)
            return std::char_traits<char>::eof();
        return m_utf8_buffer.back();
    }

private:
    void ensure_utf8_buffer()
    {
        if (m_utf8_buffer.size() > 0)
            return;
        auto ch = m_streambuf->sbumpc();
        if (ch == std::char_traits<utf16char>::eof())
        {
            m_eof = true;
            return;
        }
        // Buffer for conversion.
        utf16string u16_str;
        u16_str.push_back(static_cast<utf16char>(ch));

        if (static_cast<uint16_t>(ch) >= 0xD800)
        {
            // This is a high surrogate. Get the low surrogate and convert.
            auto low_ch = m_streambuf->sbumpc();
            if (low_ch == std::char_traits<utf16char>::eof())
            {
                // Low surrogate is not present. Replace dangling surrogate with "REPLACEMENT CHARACTER"
                m_utf8_buffer = "\xEF\xBF\xBD";
                return;
            }
            u16_str.push_back(static_cast<utf16char>(low_ch));
        }
        try
        {
            m_utf8_buffer = utility::conversions::to_utf8string(u16_str);
        }
        catch (...)
        {
            // Conversion failed. Replace the character with the Unicode "REPLACEMENT CHARACTER" https://en.wikipedia.org/wiki/Specials_Unicode_block
            m_utf8_buffer = "\xEF\xBF\xBD";
        }
        std::reverse(m_utf8_buffer.begin(), m_utf8_buffer.end());

        assert(m_utf8_buffer.size() > 0 || m_eof);
    }

    std::basic_streambuf<utf16char, std::char_traits<utf16char>>* m_streambuf;
    bool m_eof = false;

    // The string of utf8 code units are buffered in reverse order
    std::string m_utf8_buffer;
};
#endif

class JSON_StringParser final : public JSON_Parser
{
public:
    JSON_StringParser(const std::string& string)
        : m_position(&string[0])
        , m_startpos(m_position)
        , m_endpos(m_position + string.size())
    {}

protected:

    virtual JSON_Parser::int_type NextCharacter() override;
    virtual JSON_Parser::int_type PeekCharacter() override;

    virtual bool CompleteComment(Token &token) override;
    virtual bool CompleteStringLiteral(Token &token) override;

private:
    const char* m_position;
    const char* const m_startpos;
    const char* const m_endpos;
};

JSON_Parser::int_type JSON_StringParser::NextCharacter()
{
    if (m_position == m_endpos)
        return eof();

    char ch = *m_position;
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

    return ch;
}

JSON_Parser::int_type JSON_StringParser::PeekCharacter()
{
    if ( m_position == m_endpos ) return eof();

    return *m_position;
}

//
// Consume whitespace characters and return the first non-space character or EOF
//
JSON_Parser::int_type JSON_Parser::EatWhitespace()
{
   auto ch = NextCharacter();

   while ( ch != eof() && isspace(static_cast<uint8_t>(ch)))
   {
       ch = NextCharacter();
   }

   return ch;
}

bool JSON_Parser::CompleteKeywordTrue(Token &token)
{
    if (NextCharacter() != 'r')
        return false;
    if (NextCharacter() != 'u')
        return false;
    if (NextCharacter() != 'e')
        return false;
    token.kind = Token::TKN_BooleanLiteral;
    token.boolean_val = true;
    return true;
}

bool JSON_Parser::CompleteKeywordFalse(Token &token)
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
    token.boolean_val = false;
    return true;
}

bool JSON_Parser::CompleteKeywordNull(Token &token)
{
    if (NextCharacter() != 'u')
        return false;
    if (NextCharacter() != 'l')
        return false;
    if (NextCharacter() != 'l')
        return false;
    token.kind = Token::TKN_NullLiteral;
    return true;
}

// Returns false only on overflow
inline bool JSON_Parser::ParseInt64(char first, uint64_t& value)
{
    value = first - '0';
    auto ch = PeekCharacter();
    while (ch >= '0' && ch <= '9')
    {
        unsigned int next_digit = (unsigned int)(ch - '0');
        if (value > (ULLONG_MAX / 10) || (value == ULLONG_MAX/10 && next_digit > ULLONG_MAX%10))
            return false;

        NextCharacter();

        value *= 10;
        value += next_digit;
        ch = PeekCharacter();
    }
    return true;
}

#if defined(_WIN32)
    int print_llu(char* ptr, size_t n, uint64_t val64)
    {
        return _snprintf_s_l(ptr, n, _TRUNCATE, "%I64u", utility::details::scoped_c_thread_locale::c_locale(), val64);
    }
    double anystod(const char* str)
    {
        return _strtod_l(str, nullptr, utility::details::scoped_c_thread_locale::c_locale());
    }
#else
    int __attribute__((__unused__)) print_llu(char* ptr, size_t n, unsigned long long val64)
    {
        return snprintf(ptr, n, "%llu", val64);
    }
    int __attribute__((__unused__)) print_llu(char* ptr, size_t n, unsigned long val64)
    {
        return snprintf(ptr, n, "%lu", val64);
    }
    double __attribute__((__unused__)) anystod(const char* str)
    {
        return strtod(str, nullptr);
    }
#endif

bool JSON_Parser::CompleteNumberLiteral(char first, Token &token)
{
    bool minus_sign;

    if (first == '-')
    {
        minus_sign = true;

        // Safe to cast because the check after this if/else statement will cover EOF.
        first = static_cast<char>(NextCharacter());
    }
    else
    {
        minus_sign = false;
    }

    if (first < '0' || first > '9')
        return false;

    auto ch = PeekCharacter();

    //Check for two (or more) zeros at the beginning
    if (first == '0' && ch == '0')
        return false;

    // Parse the number assuming its integer
    uint64_t val64;
    bool complete = ParseInt64(first, val64);

    ch = PeekCharacter();
    if (complete && ch!='.' && ch!='E' && ch!='e')
    {
        if (minus_sign)
        {
            if (val64 > static_cast<uint64_t>(1) << 63 )
            {
                // It is negative and cannot be represented in int64, so we resort to double
                token.double_val = 0 - static_cast<double>(val64);
                token.signed_number = true;
                token.kind = Token::TKN_NumberLiteral;
                return true;
            }

            // It is negative, but fits into int64
            token.int64_val = 0 - static_cast<int64_t>(val64);
            token.kind = Token::TKN_IntegerLiteral;
            token.signed_number = true;
            return true;
        }

        // It is positive so we use unsigned int64
        token.uint64_val = val64;
        token.kind = Token::TKN_IntegerLiteral;
        token.signed_number = false;
        return true;
    }

    // Magic number 5 leaves room for decimal point, null terminator, etc (in most cases)
    ::std::vector<char> buf(::std::numeric_limits<uint64_t>::digits10 + 5);
    int count = print_llu(buf.data(), buf.size(), val64);
    _ASSERTE(count >= 0);
    _ASSERTE((size_t)count < buf.size());
    // Resize to cut off the null terminator
    buf.resize(count);

    bool decimal = false;

    while (ch != eof())
    {
        // Digit encountered?
        if (ch >= '0' && ch <= '9')
        {
            buf.push_back(static_cast<char>(ch));
            NextCharacter();
            ch = PeekCharacter();
        }

        // Decimal dot?
        else if (ch == '.')
        {
            if (decimal)
                return false;

            decimal = true;
            buf.push_back(static_cast<char>(ch));

            NextCharacter();
            ch = PeekCharacter();

            // Check that the following char is a digit
            if (ch < '0' || ch > '9')
            return false;

            buf.push_back(static_cast<char>(ch));
            NextCharacter();
            ch = PeekCharacter();
        }

        // Exponent?
        else if (ch == 'E' || ch == 'e')
        {
            buf.push_back(static_cast<char>(ch));
            NextCharacter();
            ch = PeekCharacter();

            // Check for the exponent sign
            if (ch == '+')
            {
                buf.push_back(static_cast<char>(ch));
                NextCharacter();
                ch = PeekCharacter();
            }
            else if (ch == '-')
            {
                buf.push_back(static_cast<char>(ch));
                NextCharacter();
                ch = PeekCharacter();
            }

            // First number of the exponent
            if (ch >= '0' && ch <= '9')
            {
                buf.push_back(static_cast<char>(ch));
                NextCharacter();
                ch = PeekCharacter();
            }
            else return false;

            // The rest of the exponent
            while (ch >= '0' && ch <= '9')
            {
                buf.push_back(static_cast<char>(ch));
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

    buf.push_back('\0');
    token.double_val = anystod(buf.data());
    if (minus_sign)
    {
        token.double_val = -token.double_val;
    }
    token.kind = (Token::TKN_NumberLiteral);

    return true;
}

bool JSON_Parser::CompleteComment(Token &token)
{
    // We already found a '/' character as the first of a token -- what kind of comment is it?

    auto ch = NextCharacter();

    if ( ch == eof() || (ch != '/' && ch != '*') )
        return false;

    if ( ch == '/' )
    {
        // Line comment -- look for a newline or EOF to terminate.

        ch = NextCharacter();

        while ( ch != eof() && ch != '\n')
        {
            ch = NextCharacter();
        }
    }
    else
    {
        // Block comment -- look for a terminating "*/" sequence.

        ch = NextCharacter();

        while ( true )
        {
            if ( ch == eof())
                return false;

            if ( ch == '*' )
            {
                auto ch1 = PeekCharacter();

                if ( ch1 == eof())
                    return false;

                if ( ch1 == '/' )
                {
                    // Consume the character
                    NextCharacter();
                    break;
                }

                ch = ch1;
            }

            ch = NextCharacter();
        }
    }

    token.kind = Token::TKN_Comment;

    return true;
}

bool JSON_StringParser::CompleteComment(Token &token)
{
    // This function is specialized for the string parser, since we can be slightly more
    // efficient in copying data from the input to the token: do a memcpy() rather than
    // one character at a time.

    auto ch = JSON_StringParser::NextCharacter();

    if ( ch == eof() || (ch != '/' && ch != '*') )
        return false;

    if ( ch == '/' )
    {
        // Line comment -- look for a newline or EOF to terminate.

        ch = JSON_StringParser::NextCharacter();

        while ( ch != eof() && ch != '\n')
        {
            ch = JSON_StringParser::NextCharacter();
        }
    }
    else
    {
        // Block comment -- look for a terminating "*/" sequence.

        ch = JSON_StringParser::NextCharacter();

        while ( true )
        {
            if ( ch == eof())
                return false;

            if ( ch == '*' )
            {
                ch = JSON_StringParser::PeekCharacter();

                if ( ch == eof())
                    return false;

                if ( ch == '/' )
                {
                    // Consume the character
                    JSON_StringParser::NextCharacter();
                    break;
                }

            }

            ch = JSON_StringParser::NextCharacter();
        }
    }

    token.kind = Token::TKN_Comment;

    return true;
}

inline bool JSON_Parser::handle_unescape_char(Token &token)
{
    token.has_unescape_symbol = true;

    // This function converts unescaped character pairs (e.g. "\t") into their ASCII or Unicode representations (e.g. tab sign)
    // Also it handles \u + 4 hexadecimal digits
    auto ch = NextCharacter();
    switch (ch)
    {
        case '\"':
            token.string_val.push_back('\"');
            return true;
        case '\\':
            token.string_val.push_back('\\');
            return true;
        case '/':
            token.string_val.push_back('/');
            return true;
        case 'b':
            token.string_val.push_back('\b');
            return true;
        case 'f':
            token.string_val.push_back('\f');
            return true;
        case 'r':
            token.string_val.push_back('\r');
            return true;
        case 'n':
            token.string_val.push_back('\n');
            return true;
        case 't':
            token.string_val.push_back('\t');
            return true;
        case 'u':
        {
            auto decode_utf16_unit = [](JSON_Parser& parser, json_error& ec) {
                // A four-hexdigit Unicode character.
                // Transform into a 16 bit code point.
                int decoded = 0;
                for (int i = 0; i < 4; ++i)
                {
                    int ch_int = parser.NextCharacter();
                    if (ch_int < 0 || ch_int > 127) {
                        ec = json_error::malformed_string_literal;
                        return 0;
                    }
#ifdef _WIN32
                    const int isxdigitResult = _isxdigit_l(ch_int, utility::details::scoped_c_thread_locale::c_locale());
#else
                    const int isxdigitResult = isxdigit(ch_int);
#endif
                    if (!isxdigitResult)
                    {
                        ec = json_error::malformed_string_literal;
                        return 0;
                    }

                    int val = _hexval[static_cast<size_t>(ch_int)];
                    _ASSERTE(val != -1);

                    // Add the input char to the decoded number
                    decoded |= (val << (4 * (3 - i)));
                }

                return decoded;
            };

            // Construct the character based on the decoded number
            // Convert the code unit into a UTF-8 sequence
            utf16string utf16;
            auto decoded = decode_utf16_unit(*this, token.m_error);
            if (token.m_error)
                return false;
            utf16.push_back(static_cast<utf16char>(decoded));

            if (decoded >= 0xD800)
            {
                // Decoded a high surrogate. Attempt to grab low surrogate.
                if (NextCharacter() != '\\')
                {
                    token.m_error = json_error::malformed_string_literal;
                    return false;
                }
                if (NextCharacter() != 'u')
                {
                    token.m_error = json_error::malformed_string_literal;
                    return false;
                }
                decoded = decode_utf16_unit(*this, token.m_error);
                if (token.m_error)
                    return false;
                utf16.push_back(static_cast<utf16char>(decoded));
            }

            try
            {
                utf8string utf8;
                utf8 = ::utility::conversions::utf16_to_utf8(utf16);
                token.string_val.append(utf8);
                return true;
            }
            catch (...)
            {
                token.m_error = json_error::malformed_string_literal;
                return false;
            }
        }
        default:
            // BUG: This is incorrect behavior; all characters MAY be escaped, and should be added as-is.
            return false;
    }
}

bool JSON_Parser::CompleteStringLiteral(Token &token)
{
    token.has_unescape_symbol = false;
    auto ch = NextCharacter();
    while ( ch != '"' )
    {
        if ( ch == '\\' )
        {
            handle_unescape_char(token);
        }
        else if (ch >= char(0x0) && ch < char(0x20))
        {
            return false;
        }
        else
        {
            if (ch == eof())
                return false;

            token.string_val.push_back(static_cast<char>(ch));
        }
        ch = NextCharacter();
    }

    if ( ch == '"' )
    {
        token.kind = Token::TKN_StringLiteral;
    }
    else
    {
        return false;
    }

    return true;
}


bool JSON_StringParser::CompleteStringLiteral(Token &token)
{
    // This function is specialized for the string parser, since we can be slightly more
    // efficient in copying data from the input to the token: do a memcpy() rather than
    // one character at a time.

    auto start = m_position;
    token.has_unescape_symbol = false;

    auto ch = JSON_StringParser::NextCharacter();

    while (ch != '"')
    {
        if (ch == eof())
            return false;

        if (ch == '\\')
        {
            const size_t numChars = m_position - start - 1;
            const size_t prevSize = token.string_val.size();
            token.string_val.resize(prevSize + numChars);
            memcpy(const_cast<char *>(token.string_val.c_str() + prevSize), start, numChars);

            if (!JSON_StringParser::handle_unescape_char(token))
            {
                return false;
            }

            // Reset start position and continue.
            start = m_position;
        }
        else if (ch >= char(0x0) && ch < char(0x20))
        {
            return false;
        }

        ch = JSON_StringParser::NextCharacter();
    }

    const size_t numChars = m_position - start - 1;
    const size_t prevSize = token.string_val.size();
    token.string_val.resize(prevSize + numChars);
    memcpy(const_cast<char *>(token.string_val.c_str() + prevSize), start, numChars);

    token.kind = Token::TKN_StringLiteral;

    return true;
}


void JSON_Parser::GetNextToken(Token& result)
{
try_again:
    auto ch = EatWhitespace();

    CreateToken(result, Token::TKN_EOF);

    if (ch == eof()) return;

    switch (ch)
    {
    case '{':
    case '[':
        {
            if(++m_currentParsingDepth > JSON_Parser::maxParsingDepth)
            {
                result.m_error = json_error::nesting;
                break;
            }

            Token::Kind tk = ch == '{' ? Token::TKN_OpenBrace : Token::TKN_OpenBracket;
            CreateToken(result, tk, result.start);
            break;
        }
    case '}':
    case ']':
        {
            if((signed int)(--m_currentParsingDepth) < 0)
            {
                result.m_error = json_error::mismatched_brances;
                break;
            }

            Token::Kind tk = ch == '}' ? Token::TKN_CloseBrace : Token::TKN_CloseBracket;
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
        if (!CompleteKeywordTrue(result))
        {
            result.m_error = json_error::malformed_literal;
        }
        break;
    case 'f':
        if (!CompleteKeywordFalse(result))
        {
            result.m_error = json_error::malformed_literal;
        }
        break;
    case 'n':
        if (!CompleteKeywordNull(result))
        {
            result.m_error = json_error::malformed_literal;
        }
        break;
    case '/':
        if (!CompleteComment(result))
        {
            result.m_error = json_error::malformed_comment;
            break;
        }
        // For now, we're ignoring comments.
        goto try_again;
    case '"':
        if (!CompleteStringLiteral(result))
        {
            result.m_error = json_error::malformed_string_literal;
        }
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
        if (!CompleteNumberLiteral(static_cast<char>(ch), result))
        {
            result.m_error = json_error::malformed_numeric_literal;
        }
        break;
    default:
        result.m_error = json_error::malformed_token;
        break;
    }
}


value JSON_Parser::ParseObject(Token &tkn)
{
    auto obj = utility::details::make_unique<web::json::details::_Object>(object::storage_type{}, g_keep_json_object_unsorted);
    auto& elems = obj->m_object.m_elements;

    GetNextToken(tkn);
    if (tkn.m_error) goto error;

    if (tkn.kind != Token::TKN_CloseBrace)
    {
        while (true)
        {
            // State 1: New field or end of object, looking for field name or closing brace
            std::string fieldName;
            switch (tkn.kind)
            {
            case Token::TKN_StringLiteral:
                fieldName = std::move(tkn.string_val);
                break;
            default:
                goto error;
            }

            GetNextToken(tkn);
            if (tkn.m_error) goto error;

            // State 2: Looking for a colon.
            if (tkn.kind != Token::TKN_Colon) goto done;

            GetNextToken(tkn);
            if (tkn.m_error) goto error;

            // State 3: Looking for an expression.
            elems.emplace_back(std::move(fieldName), ParseValue_inner(tkn));
            if (tkn.m_error) goto error;

            // State 4: Looking for a comma or a closing brace
            switch (tkn.kind)
            {
            case Token::TKN_Comma:
                GetNextToken(tkn);
                if (tkn.m_error) goto error;
                break;
            case Token::TKN_CloseBrace:
                goto done;
            default:
                goto error;
            }
        }
    }

done:
    GetNextToken(tkn);
    if (tkn.m_error) return value();

    if (!g_keep_json_object_unsorted) {
        ::std::sort(elems.begin(), elems.end(), json::object::compare_pairs);
    }

    return value(std::move(obj));

error:
    if (!tkn.m_error)
    {
        tkn.m_error = json_error::malformed_object_literal;
    }
    return value();
}


value JSON_Parser::ParseArray(Token &tkn)
{
    GetNextToken(tkn);
    if (tkn.m_error) return value();

    auto result = utility::details::make_unique<web::json::details::_Array>();

    if (tkn.kind != Token::TKN_CloseBracket)
    {
        while (true)
        {
            // State 1: Looking for an expression.
            result->m_array.m_elements.emplace_back(ParseValue_inner(tkn));
            if (tkn.m_error) return value();

            // State 4: Looking for a comma or a closing bracket
            switch (tkn.kind)
            {
            case Token::TKN_Comma:
                GetNextToken(tkn);
                if (tkn.m_error) return value();
                break;
            case Token::TKN_CloseBracket:
                GetNextToken(tkn);
                if (tkn.m_error) return value();
                return value(std::move(result));
            default:
                tkn.m_error = json_error::malformed_array_literal;
                return value();
            }
        }
    }

    GetNextToken(tkn);
    if (tkn.m_error) return value();

    return value(std::move(result));
}


value JSON_Parser::ParseValue_inner(Token &tkn)
{
    switch (tkn.kind)
    {
        case Token::TKN_OpenBrace:
            {
                return ParseObject(tkn);
            }
        case Token::TKN_OpenBracket:
            {
                return ParseArray(tkn);
            }
        case Token::TKN_StringLiteral:
            {
                auto strvalue = utility::details::make_unique<web::json::details::_String>(std::move(tkn.string_val), tkn.has_unescape_symbol);
                GetNextToken(tkn);
                if (tkn.m_error) return value();
                return value(std::move(strvalue));
            }
        case Token::TKN_IntegerLiteral:
            {
                std::unique_ptr<web::json::details::_Number> numvalue;
                if (tkn.signed_number)
                    numvalue = utility::details::make_unique<web::json::details::_Number>(tkn.int64_val);
                else
                    numvalue = utility::details::make_unique<web::json::details::_Number>(tkn.uint64_val);

                GetNextToken(tkn);
                if (tkn.m_error) return value();
                return value(std::move(numvalue));
            }
        case Token::TKN_NumberLiteral:
            {
                auto numvalue = utility::details::make_unique<web::json::details::_Number>(tkn.double_val);
                GetNextToken(tkn);
                if (tkn.m_error) return value();
                return value(std::move(numvalue));
            }
        case Token::TKN_BooleanLiteral:
            {
                auto boolvalue = utility::details::make_unique<web::json::details::_Boolean>(tkn.boolean_val);
                GetNextToken(tkn);
                if (tkn.m_error) return value();
                return value(std::move(boolvalue));
            }
        case Token::TKN_NullLiteral:
            {
                GetNextToken(tkn);
                // Returning a null value whether or not an error occurred.
                return value();
            }
        default:
            {
                tkn.m_error = json_error::malformed_token;
                return value();
            }
    }
}

web::json::value inner_parse(JSON_Parser& parser, web::json::details::Token& tkn)
{
    parser.GetNextToken(tkn);
    if (tkn.m_error)
    {
        return web::json::value();
    }

    auto ret = parser.ParseValue(tkn);
    if (tkn.kind != web::json::details::Token::TKN_EOF)
    {
        ret = web::json::value();
        tkn.m_error = web::json::details::json_error::left_over_character_in_stream;
    }
    return ret;
}

value parse_stream(JSON_Parser&& parser, std::error_code& error)
{
    Token tkn;

    auto ret = inner_parse(parser, tkn);

    if (tkn.m_error)
    {
        error = std::error_code(tkn.m_error, json_error_category());
        return value();
    }
    return ret;
}

value parse_stream(JSON_Parser&& parser)
{
    Token tkn;

    auto ret = inner_parse(parser, tkn);

    if (tkn.m_error)
    {
        tkn.throw_exception();
    }
    return ret;
}

}} // details::`anonymous namespace`

value value::parse(std::istream& stream)
{
    return parse_stream(JSON_StreamParser<char>(stream));
}

value value::parse(std::istream& stream, std::error_code& error)
{
    return parse_stream(JSON_StreamParser<char>(stream), error);
}

value value::parse(const std::string& stream)
{
    return parse_stream(JSON_StringParser(stream));
}

value value::parse(const std::string& stream, std::error_code& error)
{
    return parse_stream(JSON_StringParser(stream), error);
}

#if !defined(_LIBCPP_VERSION)
value value::parse(utf16istream& stream)
{
    return parse_stream(JSON_StreamParser<utf16char>(stream));
}

value value::parse(utf16istream& stream, std::error_code& error)
{
    return parse_stream(JSON_StreamParser<utf16char>(stream), error);
}
#endif

}} // web::json
