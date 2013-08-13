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
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* http_listen.h
*
* HTTP Library: HTTP listener (server-side) APIs
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
#error "Error: http server APIs are not supported in XP"
#endif //_WIN32_WINNT < _WIN32_WINNT_VISTA

#include <limits>
#include <functional>

#include "cpprest/http_msg.h"

namespace web { 

/// <summary>
/// Declaration to avoid making a dependency on IISHost.
/// </summary>
namespace iis_host
{
    class http_iis_receiver;
}

namespace http
{
namespace experimental {
namespace listener
{

/// <summary>
/// A class for listening and processing HTTP requests at a specific URI.
/// </summary>
class http_listener
{
public:
    /// <summary>
    /// Create a listener from a URI.
    /// </summary>
    /// <remarks>The listener will not have been opened when returned.</remarks>
    /// <param name="uri">URI at which the listener should accept requests.</param>
    /// <returns>An unopened http_listener.</returns>
    _ASYNCRTIMP http_listener(const http::uri &address);

    /// <summary>
    /// Default constructor.
    /// </summary>
    /// <remarks>The resulting listener cannot be used for anything, but is useful to initialize a variable
    /// that will later be overwritten with a real listener instance.</remarks>
    http_listener() : m_closed(true) { }

    /// <summary>
    /// Destructor frees any held resources.
    /// </summary>
    /// <remarks>Call close() before allowing a listener to be destroyed.</remarks>
    _ASYNCRTIMP virtual ~http_listener();

    /// <summary>
    /// Open the listener, i.e. start accepting requests.
    /// </summary>
    _ASYNCRTIMP pplx::task<void> open();

    /// <summary>
    /// Stop accepting requests and close all connections.
    /// </summary>
    /// <remarks>
    /// This function will stop accepting requests and wait for all outstanding handler calls
    /// to finish before completing the task. Calling close() from within a handler and blocking
    /// waiting for its result will result in a deadlock.
    ///
    /// Call close() before allowing a listener to be destroyed.
    /// </remarks>
    _ASYNCRTIMP pplx::task<void> close();

    /// <summary>
    /// Add a general handler to support all requests.
    /// </summary>
    /// <param name="handler">Funtion object to be called for all requests.</param>
    /// <returns>A reference to this http_listener to enable chaining.</returns>
    void support(std::function<void(http_request)> handler)  
    {
        m_all_requests = handler;
    }

    /// <summary>
    /// Add support for a specific HTTP method.
    /// </summary>
    /// <param name="method">An HTTP method.</param>
    /// <param name="handler">Funtion object to be called for all requests for the given HTTP method.</param>
    /// <returns>A reference to this http_listener to enable chaining.</returns>
    void support(const http::method &method, std::function<void(http_request)> handler)
    {
        m_supported_methods[method] = handler;
    }

    /// <summary>
    /// Adds an HTTP pipeline stage to the listener.
    /// </summary>
    /// <param name="handler">A function object representing the pipeline stage.</param>
    void add_handler(std::function<pplx::task<http::http_response>(http::http_request, std::shared_ptr<http::http_pipeline_stage>)> handler)
    {
        m_pipeline->append(std::make_shared<http::details::function_pipeline_wrapper>(handler));
    }

    /// <summary>
    /// Adds an HTTP pipeline stage to the listener.
    /// </summary>
    /// <param name="stage">A shared pointer to a pipeline stage.</param>
    void add_handler(std::shared_ptr<http::http_pipeline_stage> stage)
    {
        m_pipeline->append(stage);
    }

    /// <summary>
    /// Get the URI of the listener.
    /// </summary>
    /// <returns>The URI this listener is for.</returns>
    http::uri uri() const { return m_uri; }

    /// Move constructor.
    /// </summary>
    http_listener(http_listener &&other) : m_closed(true)
    {
        _move(std::move(other));
    }

    /// <summary>
    /// Move assignment operator.
    /// </summary>
    http_listener &operator=(http_listener &&other)
    {
        _move(std::move(other));
        return *this;
    }

    /// <summary>
    /// Handler for all requests. The HTTP host uses this to dispatch a message to the pipeline.
    /// </summary>
    /// <remarks>Only HTTP server implementations should call this API.</remarks>
    _ASYNCRTIMP pplx::task<http::http_response> handle_request(http::http_request msg);

private:
    // No copying of listeners.
    http_listener(const http_listener &other);
    http_listener &operator=(const http_listener &other);

    _ASYNCRTIMP pplx::task<http::http_response> dispatch_request(http::http_request msg);

    class _listener_stage : public http::http_pipeline_stage
    {
    public:
        _listener_stage(_In_ http_listener *listener) : m_listener(listener)
        {
        }

        virtual pplx::task<http::http_response> propagate(http::http_request request)
        {
            return m_listener->dispatch_request(request);
        }

        void reset_listener(_In_ http_listener *listener)
        {
            m_listener = listener;
        }

    private:

        http_listener *m_listener;
    };

    void _move(http_listener &&other)
    {
        if(this != &other)
        {
            // unregister this from the server
            this->close().wait();

            this->m_uri = std::move(other.m_uri);
            this->m_all_requests = std::move(other.m_all_requests);
            this->m_supported_methods = std::move(other.m_supported_methods);
            this->m_pipeline = std::move(other.m_pipeline);

            std::shared_ptr< http::http_pipeline_stage> lastStage = this->m_pipeline->last_stage();
            auto listenerStage = static_cast<_listener_stage *>(lastStage.get());
            listenerStage->reset_listener(this);

            if (!other.m_closed)
            {
                // unregister the listener being moved from the server
                // as the registration is by pointer!
                other.close().wait();

                // Register with the server
                this->open().wait();
            }
        }
    }

    // Default implementation for TRACE and OPTIONS.
    void handle_trace(http::http_request message);
    void handle_options(http::http_request message);

    /// <summary>
    /// Gets a comma seperated string containing the methods supported by this listener.
    /// </summary>
    utility::string_t get_supported_methods() const;

    // Http pipeline stages
    std::shared_ptr<http::http_pipeline> m_pipeline;

    // URI 
    http::uri m_uri;

    // Handlers
    std::function<void(http::http_request)> m_all_requests;
    std::map<http::method, std::function<void(http::http_request)>> m_supported_methods;

    // Used to record that the listener has already been closed.
    bool m_closed;
};

} // namespace listener
} // namespace experimental
} // namespace http
} // namespace web