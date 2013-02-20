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

#ifndef _CASA_HTTP_LISTENER_H
#define _CASA_HTTP_LISTENER_H

#include <limits>
#include <functional>

#include "http_msg.h"

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
namespace listener
{
namespace details {

class _function_pipeline_wrapper : public http::http_pipeline_stage
{
public:
    _function_pipeline_wrapper(std::function<pplx::task<http_response>(http_request, std::shared_ptr<http::http_pipeline_stage>)> handler) : m_handler(handler)
    {
    }

    virtual pplx::task<http_response> propagate(http_request request)
    {
        return m_handler(request, get_next_stage());
    }
private:

    std::function<pplx::task<http_response>(http_request, std::shared_ptr<http::http_pipeline_stage>)> m_handler;
};

} // namespace details

/// <summary>
/// Interface a HTTP listener must implement to work with the http_server_api.
/// </summary>
class http_listener_interface
{
public:

    /// <summary>
    /// Get the URI of the listener.
    /// </summary>
    /// <returns>The URI this listener is for.</returns>
    virtual http::uri uri() const = 0;

    /// <summary>
    /// Handler for all requests.
    /// </summary>
    virtual pplx::task<http_response> handle_request(http_request msg) = 0;
};

/// <summary>
/// A class for listening and processing HTTP requests at a specific URI.
/// </summary>
class http_listener : public http_listener_interface, public http_pipeline
{
public:

    /// <summary>
    /// Create a listener from a URI.
    /// </summary>
    /// <remarks>The listener will not have been opened when returned.</remarks>
    /// <param name="uri">URI at which the listener should accept requests.</param>
    /// <returns>An unopened http_listener.</returns>
    static http_listener create(const http::uri &uri) { return http_listener(uri); }

    /// <summary>
    /// Create an HTTP listener given a functor (lambda) to handle all GET requests.
    /// </summary>
    /// <remarks>The listener will not have been opened when returned.</remarks>
    /// <param name="uri">URI at which the listener should accept requests.</param>
    /// <param name="get">Handler to accept all incoming GET requests.</param>
    /// <returns>An unopened http_listener.</returns>
    template<typename FunctorGet>
    static http_listener create(const http::uri &uri, FunctorGet get)
    {
        http_listener listener(uri);
        listener.support(methods::GET, std::move(get));
        return listener;
    }

    /// <summary>
    /// Create an HTTP listener given functors to handle all GET and PUT requests.
    /// </summary>
    /// <remarks>The listener will not have been opened when returned.</remarks>
    /// <param name="uri">URI at which the listener should accept requests.</param>
    /// <param name="get">Handler to accept all incoming GET requests.</param>
    /// <param name="put">Handler to accept all incoming PUT requests.</param>
    /// <returns>An unopened http_listener.</returns>
    template<typename FunctorGet, typename FunctorPut>
    static http_listener create(const http::uri &uri, FunctorGet get, FunctorPut put)
    {
        http_listener listener(uri);
        listener.support(methods::GET, std::move(get)).support(methods::PUT, std::move(put));
        return listener;
    }

    /// <summary>
    /// Create an HTTP listener given functors to handle all GET, PUT, and POST requests.
    /// </summary>
    /// <remarks>The listener will not have been opened when returned.</remarks>
    /// <param name="uri">URI at which the listener should accept requests.</param>
    /// <param name="get">Handler to accept all incoming GET requests.</param>
    /// <param name="put">Handler to accept all incoming PUT requests.</param>
    /// <param name="post">Handler to accept all incoming POST requests.</param>
    /// <returns>An unopened http_listener.</returns>
    template<typename FunctorGet, typename FunctorPut, typename FunctorPost>
    static http_listener create(const http::uri &uri, FunctorGet get, FunctorPut put, FunctorPost post)
    {
        http_listener listener(uri);
        listener.support(methods::GET, std::move(get)).support(methods::PUT, std::move(put)).support(methods::POST, std::move(post));
        return listener;
    }

    /// <summary>
    /// Create an HTTP listener given functors to handle all GET, PUT, POST, and DELETE requests.
    /// </summary>
    /// <remarks>The listener will not have been opened when returned.</remarks>
    /// <param name="uri">URI at which the listener should accept requests.</param>
    /// <param name="get">Handler to accept all incoming GET requests.</param>
    /// <param name="put">Handler to accept all incoming PUT requests.</param>
    /// <param name="post">Handler to accept all incoming POST requests.</param>
    /// <param name="delte">Handler to accept all incoming DELETE requests.</param>
    /// <returns>An unopened http_listener.</returns>
    template<typename FunctorGet, typename FunctorPut, typename FunctorPost, typename FunctorDelete>
    static http_listener create(const http::uri &uri, FunctorGet get, FunctorPut put, FunctorPost post, FunctorDelete delte)
    {
        http_listener listener(uri);
        listener.support(methods::GET, std::move(get)).support(methods::PUT, std::move(put)).support(methods::POST, std::move(post)).support(methods::DEL, std::move(delte));
        return listener;
    }

    /// <summary>
    /// Default constructor.
    /// </summary>
    /// <remarks>The resulting listener cannot be used for anything, but is useful to initialize a variable
    /// that will later be overwritten with a real listener instance.</remarks>
    http_listener() : m_pipeline_stage(nullptr), m_closed(true) { }

    /// <summary>
    /// Destructor frees any held resources.
    /// </summary>
    _ASYNCRTIMP virtual ~http_listener();

    /// <summary>
    /// Open the listener, i.e. start accepting requests.
    /// </summary>
    _ASYNCRTIMP unsigned long open();

    /// <summary>
    /// Stop accepting requests and close all connections.
    /// </summary>
    _ASYNCRTIMP unsigned long close();

    /// <summary>
    /// Add a general handler to support all requests.
    /// </summary>
    /// <param name="handler">Funtion object to be called for all requests.</param>
    /// <returns>A reference to this http_listener to enable chaining.</returns>
    template <typename Functor>
    http_listener &support(Functor handler)  
    {
        m_all_requests = std::function<void(http_request)>(handler);
        return *this;
    }

    /// <summary>
    /// Add support for a specific HTTP method.
    /// </summary>
    /// <param name="method">An HTTP method.</param>
    /// <param name="handler">Funtion object to be called for all requests.</param>
    /// <returns>A reference to this http_listener to enable chaining.</returns>
    template <typename Functor>
    http_listener &support(const http::method &method, Functor handler)
    {
        m_supported_methods[method] = std::function<void(http_request)>(handler);
        return *this;
    }

    /// <summary>
    /// Add an HTTP pipeline stage to the client. It will be invoked after any already existing stages.
    /// </summary>
    /// <param name="handler">A function object representing the pipeline stage.</param>
    void add_handler(std::function<pplx::task<http_response>(http_request, std::shared_ptr<http::http_pipeline_stage>)> handler)
    {
        auto hndlr = std::make_shared<details::_function_pipeline_wrapper>(handler);
        append(hndlr);
    }

    /// <summary>
    /// Start listening for for HTTP requests and call the function object.
    /// Stop listening when the the 'until()' functor returns.
    /// </summary>
    template<typename Functor>
    pplx::task<unsigned long> listen(Functor until)
    {
        return pplx::create_task([=]() -> unsigned long
        {
            unsigned long error_code = open();
            if (error_code == 0)
            {
                until();
                return close();
            }
            return error_code;
        });
    }

    /// <summary>
    /// Get the URI of the listener.
    /// </summary>
    /// <returns>The URI this listener is for.</returns>
    http::uri uri() const { return m_uri; }

    /// <summary>
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
    /// Handler for all requests. The HTTP host uses this to dispatching a message to the pipeline.
    /// </summary>
    _ASYNCRTIMP pplx::task<http_response> handle_request(http_request msg);

    /// <summary>
    /// Handler for all requests. This is the last pipeline stage, dispatching messages to 
    /// application handlers, based on the HTTP method.
    /// </summary>
    _ASYNCRTIMP pplx::task<http_response> dispatch_request(http_request msg);

private:

    void _move(http_listener &&other)
    {
        if(this != &other)
        {
            if(!m_closed)
            {
                close();
            }

            this->m_closed = other.m_closed;
            this->m_uri = std::move(other.m_uri);
            this->m_all_requests = std::move(other.m_all_requests);
            this->m_supported_methods = std::move(other.m_supported_methods);

            this->m_pipeline_stage = other.m_pipeline_stage;
            other.m_pipeline_stage = nullptr;
            this->m_pipeline_stage->reset_listener(this);

            other.m_closed = true;

            http_pipeline::operator=(std::move(other));
        }
    }

    class _listener_stage : public http::http_pipeline_stage
    {
    public:
        _listener_stage(_In_ http_listener *listener) : m_listener(listener)
        {
        }

        virtual pplx::task<http_response> propagate(http_request request)
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

    _listener_stage* m_pipeline_stage;

    /// <summary>
    /// Private constructor. Only create_listener functions should access the constructor.
    /// </summary>
    _ASYNCRTIMP http_listener(const http::uri &address);

    http::uri m_uri;
    std::function<void(http_request)> m_all_requests;
    std::map<http::method, std::function<void(http_request)>> m_supported_methods;

    // Default implementation for TRACE and OPTIONS.
    void handle_trace(http_request message);
    void handle_options(http_request message);

    /// <summary>
    /// Gets a comma seperated string containing the methods supported by this listener.
    /// </summary>
    utility::string_t get_supported_methods() const;

    // Used to record that the listener has already been closed.
    bool m_closed;

    // No copy or assignment.
    http_listener(const http_listener &);
    http_listener &operator=(const http_listener &);
};

} // namespace listen
} // namespace http
} // namespace web

#endif  /* _CASA_HTTP_LISTENER_H */