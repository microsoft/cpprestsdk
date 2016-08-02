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
* HTTP Library: Client-side APIs.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "cpprest/details/basic_types.h"
#include "cpprest/astreambuf.h"
#include "cpprest/http_client.h"
#include "cpprest/http_msg.h"
#include <stdexcept>
#include <string>
#include <memory>

namespace web { namespace http { namespace client { namespace details {

class _http_client_communicator;

// Request context encapsulating everything necessary for creating and responding to a request.
class request_context
{
public:

    // Destructor to clean up any held resources.
    virtual ~request_context() {}

    virtual void report_exception(std::exception_ptr exceptionPtr);

    virtual concurrency::streams::streambuf<uint8_t> _get_readbuffer();

    void complete_headers();

    /// <summary>
    /// Completes this request, setting the underlying task completion event, and cleaning up the handles
    /// </summary>
    void complete_request(utility::size64_t body_size);

    void report_error(unsigned long error_code, const std::string &errorMessage);

#ifdef _WIN32
    void report_error(unsigned long error_code, const std::wstring &errorMessage);
#endif

    template<typename _ExceptionType>
    void report_exception(const _ExceptionType &e)
    {
        report_exception(std::make_exception_ptr(e));
    }

    concurrency::streams::streambuf<uint8_t> _get_writebuffer();

    // Reference to the http_client implementation.
    std::shared_ptr<_http_client_communicator> m_http_client;

    // request/response pair.
    http_request m_request;
    http_response m_response;

    utility::size64_t m_uploaded;
    utility::size64_t m_downloaded;

    // task completion event to signal request is completed.
    pplx::task_completion_event<http_response> m_request_completion;

    // Registration for cancellation notification if enabled.
    pplx::cancellation_token_registration m_cancellationRegistration;

protected:

    request_context(const std::shared_ptr<_http_client_communicator> &client, const http_request &request);

    virtual void finish();
};

//
// Interface used by client implementations. Concrete implementations are responsible for
// sending HTTP requests and receiving the responses.
//
class _http_client_communicator : public http_pipeline_stage
{
public:

    virtual ~_http_client_communicator() override = default;

    // Asynchronously send a HTTP request and process the response.
    void async_send_request(const std::shared_ptr<request_context> &request);

    void finish_request();

    const http_client_config& client_config() const;

    const uri & base_uri() const;

protected:
    _http_client_communicator(http::uri&& address, http_client_config&& client_config);

    // Method to open client.
    virtual unsigned long open() = 0;

    // HTTP client implementations must implement send_request.
    virtual void send_request(_In_ const std::shared_ptr<request_context> &request) = 0;

    // URI to connect to.
    const http::uri m_uri;

private:

    http_client_config m_client_config;

    bool m_opened;

    pplx::extensibility::critical_section_t m_open_lock;

    // Wraps opening the client around sending a request.
    void open_and_send_request(const std::shared_ptr<request_context> &request);

    unsigned long open_if_required();

    void push_request(const std::shared_ptr<request_context> &request);

    // Queue used to guarantee ordering of requests, when applicable.
    std::queue<std::shared_ptr<request_context>> m_requests_queue;
    int m_scheduled;
};

/// <summary>
/// Factory function implemented by the separate platforms to construct their subclasses of _http_client_communicator
/// </summary>
std::shared_ptr<_http_client_communicator> create_platform_final_pipeline_stage(uri&& base_uri, http_client_config&& client_config);

}}}}