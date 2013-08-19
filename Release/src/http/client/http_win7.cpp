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
* http_client.cpp
*
* HTTP Library: Client-side APIs.
* 
* This file contains the implementation for Windows Vista, Windows 7 and Windows Server
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "cpprest/http_client_impl.h"

namespace web { namespace http
{
    namespace client
    {
        namespace details
        {
            // Helper function to query for the size of header values.
            static void query_header_length(HINTERNET request_handle, DWORD header, DWORD &length)
            {
                WinHttpQueryHeaders(
                    request_handle,
                    header,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    WINHTTP_NO_OUTPUT_BUFFER,
                    &length,
                    WINHTTP_NO_HEADER_INDEX);
            }

            // Helper function to get the status code from a WinHTTP response.
            static http::status_code parse_status_code(HINTERNET request_handle)
            {
                DWORD length = 0;
                query_header_length(request_handle, WINHTTP_QUERY_STATUS_CODE, length);
                utility::string_t buffer;
                buffer.resize(length);
                WinHttpQueryHeaders(
                    request_handle,
                    WINHTTP_QUERY_STATUS_CODE,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &buffer[0],
                    &length,
                    WINHTTP_NO_HEADER_INDEX);
                return (unsigned short)_wtoi(buffer.c_str());
            }

            // Helper function to get the reason phrase from a WinHTTP response.
            static utility::string_t parse_reason_phrase(HINTERNET request_handle)
            {
                utility::string_t phrase;
                DWORD length = 0;

                query_header_length(request_handle, WINHTTP_QUERY_STATUS_TEXT, length);
                phrase.resize(length);
                WinHttpQueryHeaders(
                    request_handle,
                    WINHTTP_QUERY_STATUS_TEXT,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &phrase[0],
                    &length,
                    WINHTTP_NO_HEADER_INDEX);
                // WinHTTP reports back the wrong length, trim any null characters.
                trim_nulls(phrase);
                return phrase;
            }

            /// <summary>
            /// Parses a string containing Http headers.
            /// </summary>
            static void parse_winhttp_headers(HINTERNET request_handle, _In_z_ utf16char *headersStr, http_response &response)
            {
                // Status code and reason phrase.
                response.set_status_code(parse_status_code(request_handle));
                response.set_reason_phrase(parse_reason_phrase(request_handle));

                parse_headers_string(headersStr, response.headers());
            }

            // Helper function to build an error message from a WinHTTP async result.
            static std::string build_callback_error_msg(_In_ WINHTTP_ASYNC_RESULT *error_result)
            {
                std::string error_msg("Error in: ");
                switch(error_result->dwResult)
                {
                case API_RECEIVE_RESPONSE:
                    error_msg.append("WinHttpReceiveResponse");
                    break;
                case API_QUERY_DATA_AVAILABLE:
                    error_msg.append("WinHttpQueryDataAvaliable");
                    break;
                case API_READ_DATA:
                    error_msg.append("WinHttpReadData");
                    break;
                case API_WRITE_DATA:
                    error_msg.append("WinHttpWriteData");
                    break;
                case API_SEND_REQUEST:
                    error_msg.append("WinHttpSendRequest");
                    break;
                default:
                    error_msg.append("Unknown WinHTTP Function");
                    break;
                }
                return error_msg;
            }

            class winhttp_client;

            struct winhttp_send_request_data
            {
                bool                       m_proxy_authentication_tried;
                bool                       m_server_authentication_tried;
            };

            class memory_holder
            {
                uint8_t* m_data;
                size_t m_allocated_size;
                bool m_internally_allocated;
                bool released;

                void release_memory()
                {
                    if (m_internally_allocated)
                    {
                        assert(m_data != nullptr);
                        delete[] m_data;
                        released = true;
                    }
                }
            public:
                memory_holder()
                    : m_data(nullptr), m_internally_allocated(false), m_allocated_size(0), released(false)
                {
                }

                ~memory_holder()
                {
                    assert(released == false);
                    release_memory();
                }

                memory_holder& operator=(memory_holder&& holder)
                {
                    assert(released == false);
                    release_memory();

                    // Copy the varialbes
                    m_data = holder.m_data;
                    m_internally_allocated = holder.m_internally_allocated;
                    m_allocated_size = holder.m_allocated_size;

                    // Null out the variables of holder
                    holder.m_data = nullptr;
                    holder.m_allocated_size = 0;
                    holder.m_internally_allocated = false;

                    released = false;

                    return *this;
                }

                void allocate_space(size_t length)
                {
                    // Do not allocate if you already hold a piece of memory which is not less than the one asked for
                    if (m_internally_allocated && length <= m_allocated_size)
                    {
                        return;
                    }

                    release_memory();
                    m_data = new uint8_t[length];
                    m_allocated_size = length;
                    m_internally_allocated = true;
                    released = false;
                }

                inline void reassign_to(_In_opt_ uint8_t *block)
                {
                    assert(block != nullptr);
                    release_memory();
                    m_data = block;
                    m_internally_allocated = false;
                    released = false;
                }

                inline bool is_internally_allocated()
                {
                    assert(released == false); 
                    return m_internally_allocated;
                }

                inline uint8_t* get()
                {
                    assert(released == false);
                    return m_data;
                }
            };

            // Additional information necessary to track a WinHTTP request.
            class winhttp_request_context : public request_context
            {
            public:

                // Factory function to create requests on the heap.
                static request_context * create_request_context(std::shared_ptr<_http_client_communicator> &client, http_request &request)
                {
                    return new winhttp_request_context(client, request);
                }

                ~winhttp_request_context()
                {
                    cleanup();
                }

                void allocate_request_space(_In_opt_ uint8_t *block, size_t length)
                {
                    if (block == nullptr)
                        request_data.allocate_space(length);
                    else
                        request_data.reassign_to(block);
                }

                void allocate_reply_space(_In_opt_ uint8_t *block, size_t length)
                {
                    if (block == nullptr)
                        reply_data.allocate_space(length);
                    else
                        reply_data.reassign_to(block);
                }

                bool is_externally_allocated()
                {
                    return transfered ? !reply_data.is_internally_allocated() : !request_data.is_internally_allocated();
                }

                void transfer_space()
                {
                    transfered = true;
                    reply_data = static_cast<memory_holder&&>(request_data);
                }

                HINTERNET m_request_handle;
                winhttp_send_request_data * m_request_data;

                bool m_need_to_chunk;
                bool m_too_large;
                bool transfered;
                size64_t m_remaining_to_write;

                size_t m_chunksize;

                std::char_traits<uint8_t>::pos_type m_readbuf_pos;

                memory_holder request_data;
                memory_holder reply_data;

            protected:

                virtual void cleanup()
                {
                    if(m_request_handle != nullptr)
                    {
                        auto tmp_handle = m_request_handle;
                        m_request_handle = nullptr;
                        WinHttpCloseHandle(tmp_handle);
                    }
                }

            private:

                // Can only create on the heap using factory function.
                winhttp_request_context(std::shared_ptr<_http_client_communicator> &client, http_request request)
                    : request_context(client, request), 
                    m_request_handle(nullptr), 
                    m_need_to_chunk(false), 
                    m_too_large(false),
                    m_chunksize(client->client_config().chunksize()),
                    m_readbuf_pos(0),
                    request_data(),
                    reply_data(),
                    m_remaining_to_write(0),
                    transfered(false)
                {
                }
            };

            static DWORD ChooseAuthScheme( DWORD dwSupportedSchemes )
            {
                //  It is the server's responsibility only to accept 
                //  authentication schemes that provide a sufficient
                //  level of security to protect the servers resources.
                //
                //  The client is also obligated only to use an authentication
                //  scheme that adequately protects its username and password.
                //
                if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NEGOTIATE )
                    return WINHTTP_AUTH_SCHEME_NEGOTIATE;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NTLM )
                    return WINHTTP_AUTH_SCHEME_NTLM;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_PASSPORT )
                    return WINHTTP_AUTH_SCHEME_PASSPORT;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_DIGEST )
                    return WINHTTP_AUTH_SCHEME_DIGEST;
                else if( dwSupportedSchemes & WINHTTP_AUTH_SCHEME_BASIC )
                    return WINHTTP_AUTH_SCHEME_BASIC;
                else
                    return 0;
            }

            // WinHTTP client.
            class winhttp_client : public _http_client_communicator
            {
            public:
                winhttp_client(const http::uri &address, const http_client_config& client_config) 
                    : _http_client_communicator(address, client_config), m_secure(address.scheme() == _XPLATSTR("https")), m_hSession(nullptr), m_hConnection(nullptr) { }

                // Closes session.
                ~winhttp_client()
                {
                    if(m_hConnection != nullptr)
                    {
                        if(WinHttpCloseHandle(m_hConnection) == NULL)
                        {
                            report_failure(_XPLATSTR("Error closing connection"));
                        }
                    }

                    if(m_hSession != nullptr)
                    {
                        // Unregister the callback.
                        if(!WinHttpSetStatusCallback(
                            m_hSession,
                            nullptr,
                            WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS,
                            NULL))
                        {
                            report_failure(_XPLATSTR("Error unregistering callback"));
                        }

                        if(WinHttpCloseHandle(m_hSession) == NULL)
                        {
                            report_failure(_XPLATSTR("Error closing session"));
                        }
                    }
                }

            protected:

                unsigned long report_failure(const utility::string_t& errorMessage)
                {
                    // Should we log?
                    UNREFERENCED_PARAMETER(errorMessage);

                    return GetLastError();
                }

                // Open session and connection with the server.
                unsigned long open()
                {
                    DWORD access_type;
                    LPCWSTR proxy_name;
                    utility::string_t proxy_str;
                    http::uri uri;

                    auto& config = client_config();

                    if(config.proxy().is_disabled())
                    {
                        access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
                        proxy_name = WINHTTP_NO_PROXY_NAME;
                    }
                    else if(config.proxy().is_default() || config.proxy().is_auto_discovery())
                    {
                        access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
                        proxy_name = WINHTTP_NO_PROXY_NAME;
                    }
                    else
                    {
                        _ASSERTE(config.proxy().is_specified());
                        access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
                        // WinHttpOpen cannot handle trailing slash in the name, so here is some string gymnastics to keep WinHttpOpen happy
                        // proxy_str is intentionally declared at the function level to avoid poiinting to the string in the destructed object
                        uri = config.proxy().address();
                        if(uri.is_port_default())
                        {
                            proxy_name = uri.host().c_str();
                        }
                        else
                        {
                            if (uri.port() > 0)
                            {
                                utility::ostringstream_t ss;
                                ss << uri.host() << _XPLATSTR(":") << uri.port();
                                proxy_str = ss.str();
                            }
                            else
                                proxy_str = uri.host();
                            proxy_name = proxy_str.c_str();
                        }
                    }

                    // Open session.
                    m_hSession = WinHttpOpen(
                        NULL,
                        access_type,
                        proxy_name,
                        WINHTTP_NO_PROXY_BYPASS,
                        WINHTTP_FLAG_ASYNC);
                    if(!m_hSession)
                    {
                        return report_failure(_XPLATSTR("Error opening session"));
                    }

                    // Set timeouts.
                    auto timeout = config.timeout();
                    int secs = static_cast<int>(timeout.count());

                    if(!WinHttpSetTimeouts(m_hSession, 
                        60000,
                        60000,
                        secs * 1000,
                        secs * 1000))
                    {
                        return report_failure(_XPLATSTR("Error setting timeouts"));
                    }

                    if(config.guarantee_order())
                    {
                        // Set max connection to use per server to 1.
                        DWORD maxConnections = 1;
                        if(!WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConnections, sizeof(maxConnections)))
                        {
                            return report_failure(_XPLATSTR("Error setting options"));
                        }
                    }

#if 0 // Work in progress. Enable this to support server certrificate revocation check
                    if( m_secure )
                    {
                        DWORD dwEnableSSLRevocOpt = WINHTTP_ENABLE_SSL_REVOCATION;
                        if(!WinHttpSetOption(m_hSession, WINHTTP_OPTION_ENABLE_FEATURE, &dwEnableSSLRevocOpt, sizeof(dwEnableSSLRevocOpt)))
                        {
                            DWORD dwError = GetLastError(); dwError;
                            return report_failure(U("Error enabling SSL revocation check"));
                        }
                    }
#endif
                    // Register asynchronous callback.
                    if(WINHTTP_INVALID_STATUS_CALLBACK == WinHttpSetStatusCallback(
                        m_hSession,
                        &winhttp_client::completion_callback,
                        WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES,
                        0))
                    {
                        return report_failure(_XPLATSTR("Error registering callback"));
                    }

                    // Open connection.
                    unsigned int port = m_uri.is_port_default() ? (m_secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT) : m_uri.port();
                    m_hConnection = WinHttpConnect(
                        m_hSession,
                        m_uri.host().c_str(),
                        (INTERNET_PORT)port,
                        0);

                    if(m_hConnection == nullptr)
                    {
                        return report_failure(_XPLATSTR("Error opening connection"));
                    }

                    return S_OK;
                }

                // Start sending request.
                void send_request(_In_ request_context *request)
                {
                    http_request &msg = request->m_request;
                    winhttp_request_context * winhttp_context = static_cast<winhttp_request_context *>(request);

                    // Need to form uri path, query, and fragment for this request.
                    // Make sure to keep any path that was specified with the uri when the http_client was created.
                    utility::string_t encoded_resource = http::uri_builder(m_uri).append(msg.relative_uri()).to_uri().resource().to_string();

                    WINHTTP_PROXY_INFO info;
                    bool proxy_info_required = false;

                    if( client_config().proxy().is_auto_discovery() )
                    {
                        WINHTTP_AUTOPROXY_OPTIONS autoproxy_options;
                        memset( &autoproxy_options, 0, sizeof(WINHTTP_AUTOPROXY_OPTIONS) );
                        memset( &info, 0, sizeof(WINHTTP_PROXY_INFO) );

                        autoproxy_options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
                        autoproxy_options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
                        autoproxy_options.fAutoLogonIfChallenged = TRUE;

                        auto result = WinHttpGetProxyForUrl(
                            m_hSession,
                            m_uri.to_string().c_str(),
                            &autoproxy_options,
                            &info );
                        if(result)
                        {
                            proxy_info_required = true;
                        }
                        else
                        {
                            // Failure to download the auto-configuration script is not fatal. Fall back to the default proxy.
                        }
                    }

                    // Open the request.
                    winhttp_context->m_request_handle = WinHttpOpenRequest(
                        m_hConnection,
                        msg.method().c_str(),
                        encoded_resource.c_str(),
                        nullptr,
                        WINHTTP_NO_REFERER,
                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                        WINHTTP_FLAG_ESCAPE_DISABLE | (m_secure ? WINHTTP_FLAG_SECURE : 0));
                    if(winhttp_context->m_request_handle == nullptr)
                    {
                        request->report_error(_XPLATSTR("Error opening request"));
                        return;
                    }

                    if( proxy_info_required )
                    {
                        auto result = WinHttpSetOption(
                            winhttp_context->m_request_handle,
                            WINHTTP_OPTION_PROXY,
                            &info, 
                            sizeof(WINHTTP_PROXY_INFO) );
                        if(!result)
                        {
                            request->report_error(_XPLATSTR("Error setting http proxy option"));
                            return;
                        }
                    }

                    // If credentials are specified, use autologon policy: WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH
                    //    => default credentials are not used.
                    // Else, the default autologon policy WINHTTP_AUTOLOGON_SECURITY_LEVEL_MEDIUM will be used.
                    if ( !client_config().credentials().username().empty() )
                    {
                        DWORD data = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;

                        auto result = WinHttpSetOption(
                            winhttp_context->m_request_handle,
                            WINHTTP_OPTION_AUTOLOGON_POLICY,
                            &data, 
                            sizeof(data));
                        if(!result)
                        {
                            request->report_error(_XPLATSTR("Error setting autologon policy to WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH."));
                            return;
                        }
                    }

                    // Check to turn off server certificate verification.
                    if(!client_config().validate_certificates())
                    {
                        DWORD data = SECURITY_FLAG_IGNORE_UNKNOWN_CA 
                            | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID 
                            | SECURITY_FLAG_IGNORE_CERT_CN_INVALID 
                            | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

                        auto result = WinHttpSetOption(
                            winhttp_context->m_request_handle,
                            WINHTTP_OPTION_SECURITY_FLAGS,
                            &data,
                            sizeof(data));
                        if(!result)
                        {
                            request->report_error(U("Error setting WinHttp to ignore server certification validation errors."));
                            return;
                        }
                    }

                    size_t content_length = msg._get_impl()->_get_content_length();
                    if (content_length > 0)
                    {
                        if ( msg.method() == http::methods::GET || msg.method() == http::methods::HEAD )
                        {
                            request->report_exception(http_exception(get_with_body));
                            return;
                        }

                        // There is a request body that needs to be transferred.

                        if (content_length == std::numeric_limits<size_t>::max()) 
                        {
                            // The content length is unknown and the application set a stream. This is an 
                            // indication that we will need to chunk the data.
                            winhttp_context->m_need_to_chunk = true;
                        }
                        else
                        {
                            // While we won't be transfer-encoding the data, we will write it in portions.
                            winhttp_context->m_too_large = true;
                            winhttp_context->m_remaining_to_write = content_length;
                        }
                    }


                    // Add headers.
                    if(!msg.headers().empty())
                    {
                        utility::string_t flattened_headers = flatten_http_headers(msg.headers());
                        if(!WinHttpAddRequestHeaders(
                            winhttp_context->m_request_handle,
                            flattened_headers.c_str(),
                            (DWORD)flattened_headers.length(),
                            WINHTTP_ADDREQ_FLAG_ADD))
                        {
                            request->report_error(_XPLATSTR("Error adding request headers"));
                            return;
                        }
                    }

                    m_request_data.m_proxy_authentication_tried = false;
                    m_request_data.m_server_authentication_tried = false;

                    winhttp_context->m_request_data = &m_request_data;

                    _start_request_send(winhttp_context, content_length);

                    return;
                }

            private:

                static bool _check_streambuf(_In_ winhttp_request_context * winhttp_context, concurrency::streams::streambuf<uint8_t> rdbuf, const utility::char_t* msg) 
                {
                    if ( !rdbuf.is_open() )
                    {
                        auto eptr = rdbuf.exception();
                        if ( !(eptr == nullptr) )
                            winhttp_context->report_exception(eptr);
                        else
                            winhttp_context->report_error(msg);
                    }
                    return rdbuf.is_open();
                }

                void _start_request_send(_In_ winhttp_request_context * winhttp_context, size_t content_length)
                {
                    if ( !winhttp_context->m_need_to_chunk && !winhttp_context->m_too_large )
                    {
                        // We have a message that is small enough to send in one chunk.

                        // Note: the contect object will go away as a side-effect of the WinHttpSendRequest() call,
                        // so we have to save the progress handler pointer.
                        auto progress = winhttp_context->m_request._get_impl()->_progress_handler();

                        if(!WinHttpSendRequest(
                            winhttp_context->m_request_handle,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            nullptr,
                            0,
                            0,
                            (DWORD_PTR)winhttp_context))
                        {
                            winhttp_context->report_error(_XPLATSTR("Error starting to send request"));
                        }

                        if ( progress )
                        {
                            (*progress)(message_direction::upload, 0);
                        }
						return;
                    }

                    // Capure the current read position of the stream.

                    auto rbuf = winhttp_context->_get_readbuffer();
                    if ( !_check_streambuf(winhttp_context, rbuf, _XPLATSTR("Input stream is not open")) )
                    {
                        return;
                    }

                    winhttp_context->m_readbuf_pos = rbuf.getpos(std::ios_base::in);

                    // If we find ourselves here, we either don't know how large the message
                    // body is, or it is larger than our threshold.

                    if(!WinHttpSendRequest(
                        winhttp_context->m_request_handle,
                        WINHTTP_NO_ADDITIONAL_HEADERS,
                        0,
                        nullptr,
                        0,
                        winhttp_context->m_too_large ? 
                        (DWORD)content_length : 
                    WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH,
                        (DWORD_PTR)winhttp_context))
                    {
                        winhttp_context->report_error(_XPLATSTR("Error starting to send chunked request"));
                    }
                }

                static void _transfer_encoding_chunked_write_data(_In_ winhttp_request_context * p_request_context)
                {
                    const size_t chunk_size = p_request_context->m_chunksize;

                    p_request_context->allocate_request_space(nullptr, chunk_size+http::details::chunked_encoding::additional_encoding_space);

                    auto rbuf = p_request_context->_get_readbuffer();
                    if ( !_check_streambuf(p_request_context, rbuf, _XPLATSTR("Input stream is not open")) )
                    {
                        return;
                    }

                    rbuf.getn(&p_request_context->request_data.get()[http::details::chunked_encoding::data_offset], chunk_size).then(
                        [p_request_context, chunk_size](pplx::task<size_t> op)
                    {
                        size_t read;
                        try { read = op.get(); }
                        catch (...)
                        {
                            p_request_context->report_exception(std::current_exception());
                            return;
                        }

                        _ASSERTE(read != static_cast<size_t>(-1));

                        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(p_request_context->request_data.get(), chunk_size+http::details::chunked_encoding::additional_encoding_space, read);

                        if ( read == 0 )
                            p_request_context->m_need_to_chunk = false;

                        auto length = read+(http::details::chunked_encoding::additional_encoding_space-offset);

                        if(!WinHttpWriteData(
                            p_request_context->m_request_handle,
                            &p_request_context->request_data.get()[offset],
                            (DWORD)length,
                            nullptr))
                        {
                            p_request_context->report_error(_XPLATSTR("Error writing data"));
                        }

                    });
                }

                static void _multiple_segment_write_data(_In_ winhttp_request_context * p_request_context)
                {
                    auto rbuf = p_request_context->_get_readbuffer();
                    if ( !_check_streambuf(p_request_context, rbuf, _XPLATSTR("Input stream is not open")) )
                    {
                        return;
                    }

                    const size_t chunk_size = p_request_context->m_chunksize;

                    msl::utilities::SafeInt<size64_t> safeCount = p_request_context->m_remaining_to_write;
                    safeCount = safeCount.Min(chunk_size);

                    uint8_t*  block = nullptr; 
                    size_t length = 0;
                    if ( rbuf.acquire(block, length) )
                    {
                        if ( length == 0 )
                        {
                            // Unexpected end-of-stream.
                            if (!(rbuf.exception() == nullptr))
                                p_request_context->report_exception(rbuf.exception());
                            else
                                p_request_context->report_error(_XPLATSTR("Error reading outgoing HTTP body from its stream."));
                            return;
                        }

                        p_request_context->allocate_request_space(block, length);

                        size_t to_write = safeCount.Min(length);

                        p_request_context->m_remaining_to_write -= to_write;

                        if ( p_request_context->m_remaining_to_write == 0 )
                        {
                            p_request_context->m_too_large = false;
                        }

                        if( !WinHttpWriteData(
                            p_request_context->m_request_handle,
                            p_request_context->request_data.get(),
                            (DWORD)to_write,
                            nullptr))
                        {
                            p_request_context->report_error(_XPLATSTR("Error writing data"));
                        }      

                        return;
                    }

                    size_t next_chunk_size = safeCount.Min(chunk_size);

                    p_request_context->allocate_request_space(nullptr, next_chunk_size);

                    rbuf.getn(p_request_context->request_data.get(), next_chunk_size).then(
                        [p_request_context, next_chunk_size](pplx::task<size_t> op)
                    {
                        size_t read;
                        try { read = op.get(); } catch (...)
                        {
                            p_request_context->report_exception(std::current_exception());
                            return;
                        }

                        _ASSERTE(read != static_cast<size_t>(-1));

                        p_request_context->m_remaining_to_write -= read;

                        if ( read == 0 || p_request_context->m_remaining_to_write == 0 )
                        {
                            p_request_context->m_too_large = false;
                        }

                        if( !WinHttpWriteData(
                            p_request_context->m_request_handle,
                            p_request_context->request_data.get(),
                            (DWORD)read,
                            nullptr))
                        {
                            p_request_context->report_error(_XPLATSTR("Error writing data"));
                        }       
                    });
                }

                // Returns true if the request was completed, or false if a new request with credentials was issued
                static bool handle_authentication_failure(
                    HINTERNET hRequestHandle,
                    _In_ winhttp_request_context * p_request_context
                    )
                {
                    http_response & response = p_request_context->m_response;
                    http_request & request = p_request_context->m_request;

                    _ASSERTE(response.status_code() == status_codes::Unauthorized  || response.status_code() == status_codes::ProxyAuthRequired);

                    // If the application set a stream for the request body, we can only resend if the input stream supports
                    // seeking and we are also successful in seeking to the position we started at when the original request
                    // was sent.

                    bool can_resend = false;
                    bool got_credentials = false;
                    BOOL results;
                    DWORD dwSupportedSchemes;
                    DWORD dwFirstScheme;
                    DWORD dwTarget = 0;
                    DWORD dwSelectedScheme = 0;
                    string_t username;
                    string_t password;

                    if (request.body())
                    {
                        // Valid request stream => msg has a body that needs to be resend

                        auto rdpos = p_request_context->m_readbuf_pos;

                        // Check if the saved read position is valid
                        if (rdpos != (std::char_traits<uint8_t>::pos_type)-1)
                        {
                            auto rbuf = p_request_context->_get_readbuffer();

                            if ( !rbuf.is_open() )
                            {
                                can_resend = false;
                            }

                            // Try to seek back to the saved read position
                            if ( rbuf.seekpos(rdpos, std::ios::ios_base::in) == rdpos )
                            {
                                // Success.
                                can_resend = true;
                            }
                        }
                    }
                    else
                    {
                        // There is no msg body
                        can_resend = true;
                    }

                    if (can_resend)
                    {
                        // The proxy requires authentication.  Sending credentials...
                        // Obtain the supported and preferred schemes.
                        results = WinHttpQueryAuthSchemes( hRequestHandle, 
                            &dwSupportedSchemes, 
                            &dwFirstScheme, 
                            &dwTarget );

                        if (!results)
                        {
                            // This will return the authentication failure to the user, without reporting fatal errors
                            return true;
                        }

                        dwSelectedScheme = ChooseAuthScheme( dwSupportedSchemes);
                        if( dwSelectedScheme == 0 )
                        {
                            // This will return the authentication failure to the user, without reporting fatal errors
                            return true;
                        }

                        if(response.status_code() == status_codes::ProxyAuthRequired /*407*/ && !p_request_context->m_request_data->m_proxy_authentication_tried)
                        {
                            // See if the credentials on the proxy were set. If not, there are no credentials to supply hence we cannot resend
                            web_proxy proxy = p_request_context->m_http_client->client_config().proxy();
                            // No need to check if proxy is disabled, because disabled proxies cannot have credentials set on them
                            credentials cred = proxy.credentials();
                            if(cred.is_set())
                            {
                                username = cred.username();
                                password = cred.password();
                                dwTarget = WINHTTP_AUTH_TARGET_PROXY;
                                got_credentials = !username.empty();
                                p_request_context->m_request_data->m_proxy_authentication_tried = true;
                            }
                        }
                        else if(response.status_code() == status_codes::Unauthorized /*401*/ && !p_request_context->m_request_data->m_server_authentication_tried)
                        {
                            username = p_request_context->m_http_client->client_config().credentials().username();
                            password = p_request_context->m_http_client->client_config().credentials().password();
                            dwTarget = WINHTTP_AUTH_TARGET_SERVER;
                            got_credentials = !username.empty();
                            p_request_context->m_request_data->m_server_authentication_tried = true;
                        }
                    }

                    if( !(can_resend && got_credentials) )
                    {
                        // Either we cannot resend, or the user did not provide non-empty credentials.
                        // Return the authentication failure to the user.
                        return true;
                    }

                    results = WinHttpSetCredentials( hRequestHandle,
                        dwTarget, 
                        dwSelectedScheme,
                        username.c_str(),
                        password.c_str(),
                        nullptr );
                    if(!results)
                    {
                        // This will return the authentication failure to the user, without reporting fatal errors
                        return true;
                    }

                    // Figure out how the data should be sent, if any
                    size_t content_length = request._get_impl()->_get_content_length();

                    if (content_length > 0)
                    {
                        // There is a request body that needs to be transferred.

                        if (content_length == std::numeric_limits<size_t>::max()) 
                        {
                            // The content length is unknown and the application set a stream. This is an 
                            // indication that we will need to chunk the data.
                            p_request_context->m_need_to_chunk = true;
                        }
                        else
                        {
                            // While we won't be transfer-encoding the data, we will write it in portions.
                            p_request_context->m_too_large = true;
                            p_request_context->m_remaining_to_write = content_length;
                        }
                    }

                    // We're good.
                    winhttp_client* winclnt = reinterpret_cast<winhttp_client*>(p_request_context->m_http_client.get());
                    winclnt->_start_request_send(p_request_context, content_length);

                    // We will not complete the request. Instead wait for the response to the request that was resent
                    return false;
                }

                // Callback used with WinHTTP to listen for async completions.
                static void CALLBACK completion_callback(
                    HINTERNET hRequestHandle,
                    DWORD_PTR context,
                    DWORD statusCode,
                    _In_ void* statusInfo,
                    DWORD statusInfoLength)
                {
                    UNREFERENCED_PARAMETER(statusInfoLength);
                    
                    if ( statusCode == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING )
                        return;

                    winhttp_request_context * p_request_context = reinterpret_cast<winhttp_request_context *>(context);

                    if(p_request_context != nullptr)
                    {
                        switch (statusCode)
                        {
                        case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR :
                            {
                                WINHTTP_ASYNC_RESULT *error_result = reinterpret_cast<WINHTTP_ASYNC_RESULT *>(statusInfo);

                                p_request_context->report_error(error_result->dwError, utility::conversions::to_string_t(build_callback_error_msg(error_result)));
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE :
                            {
                                if ( p_request_context->m_need_to_chunk )
                                {
                                    _transfer_encoding_chunked_write_data(p_request_context);
                                }
                                else if ( p_request_context->m_too_large )
                                {
                                    _multiple_segment_write_data(p_request_context);
                                }
                                else 
                                {
                                    if(!WinHttpReceiveResponse(hRequestHandle, nullptr))
                                    {
                                        p_request_context->report_error(_XPLATSTR("Error receiving response"));
                                    }
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE :
                            {
                                DWORD bytesWritten = *((DWORD *)statusInfo);
                                _ASSERTE(statusInfoLength == sizeof(DWORD));

                                if ( bytesWritten > 0 )
                                {
                                    auto progress = p_request_context->m_request._get_impl()->_progress_handler();
                                    if ( progress )
                                    {
                                        p_request_context->m_uploaded += (size64_t)bytesWritten;
                                        (*progress)(message_direction::upload, p_request_context->m_uploaded);
                                    }
                                }

                                if ( p_request_context->is_externally_allocated() )
                                {
                                    p_request_context->_get_readbuffer().release(p_request_context->request_data.get(), bytesWritten);
                                }

                                if ( p_request_context->m_need_to_chunk )
                                {
                                    _transfer_encoding_chunked_write_data(p_request_context);
                                }
                                else if ( p_request_context->m_too_large )
                                {
                                    _multiple_segment_write_data(p_request_context);
                                }
                                else
                                {
                                    if(!WinHttpReceiveResponse(hRequestHandle, nullptr))
                                    {
                                        p_request_context->report_error(_XPLATSTR("Error receiving response"));
                                    }
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE :
                            {
                                // Clear the space allocated for the request.
                                p_request_context->transfer_space();

                                // First need to query to see what the headers size is.
                                DWORD headerBufferLength = 0;
                                query_header_length(hRequestHandle, WINHTTP_QUERY_RAW_HEADERS_CRLF, headerBufferLength);

                                // Now allocate buffer for headers and query for them.
                                std::vector<unsigned char> header_raw_buffer;
                                header_raw_buffer.resize(headerBufferLength);
                                utf16char * header_buffer = reinterpret_cast<utf16char *>(&header_raw_buffer[0]);
                                if(!WinHttpQueryHeaders(
                                    hRequestHandle,
                                    WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    header_buffer,
                                    &headerBufferLength,
                                    WINHTTP_NO_HEADER_INDEX))
                                {
                                    p_request_context->report_error(_XPLATSTR("Error receiving http headers"));
                                    return;
                                }

                                http_response & response = p_request_context->m_response;
                                parse_winhttp_headers(hRequestHandle, header_buffer, response);

                                if(response.status_code() == status_codes::Unauthorized /*401*/ ||
                                    response.status_code() == status_codes::ProxyAuthRequired /*407*/)
                                {
                                    bool completed = handle_authentication_failure(hRequestHandle, p_request_context);
                                    if( !completed )
                                    {
                                        // The request was not completed but resent with credentials. Wait until we get a new response
                                        return;
                                    }
                                }

                                // Signal that the headers are available.
                                p_request_context->complete_headers();

                                // If the method was 'HEAD,' the body of the message is by definition empty. No need to 
                                // read it. Any headers that suggest the presence of a body can safely be ignored.
                                if (p_request_context->m_request.method() == methods::HEAD )
                                {
                                    p_request_context->allocate_request_space(nullptr, 0);
                                    p_request_context->complete_request(0);
                                    return;
                                }

                                size_t content_length = 0;
                                utility::string_t transfer_encoding;

                                bool has_cnt_length = response.headers().match(header_names::content_length, content_length);
                                bool has_xfr_encode = response.headers().match(header_names::transfer_encoding, transfer_encoding);

                                // Determine if there is a msg body that needs to be read
                                bool hasMsgBody = (has_cnt_length && (content_length > 0)) || (has_xfr_encode);

                                if (!hasMsgBody)
                                {
                                    auto progress = p_request_context->m_request._get_impl()->_progress_handler();
                                    if ( progress )
                                    {
                                        (*progress)(message_direction::download, 0);
                                    }

                                    p_request_context->complete_request(0);
                                    return;
                                }

                                // HTTP Specification states:
                                // If a message is received with both a Transfer-Encoding header field 
                                // and a Content-Length header field, the latter MUST be ignored.

                                // At least one of them should be set
                                _ASSERTE(has_xfr_encode || has_cnt_length);

                                // For both cases (with/without transfer-encoding), we will read the data as they are coming from the server.
                                // WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE callback determines whether this function was successful and the value of the parameters.
                                if(!WinHttpQueryDataAvailable(hRequestHandle, nullptr))
                                {
                                    p_request_context->report_error(_XPLATSTR("Error querying for http body data"));
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE :
                            {
                                // Status information contains pointer to DWORD containing number of bytes available.
                                DWORD num_bytes = *(PDWORD)statusInfo;

                                if(num_bytes > 0)
                                {
                                    auto writebuf = p_request_context->_get_writebuffer();
                                    if ( !_check_streambuf(p_request_context, writebuf, _XPLATSTR("Output stream is not open")) )
                                        break;

                                    p_request_context->allocate_reply_space(writebuf.alloc(num_bytes), num_bytes);

                                    // Read in body all at once.
                                    if(!WinHttpReadData(
                                        hRequestHandle,
                                        (LPVOID)p_request_context->reply_data.get(),
                                        (DWORD)num_bytes,
                                        nullptr))
                                    {
                                        p_request_context->report_error(_XPLATSTR("Error receiving http body chunk"));
                                    }
                                }
                                else
                                {
                                    p_request_context->complete_request(p_request_context->m_total_response_size);
                                }
                                break;
                            }
                        case WINHTTP_CALLBACK_STATUS_READ_COMPLETE :
                            {
                                // Status information length contains the number of bytes read.
                                // WinHTTP will always fill the whole buffer or read nothing.
                                // If number of bytes read is zero than we have reached the end.

                                if(statusInfoLength > 0)
                                {
                                    auto progress = p_request_context->m_request._get_impl()->_progress_handler();
                                    if ( progress )
                                    {
                                        p_request_context->m_downloaded += (size64_t)statusInfoLength;
                                        (*progress)(message_direction::download, p_request_context->m_downloaded);
                                    }

                                    auto writebuf = p_request_context->_get_writebuffer();
                                    if ( !_check_streambuf(p_request_context, writebuf, _XPLATSTR("Output stream is not open")) )
                                        break;

                                    auto after_sync = 
                                        [hRequestHandle, p_request_context]
                                    (pplx::task<void> sync_op)
                                    {
                                        try { sync_op.wait(); } catch(...)
                                        {
                                            p_request_context->report_exception(std::current_exception());
                                            return;
                                        }

                                        // Look for more data
                                        if( !WinHttpQueryDataAvailable(hRequestHandle, nullptr))
                                        {
                                            p_request_context->report_error(_XPLATSTR("Error querying for http body chunk"));
											return;
                                        }
                                    };

                                    if ( p_request_context->is_externally_allocated() )
                                    {
                                        p_request_context->m_total_response_size += statusInfoLength;
                                        writebuf.commit(statusInfoLength);

                                        writebuf.sync().then(after_sync);
                                    }
                                    else 
                                    {
                                        writebuf.putn(p_request_context->reply_data.get(), statusInfoLength).then(
                                            [hRequestHandle, p_request_context, statusInfoLength, after_sync]
                                        (pplx::task<size_t> op)
                                        {
                                            size_t written = 0;
                                            try { written = op.get(); } catch(...)
                                            {
                                                p_request_context->report_exception(std::current_exception());
                                                return;
                                            }

                                            p_request_context->m_total_response_size += written;

                                            // If we couldn't write everything, it's time to exit.
                                            if ( written != statusInfoLength ) 
                                            {
                                                p_request_context->report_exception(std::runtime_error("response stream unexpectedly failed to write the requested number of bytes"));
                                                return;
                                            }

                                            auto wbuf = p_request_context->_get_writebuffer();
                                            if ( !_check_streambuf(p_request_context, wbuf, _XPLATSTR("Output stream is not open")) )
                                                return;

                                            wbuf.sync().then(after_sync);
                                        });
                                    }
                                }
                                else
                                {
                                    // Done reading so set task completion event and close the request handle.
                                    p_request_context->complete_request(p_request_context->m_total_response_size);
                                }
                                break;
                            }
                        }
                    }
                }

                // WinHTTP session and connection
                HINTERNET m_hSession;
                HINTERNET m_hConnection;
                winhttp_send_request_data m_request_data;
                bool      m_secure;

                // No copy or assignment.
                winhttp_client(const winhttp_client&);
                winhttp_client &operator=(const winhttp_client&);
            };


        } // namespace details

        // Helper function to check to make sure the uri is valid.
        static void verify_uri(const uri &uri)
        {
            // Somethings like proper URI schema are verified by the URI class.
            // We only need to check certain things specific to HTTP.
            if( uri.scheme() != _XPLATSTR("http") && uri.scheme() != _XPLATSTR("https") )
            {
                throw std::invalid_argument("URI scheme must be 'http' or 'https'");
            }

            if(uri.host().empty())
            {
                throw std::invalid_argument("URI must contain a hostname.");
            }
        }

        class http_network_handler : public http_pipeline_stage
        {
        public:

            http_network_handler(const uri &base_uri, const http_client_config& client_config) :
                m_http_client_impl(std::make_shared<details::winhttp_client>(base_uri, client_config))
            {
            }

            ~ http_network_handler()
            {
            }

            virtual pplx::task<http_response> propagate(http_request request)
            {

                details::request_context * context = details::winhttp_request_context::create_request_context(m_http_client_impl, request);

                // Use a task to externally signal the final result and completion of the task.
                auto result_task = pplx::create_task(context->m_request_completion);

                // Asynchronously send the response with the HTTP client implementation.
                m_http_client_impl->async_send_request(context);

                return result_task;
            }

            const std::shared_ptr<details::_http_client_communicator>& http_client_impl() const
            {
                return m_http_client_impl;
            }

        private:
            std::shared_ptr<details::_http_client_communicator> m_http_client_impl;
        };

        http_client::http_client(const uri &base_uri)
			:_base_uri(base_uri)
        {
            build_pipeline(base_uri, http_client_config());
        }

        http_client::http_client(const uri &base_uri, const http_client_config& client_config)
			:_base_uri(base_uri)
        {
            build_pipeline(base_uri, client_config);
        }

        void http_client::build_pipeline(const uri &base_uri, const http_client_config& client_config)
        {
            verify_uri(base_uri);
            m_pipeline = ::web::http::http_pipeline::create_pipeline(std::make_shared<http_network_handler>(base_uri, client_config));
        }

        pplx::task<http_response> http_client::request(http_request request)
        {
            return m_pipeline->propagate(request);
        }

        const http_client_config& http_client::client_config() const
        {
            http_network_handler* ph = static_cast<http_network_handler*>(m_pipeline->last_stage().get());
            return ph->http_client_impl()->client_config();
        }

    } // namespace client
}} // namespace casablanca::http
