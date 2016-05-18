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
* Protocol independent support for URIs.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace utility::conversions;

namespace web { namespace details
{
utility::string_t uri_components::join()
{
    // canonicalize components first

    // convert scheme to lowercase
    std::transform(m_scheme.begin(), m_scheme.end(), m_scheme.begin(), [this](utility::char_t c) {
        return (utility::char_t)tolower(c);
    });

    // convert host to lowercase
    std::transform(m_host.begin(), m_host.end(), m_host.begin(), [this](utility::char_t c) {
        return (utility::char_t)tolower(c);
    });

    // canonicalize the path to have a leading slash if it's a full uri
    if (!m_host.empty() && m_path.empty())
    {
        m_path = _XPLATSTR("/");
    }
    else if (!m_host.empty() && m_path[0] != _XPLATSTR('/'))
    {
        m_path.insert(m_path.begin(), 1, _XPLATSTR('/'));
    }

    utility::string_t ret;

    if (!m_scheme.empty())
    {
        ret.append(m_scheme).append({ _XPLATSTR(':') });
    }

    if (!m_host.empty())
    {
        ret.append(_XPLATSTR("//"));

        if (!m_user_info.empty())
        {
            ret.append(m_user_info).append({ _XPLATSTR('@') });
        }

        ret.append(m_host);

        if (m_port > 0)
        {
            ret.append({ _XPLATSTR(':') }).append(utility::conversions::print_string(m_port, std::locale::classic()));
        }
    }

    if (!m_path.empty())
    {
        // only add the leading slash when the host is present
        if (!m_host.empty() && m_path.front() != _XPLATSTR('/'))
        {
            ret.append({ _XPLATSTR('/') });
        }

        ret.append(m_path);
    }

    if (!m_query.empty())
    {
        ret.append({ _XPLATSTR('?') }).append(m_query);
    }

    if (!m_fragment.empty())
    {
        ret.append({ _XPLATSTR('#') }).append(m_fragment);
    }

    return ret;
}
}

using namespace details;

uri::uri(const details::uri_components &components) : m_components(components)
{
    m_uri = m_components.join();
    if (!details::uri_parser::validate(m_uri))
    {
        throw uri_exception("provided uri is invalid: " + utility::conversions::to_utf8string(m_uri));
    }
}

uri::uri(const utility::string_t &uri_string)
{
    if (!details::uri_parser::parse(uri_string, m_components))
    {
        throw uri_exception("provided uri is invalid: " + utility::conversions::to_utf8string(uri_string));
    }
    m_uri = m_components.join();
}

uri::uri(const utility::char_t *uri_string): m_uri(uri_string)
{
    if (!details::uri_parser::parse(uri_string, m_components))
    {
        throw uri_exception("provided uri is invalid: " + utility::conversions::to_utf8string(uri_string));
    }
    m_uri = m_components.join();
}

utility::string_t uri::encode_impl(const utility::string_t &raw, const std::function<bool(int)>& should_encode)
{
    const utility::char_t * const hex = _XPLATSTR("0123456789ABCDEF");
    utility::string_t encoded;
    std::string utf8raw = to_utf8string(raw);
    for (auto iter = utf8raw.begin(); iter != utf8raw.end(); ++iter)
    {
        // for utf8 encoded string, char ASCII can be greater than 127.
        int ch = static_cast<unsigned char>(*iter);
        // ch should be same under both utf8 and utf16.
        if(should_encode(ch))
        {
            encoded.push_back(_XPLATSTR('%'));
            encoded.push_back(hex[(ch >> 4) & 0xF]);
            encoded.push_back(hex[ch & 0xF]);
        }
        else
        {
            // ASCII don't need to be encoded, which should be same on both utf8 and utf16.
            encoded.push_back((utility::char_t)ch);
        }
    }
    return encoded;
}

/// </summary>
/// Encodes a string by converting all characters except for RFC 3986 unreserved characters to their
/// hexadecimal representation.
/// </summary>
utility::string_t uri::encode_data_string(const utility::string_t &raw)
{
    return uri::encode_impl(raw, [](int ch) -> bool
    {
        return !uri_parser::is_unreserved(ch);
    });
}

utility::string_t uri::encode_uri(const utility::string_t &raw, uri::components::component component)
{
    // Note: we also encode the '+' character because some non-standard implementations
    // encode the space character as a '+' instead of %20. To better interoperate we encode
    // '+' to avoid any confusion and be mistaken as a space.
    switch(component)
    {
    case components::user_info:
        return uri::encode_impl(raw, [](int ch) -> bool
        {
            return !uri_parser::is_user_info_character(ch)
                || ch == '%' || ch == '+';
        });
    case components::host:
        return uri::encode_impl(raw, [](int ch) -> bool
        {
            // No encoding of ASCII characters in host name (RFC 3986 3.2.2)
            return ch > 127;
        });
    case components::path:
        return uri::encode_impl(raw, [](int ch) -> bool
        {
            return !uri_parser::is_path_character(ch)
                || ch == '%' || ch == '+';
        });
    case components::query:
        return uri::encode_impl(raw, [](int ch) -> bool
        {
            return !uri_parser::is_query_character(ch)
                || ch == '%' || ch == '+';
        });
    case components::fragment:
        return uri::encode_impl(raw, [](int ch) -> bool
        {
            return !uri_parser::is_fragment_character(ch)
                || ch == '%' || ch == '+';
        });
    case components::full_uri:
    default:
        return uri::encode_impl(raw, [](int ch) -> bool
        {
            return !uri_parser::is_unreserved(ch) && !uri_parser::is_reserved(ch);
        });
    };
}

/// <summary>
/// Helper function to convert a hex character digit to a decimal character value.
/// Throws an exception if not a valid hex digit.
/// </summary>
static int hex_char_digit_to_decimal_char(int hex)
{
    int decimal;
    if(hex >= '0' && hex <= '9')
    {
        decimal = hex - '0';
    }
    else if(hex >= 'A' && hex <= 'F')
    {
        decimal = 10 + (hex - 'A');
    }
    else if(hex >= 'a' && hex <= 'f')
    {
        decimal = 10 + (hex - 'a');
    }
    else
    {
        throw uri_exception("Invalid hexadecimal digit");
    }
    return decimal;
}

utility::string_t uri::decode(const utility::string_t &encoded)
{
    std::string utf8raw;
    for(auto iter = encoded.begin(); iter != encoded.end(); ++iter)
    {
        if(*iter == _XPLATSTR('%'))
        {
            if(++iter == encoded.end())
            {
                throw uri_exception("Invalid URI string, two hexadecimal digits must follow '%'");
            }
            int decimal_value = hex_char_digit_to_decimal_char(static_cast<int>(*iter)) << 4;
            if(++iter == encoded.end())
            {
                throw uri_exception("Invalid URI string, two hexadecimal digits must follow '%'");
            }
            decimal_value += hex_char_digit_to_decimal_char(static_cast<int>(*iter));

            utf8raw.push_back(static_cast<char>(decimal_value));
        }
        else
        {
            // encoded string has to be ASCII.
            utf8raw.push_back(reinterpret_cast<const char &>(*iter));
        }
    }
    return to_string_t(utf8raw);
}

std::vector<utility::string_t> uri::split_path(const utility::string_t &path)
{
    std::vector<utility::string_t> results;
    utility::istringstream_t iss(path);
    iss.imbue(std::locale::classic());
    utility::string_t s;

    while (std::getline(iss, s, _XPLATSTR('/')))
    {
        if (!s.empty())
        {
            results.push_back(s);
        }
    }

    return results;
}

std::map<utility::string_t, utility::string_t> uri::split_query(const utility::string_t &query)
{
    std::map<utility::string_t, utility::string_t> results;

    // Split into key value pairs separated by '&'.
    size_t prev_amp_index = 0;
    while(prev_amp_index != utility::string_t::npos)
    {
        size_t amp_index = query.find_first_of(_XPLATSTR('&'), prev_amp_index);
        if (amp_index == utility::string_t::npos)
            amp_index = query.find_first_of(_XPLATSTR(';'), prev_amp_index);

        utility::string_t key_value_pair = query.substr(
            prev_amp_index,
            amp_index == utility::string_t::npos ? query.size() - prev_amp_index : amp_index - prev_amp_index);
        prev_amp_index = amp_index == utility::string_t::npos ? utility::string_t::npos : amp_index + 1;

        size_t equals_index = key_value_pair.find_first_of(_XPLATSTR('='));
        if(equals_index == utility::string_t::npos)
        {
            continue;
        }
        else if (equals_index == 0)
        {
            utility::string_t value(key_value_pair.begin() + equals_index + 1, key_value_pair.end());
            results[_XPLATSTR("")] = value;
        }
        else
        {
            utility::string_t key(key_value_pair.begin(), key_value_pair.begin() + equals_index);
            utility::string_t value(key_value_pair.begin() + equals_index + 1, key_value_pair.end());
        results[key] = value;
    }
    }

    return results;
}

bool uri::validate(const utility::string_t &uri_string)
{
    return uri_parser::validate(uri_string);
}

uri uri::authority() const
{
    return uri_builder().set_scheme(this->scheme()).set_host(this->host()).set_port(this->port()).set_user_info(this->user_info()).to_uri();
}

uri uri::resource() const
{
    return uri_builder().set_path(this->path()).set_query(this->query()).set_fragment(this->fragment()).to_uri();
}

bool uri::operator == (const uri &other) const
{
    // Each individual URI component must be decoded before performing comparison.
    // TFS # 375865

    if (this->is_empty() && other.is_empty())
    {
        return true;
    }
    else if (this->is_empty() || other.is_empty())
    {
        return false;
    }
    else if (this->scheme() != other.scheme())
    {
        // scheme is canonicalized to lowercase
        return false;
    }
    else if(uri::decode(this->user_info()) != uri::decode(other.user_info()))
    {
        return false;
    }
    else if (uri::decode(this->host()) != uri::decode(other.host()))
    {
        // host is canonicalized to lowercase
        return false;
    }
    else if (this->port() != other.port())
    {
        return false;
    }
    else if (uri::decode(this->path()) != uri::decode(other.path()))
    {
        return false;
    }
    else if (uri::decode(this->query()) != uri::decode(other.query()))
    {
        return false;
    }
    else if (uri::decode(this->fragment()) != uri::decode(other.fragment()))
    {
        return false;
    }

    return true;
}

}
