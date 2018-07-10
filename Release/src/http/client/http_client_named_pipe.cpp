/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: Client-side APIs.
*
* This file contains the implementation for Windows Desktop, based on WinHTTP.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#include "cpprest/http_headers.h"
#include "http_client_impl.h"

#include "../common/internal_http_helpers.h"

#include <boost/algorithm/string/predicate.hpp>

#ifndef CPPREST_TARGET_XP
#include <VersionHelpers.h>
#endif

#undef min
#undef max

#define CRLF std::string("\r\n")

namespace web
{
namespace http
{
namespace client
{
namespace details
{

// Helper function to trim leading and trailing null characters from a string.
static void trim_nulls(utility::string_t &str)
{
    size_t index;
    for (index = 0; index < str.size() && str[index] == 0; ++index);
    str.erase(0, index);
    for (index = str.size(); index > 0 && str[index - 1] == 0; --index);
    str.erase(index);
}

// Helper function to build error messages.
static std::string build_error_msg(unsigned long code, const std::string &location)
{
    std::string msg(location);
    msg.append(": ");
    msg.append(std::to_string(code));
    msg.append(": ");
    msg.append(utility::details::windows_category().message(code));
    return msg;
}

class memory_holder
{
    uint8_t* m_externalData;
    std::vector<uint8_t> m_internalData;

public:
    memory_holder() : m_externalData(nullptr)
    {
    }

    void allocate_space(size_t length)
    {
        if (length > m_internalData.size())
        {
            m_internalData.resize(length);
        }
        m_externalData = nullptr;
    }

    inline void reassign_to(_In_opt_ uint8_t *block)
    {
        assert(block != nullptr);
        m_externalData = block;
    }

    inline bool is_internally_allocated() const
    {
        return m_externalData == nullptr;
    }

    inline uint8_t* get()
    {
        return is_internally_allocated() ? &m_internalData[0] : m_externalData ;
    }
};

// Possible ways a message body can be sent/received.
enum msg_body_type
{
    no_body,
    content_length_chunked,
    transfer_encoding_chunked
};

// Additional information necessary to track a WinHTTP request.
class named_pipe_request_context : public request_context
{
public:

    // Factory function to create requests on the heap.
    static std::shared_ptr<request_context> create_request_context(const std::shared_ptr<_http_client_communicator> &client, const http_request &request)
    {
        std::shared_ptr<named_pipe_request_context> ret(new named_pipe_request_context(client, request));
        ret->m_self_reference = ret;
        return std::move(ret);
    }

    ~named_pipe_request_context()
    {
        cleanup();
    }

    msg_body_type m_bodyType;

    utility::size64_t m_remaining_to_write;

    std::char_traits<uint8_t>::pos_type m_startingPosition;

    // If the user specified that to guarantee data buffering of request data, in case of challenged authentication requests, etc...
    // Then if the request stream buffer doesn't support seeking we need to copy the body chunks as it is sent.
    concurrency::streams::istream m_readStream;
    std::unique_ptr<concurrency::streams::container_buffer<std::vector<uint8_t>>> m_readBufferCopy;
    virtual concurrency::streams::streambuf<uint8_t> _get_readbuffer()
    {
        return m_readStream.streambuf();
    }

    // This self reference will keep us alive until finish() is called.
    std::shared_ptr<named_pipe_request_context> m_self_reference;
    memory_holder m_body_data;
    std::unique_ptr<web::http::details::compression::stream_decompressor> decompressor;

    virtual void cleanup()
    {
        // TODO
    }

protected:

    virtual void finish()
    {
        request_context::finish();
        assert(m_self_reference != nullptr);
        auto dereference_self = std::move(m_self_reference);
        // As the stack frame cleans up, this will be deleted if no other references exist.
    }

private:

    // Can only create on the heap using factory function.
    named_pipe_request_context(const std::shared_ptr<_http_client_communicator> &client, const http_request &request)
        : request_context(client, request),
        m_bodyType(no_body),
        m_startingPosition(std::char_traits<uint8_t>::eof()),
        m_body_data(),
        m_remaining_to_write(0),
        m_readStream(request.body())
    {
    }
};

// named pipe client.
class named_pipe_client : public _http_client_communicator
{
public:
    static const DWORD PIPE_BUFFER_SIZE = 64 * 1024;

    named_pipe_client(http::uri address, http_client_config client_config)
        : _http_client_communicator(std::move(address), std::move(client_config))
    {}

    named_pipe_client(const named_pipe_client&) = delete;
    named_pipe_client &operator=(const named_pipe_client&) = delete;

    // Closes session.
    ~named_pipe_client()
    {
        // TODO
    }

    virtual pplx::task<http_response> propagate(http_request request) override
    {
        auto self = std::static_pointer_cast<_http_client_communicator>(shared_from_this());
        auto context = details::named_pipe_request_context::create_request_context(self, request);

        // Use a task to externally signal the final result and completion of the task.
        auto result_task = pplx::create_task(context->m_request_completion);

        // Asynchronously send the response with the HTTP client implementation.
        this->async_send_request(context);

        return result_task;
    }

protected:

    unsigned long report_failure(const utility::string_t& errorMessage)
    {
        // Should we log?
        CASABLANCA_UNREFERENCED_PARAMETER(errorMessage);

        return GetLastError();
    }

    // Open session and connection with the server.
    virtual unsigned long open() override
    {
        const auto& config = client_config();

        // TODO: Validate client config; error out on any of these conditions
        //       * config.proxy() is enabled
        //       * config.timeout is specified
        //       * config.guarantee_order is specified
        //       * config.credentials is specified
        //       * config.validate_certificates is specified
        //       * config.request_compressed_response is specified

        return S_OK;
    }

    // Start sending request.
    void send_request(_In_ const std::shared_ptr<request_context> &request)
    {
        const auto& config = client_config();

        http_request &msg = request->m_request;
        std::shared_ptr<named_pipe_request_context> named_pipe_context = std::static_pointer_cast<named_pipe_request_context>(request);
        std::weak_ptr<named_pipe_request_context> weak_named_pipe_context = named_pipe_context;

        // TODO: validate uri
        auto path = uri::split_path(base_uri().path());

        utility::string_t pipe_name = U("\\\\.\\pipe\\") + path[0];

        // TODO: Need to close pipe handle
        // TODO: Add timeout
        m_namedPipe = CreateFile(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (m_namedPipe == INVALID_HANDLE_VALUE)
        {
            auto errorCode = GetLastError();
            named_pipe_context->report_error(errorCode, build_error_msg(errorCode, "Error opening named pipe"));
            return;
        }

        DWORD readMode = PIPE_READMODE_BYTE;
        auto success = SetNamedPipeHandleState(
            m_namedPipe,
            &readMode,
            NULL,
            NULL);
        if (!success)
        {
            auto errorCode = GetLastError();
            named_pipe_context->report_error(errorCode, build_error_msg(errorCode, "Error setting read mode on named pipe"));
            return;
        }

        config._invoke_nativesessionhandle_options(m_namedPipe);

        // Need to form uri path, query, and fragment for this request.
        // Make sure to keep any path that was specified with the uri when the http_client was created.
        const utility::string_t encoded_resource = http::uri_builder(m_uri).append(msg.relative_uri()).to_uri().resource().to_string();

        const size_t content_length = msg._get_impl()->_get_content_length();
        if (content_length > 0)
        {
            if ( msg.method() == http::methods::GET || msg.method() == http::methods::HEAD )
            {
                request->report_exception(http_exception(get_with_body_err_msg));
                return;
            }

            // TODO: Add support for chunked requests

            // There is a request body that needs to be transferred.
            if (content_length == std::numeric_limits<size_t>::max())
            {
                // The content length is unknown and the application set a stream. This is an
                // indication that we will use transfer encoding chunked.
                named_pipe_context->m_bodyType = transfer_encoding_chunked;
            }
            else
            {
                // While we won't be transfer-encoding the data, we will write it in portions.
                named_pipe_context->m_bodyType = content_length_chunked;
                named_pipe_context->m_remaining_to_write = content_length;
            }
        }

        // TODO: Need better buffer management
        std::ostringstream request_stream;
        request_stream.imbue(std::locale::classic());
        const auto &host = utility::conversions::to_utf8string(m_uri.host());

        request_stream << utility::conversions::to_utf8string(msg.method()) << " " << utility::conversions::to_utf8string(encoded_resource) << " " << "HTTP/1.1" << CRLF;

        // Add the Host header if user has not specified it explicitly
        if (!msg.headers().has(header_names::host))
        {
            request_stream << "Host: " << host << CRLF;
        }

        // Add headers.
        if (!msg.headers().empty())
        {
            request_stream << utility::conversions::to_utf8string(::web::http::details::flatten_http_headers(msg.headers()));
        }

        // TODO: Add support for "Connection: Keep-Alive" ?
        //// Enforce HTTP connection keep alive (even for the old HTTP/1.0 protocol).
        //request_stream << "Connection: Keep-Alive" << CRLF << CRLF;

        // Add the body
        if (msg.body()) {
            auto rbuf = request->_get_readbuffer();

            uint8_t*  block = nullptr;
            size_t length = 0;
            if (rbuf.acquire(block, length)) {
                request_stream << CRLF << std::string(reinterpret_cast<char*>(block), length);
            }
        }

        // Register for notification on cancellation to abort this request.
        if(msg._cancellation_token() != pplx::cancellation_token::none())
        {
            // cancellation callback is unregistered when request is completed.
            named_pipe_context->m_cancellationRegistration = msg._cancellation_token().register_callback([weak_named_pipe_context]()
            {
                auto lock = weak_named_pipe_context.lock();
                if (!lock) return;
                lock->cleanup();
            });
        }

        // Call the callback function of user customized options.
        try
        {
            client_config().invoke_nativehandle_options(m_namedPipe);
        }
        catch (...)
        {
            request->report_exception(std::current_exception());
            return;
        }

        _start_request_send(named_pipe_context, content_length, request_stream);
    }

private:

    void _start_request_send(const std::shared_ptr<named_pipe_request_context>& named_pipe_context, size_t content_length, std::ostringstream& request_stream)
    {
        DWORD bytesWritten = 0;

        auto success = WriteFile(m_namedPipe, request_stream.str().c_str(), static_cast<DWORD>(request_stream.str().length()), &bytesWritten, nullptr);

        if(!success) {
            auto errorCode = GetLastError();
            named_pipe_context->report_error(errorCode, build_error_msg(errorCode, "WriteFile"));
        }

        auto buffer = std::unique_ptr<unsigned char[]>(new unsigned char[msl::safeint3::SafeInt<unsigned long>(named_pipe_client::PIPE_BUFFER_SIZE)]);

        DWORD bytesRead = 0;

        auto fSuccess = ReadFile(
            m_namedPipe,
            buffer.get(),
            named_pipe_client::PIPE_BUFFER_SIZE,
            &bytesRead,
            nullptr);
    
        if (!success) {
            auto errorCode = GetLastError();
            named_pipe_context->report_error(errorCode, build_error_msg(errorCode, "ReadFile"));
        }
        buffer[bytesRead] = '\0';

        std::istringstream response_stream(reinterpret_cast<char*>(buffer.get()));
        response_stream.imbue(std::locale::classic());

        http_response& response = named_pipe_context->m_response;
        response.headers().clear();

        // Status code and reason phrase.
        std::string http_version;
        response_stream >> http_version;
        status_code status_code;
        response_stream >> status_code;

        std::string status_message;
        std::getline(response_stream, status_message);

        response.set_status_code(status_code);

        ::web::http::details::trim_whitespace(status_message);
        response.set_reason_phrase(utility::conversions::to_string_t(std::move(status_message)));

        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            named_pipe_context->report_error(0, build_error_msg(0, "Invalid HTTP status line"));
        }

        // Headers
        std::string header;
        while (std::getline(response_stream, header) && header != "\r")
        {
            const auto colon = header.find(':');
            if (colon != std::string::npos)
            {
                auto name = header.substr(0, colon);
                auto value = header.substr(colon + 1, header.size() - colon - 2);
                ::web::http::details::trim_whitespace(name);
                ::web::http::details::trim_whitespace(value);

                if (boost::iequals(name, header_names::transfer_encoding))
                {
                    // TODO: Add support for chunked responses
                }

                if (boost::iequals(name, header_names::connection))
                {
                    // TODO: Add support for Connection header?
                }

                response.headers().add(utility::conversions::to_string_t(std::move(name)), utility::conversions::to_string_t(std::move(value)));
            }
        }

        named_pipe_context->complete_headers();

        auto bodySize = response_stream.rdbuf()->in_avail();

        std::string tmp(std::istreambuf_iterator<char>(response_stream), {});

        auto request_body_buf = named_pipe_context->_get_writebuffer();
        auto body = request_body_buf.alloc(bodySize);
        memcpy(body, tmp.c_str(), bodySize);

        request_body_buf.commit(bodySize);

        named_pipe_context->complete_request(bodySize);
    }

    HANDLE m_namedPipe;
};

std::shared_ptr<_http_client_communicator> create_named_pipe_client(uri&& base_uri, http_client_config&& client_config)
{
    return std::make_shared<details::named_pipe_client>(std::move(base_uri), std::move(client_config));
}

}}}}

