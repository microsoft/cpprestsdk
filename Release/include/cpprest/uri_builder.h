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
* Builder style class for creating URIs.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "cpprest/base_uri.h"
#include "cpprest/details/uri_parser.h"

namespace web
{
    /// <summary>
    /// Builder for constructing URIs incrementally.
    /// </summary>
    class uri_builder
    {
    public:

        /// <summary>
        /// Creates a builder with an initially empty URI.
        /// </summary>
        uri_builder() {}

        /// <summary>
        /// Creates a builder with a existing URI object.
        /// </summary>
        /// <param name="uri_str">Encoded string containing the URI.</param>
        uri_builder(const uri &uri_str): m_uri(uri_str.m_components) {}

        /// <summary>
        /// Get the scheme component of the URI as an encoded string.
        /// </summary>
        /// <returns>The URI scheme as a string.</returns>
        const utility::string_t &scheme() const { return m_uri.m_scheme; }

        /// <summary>
        /// Get the user information component of the URI as an encoded string.
        /// </summary>
        /// <returns>The URI user information as a string.</returns>
        const utility::string_t &user_info() const { return m_uri.m_user_info; }

        /// <summary>
        /// Get the host component of the URI as an encoded string.
        /// </summary>
        /// <returns>The URI host as a string.</returns>
        const utility::string_t &host() const { return m_uri.m_host; }

        /// <summary>
        /// Get the port component of the URI. Returns -1 if no port is specified.
        /// </summary>
        /// <returns>The URI port as an integer.</returns>
        int port() const { return m_uri.m_port; }

        /// <summary>
        /// Get the path component of the URI as an encoded string.
        /// </summary>
        /// <returns>The URI path as a string.</returns>
        const utility::string_t &path() const { return m_uri.m_path; }

        /// <summary>
        /// Get the query component of the URI as an encoded string.
        /// </summary>
        /// <returns>The URI query as a string.</returns>
        const utility::string_t &query() const { return m_uri.m_query; }

        /// <summary>
        /// Get the fragment component of the URI as an encoded string.
        /// </summary>
        /// <returns>The URI fragment as a string.</returns>
        const utility::string_t &fragment() const { return m_uri.m_fragment; }

        /// <summary>
        /// Set the scheme of the URI.
        /// </summary>
        /// <param name="scheme">Uri scheme.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_scheme(const utility::string_t &scheme)
        {
            m_uri.m_scheme = scheme;
            return *this;
        }

        /// <summary>
        /// Set the user info component of the URI.
        /// </summary>
        /// <param name="user_info">User info as a decoded string.</param>
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_user_info(const utility::string_t &user_info, bool do_encoding = false)
        {
            m_uri.m_user_info = do_encoding ? uri::encode_uri(user_info, uri::components::user_info) : user_info;
            return *this;
        }

        /// <summary>
        /// Set the host component of the URI.
        /// </summary>
        /// <param name="host">Host as a decoded string.</param>
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_host(const utility::string_t &host, bool do_encoding = false)
        {
            m_uri.m_host = do_encoding ? uri::encode_uri(host, uri::components::host) : host;
            return *this;
        }

        /// <summary>
        /// Set the port component of the URI.
        /// </summary>
        /// <param name="port">Port as an integer.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_port(int port)
        {
            m_uri.m_port = port;
            return *this;
        }

        /// <summary>
        /// Set the port component of the URI.
        /// </summary>
        /// <param name="port">Port as a string.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        /// <remarks>When string can't be converted to an integer the port is left unchanged.</remarks>
        uri_builder & set_port(const utility::string_t &port)
        {
            utility::istringstream_t portStream(port);
            int port_tmp;
            portStream >> port_tmp;
            if(portStream.fail() || portStream.bad())
            {
                throw std::invalid_argument("invalid port argument, must be non empty string containing integer value");
            }
            m_uri.m_port = port_tmp;
            return *this;
        }

        /// <summary>
        /// Set the path component of the URI.
        /// </summary>
        /// <param name="path">Path as a decoded string.</param>
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_path(const utility::string_t &path, bool do_encoding = false)
        {
            m_uri.m_path = do_encoding ? uri::encode_uri(path, uri::components::path) : path;
            return *this;
        }


        /// <summary>
        /// Set the query component of the URI.
        /// </summary>
        /// <param name="query">Query as a decoded string.</param>
        /// <param name="do_encoding">Specify whether apply URI encoding to the given string.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_query(const utility::string_t &query, bool do_encoding = false)
        {
            m_uri.m_query = do_encoding ? uri::encode_uri(query, uri::components::query) : query;
            return *this;
        }

        /// <summary>
        /// Set the fragment component of the URI.
        /// </summary>
        /// <param name="fragment">Fragment as a decoded string.</param>
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.</param>
        /// <returns>A reference to this <c>uri_builder</c> to support chaining.</returns>
        uri_builder & set_fragment(const utility::string_t &fragment, bool do_encoding = false)
        {
            m_uri.m_fragment = do_encoding ? uri::encode_uri(fragment, uri::components::fragment) : fragment;
            return *this;
        }

        /// <summary>
        /// Clears all components of the underlying URI in this uri_builder.
        /// </summary>
        void clear()
        {
            m_uri = details::uri_components();
        }

        /// <summary>
        /// Appends another path to the path of this uri_builder.
        /// </summary>
        /// <param name="path">Path to append as a already encoded string.</param>
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.</param>
        /// <returns>A reference to this uri_builder to support chaining.</returns>
        _ASYNCRTIMP uri_builder &append_path(const utility::string_t &path, bool do_encoding = false);

        /// <summary>
        /// Appends another query to the query of this uri_builder.
        /// </summary>
        /// <param name="query">Query to append as a decoded string.</param>
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.</param>
        /// <returns>A reference to this uri_builder to support chaining.</returns>
        _ASYNCRTIMP uri_builder &append_query(const utility::string_t &query, bool do_encoding = false);

        /// <summary>
        /// Appends an relative uri (Path, Query and fragment) at the end of the current uri.
        /// </summary>
        /// <param name="relative_uri">The relative uri to append.</param>
        /// <returns>A reference to this uri_builder to support chaining.</returns>
        _ASYNCRTIMP uri_builder &append(const uri &relative_uri);

        /// <summary>
        /// Appends another query to the query of this uri_builder, encoding it first. This overload is useful when building a query segment of
        /// the form "element=10", where the right hand side of the query is stored as a type other than a string, for instance, an integral type.
        /// </summary>
        /// <param name="name">The name portion of the query string</param>
        /// <param name="value">The value portion of the query string</param>
        /// <returns>A reference to this uri_builder to support chaining.</returns>
        template<typename T>
        uri_builder &append_query(const utility::string_t &name, const T &value, bool do_encoding = true)
        {
            auto encodedName = name;
            auto encodedValue = ::utility::conversions::print_string(value, std::locale::classic());

            if (do_encoding)
            {
                auto encodingCheck = [](int ch)
                {
                    switch (ch)
                    {
                        // Encode '&', ';', and '=' since they are used
                        // as delimiters in query component.
                    case '&':
                    case ';':
                    case '=':
                    case '%':
                    case '+':
                        return true;
                    default:
                        return !::web::details::uri_parser::is_query_character(ch);
                    }
                };
                encodedName = uri::encode_impl(encodedName, encodingCheck);
                encodedValue = uri::encode_impl(encodedValue, encodingCheck);
            }

            auto encodedQuery = encodedName;
            encodedQuery.append(_XPLATSTR("="));
            encodedQuery.append(encodedValue);
            // The query key value pair was already encoded by us or the user separately.
            return append_query(encodedQuery, false);
        }

        /// <summary>
        /// Combine and validate the URI components into a encoded string. An exception will be thrown if the URI is invalid.
        /// </summary>
        /// <returns>The created URI as a string.</returns>
        _ASYNCRTIMP utility::string_t to_string();

        /// <summary>
        /// Combine and validate the URI components into a URI class instance. An exception will be thrown if the URI is invalid.
        /// </summary>
        /// <returns>The create URI as a URI class instance.</returns>
        _ASYNCRTIMP uri to_uri();

        /// <summary>
        /// Validate the generated URI from all existing components of this uri_builder.
        /// </summary>
        /// <returns>Whether the URI is valid.</returns>
        _ASYNCRTIMP bool is_valid();

    private:
        details::uri_components m_uri;
    };
} // namespace web
