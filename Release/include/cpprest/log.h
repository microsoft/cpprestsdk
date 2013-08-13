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
* log.h
*
* Basic logging interface.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include <string>
#include <vector>

#include "cpprest/xxpublic.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else 
#include "pplx/pplxtasks.h"
#endif

namespace web 
{
// To avoid bringing in http_msg.h.
namespace http
{
    class http_request;
}
}

namespace utility
{
namespace experimental 
{
namespace logging 
{
    /// <summary>
    /// The set of commands that are sent at the low-level communication layer.   
    /// </summary>
    enum log_level { LOG_INFO,      // Informational message (started, shut down, etc.)
                     LOG_WARNING,   // An error that the software recovered from, but could cause trouble later.
                     LOG_ERROR,     // A general error that needs some intervention
                     LOG_FATAL      // An error significant enough to require the system to terminate
    };

    /// <summary>
    /// Base class for all log implementations.
    /// </summary>
    class _Log
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        _Log () : m_level(LOG_INFO), m_sync(true) {}

        /// <summary>
        /// Log a simple message at a certain level of significance.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="message">The text of the message</param>
        /// <remarks>The code associated with the message will be 0</remarks>
        virtual void post(log_level level, const utility::string_t &message) = 0;

        /// <summary>
        /// Log a simple message at a certain level of significance.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="code">An application-specific integer code associated with the message</param>
        /// <param name="message">The text of the message</param>
        virtual void post(log_level level, int code, const utility::string_t &message) = 0;

        /// <summary>
        /// Log a message related to a particular http request.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="code">An application-specific integer code associated with the message</param>
        /// <param name="message">The text of the message</param>
        /// <param name="http_msg">The http message this log message is assocated with.</param>
        /// <remarks>
        /// Not all logger implementations needs to implement this method, it defaults to a regular simple
        /// message. Implementations like IIS logger may choose to override and given pertinent information
        /// related to the http request.
        /// </remarks>        
        virtual _ASYNCRTIMP void post(log_level level, int code, const utility::string_t &message, ::web::http::http_request http_msg);
         
        /// <summary>
        /// Set the current significance level at which messages are propagated. Messages at levels
        /// below the threshold will be ignored in subsequent calls to 'log'
        /// </summary>
        /// <param name="level">The minimal significance level to propagate</param>
        void set_level(log_level level) { m_level = level; }

        /// <summary>
        /// Get the current significance level at which messages are propagated.
        /// </summary>
        /// <returns>The current level setting</returns>
        log_level get_level() const { return m_level; }

        /// <summary>
        /// Set the current synchronous/asynchronous logging policy.
        /// </summary>
        /// <param name="sync">True if logging should be done synchronously, false if asynchronously.</param>
        void set_synchronous(bool sync) { flush(); m_sync = sync;} 

        /// <summary>
        /// Flush outstanding messages.
        /// </summary>
        void flush() 
        {
             if ( !m_sync ) m_tasks.wait();
        } 

    protected:

        log_level m_level;

        bool m_sync;

        class task_vector
        {
        public:

            // Since the wait function doesn't return a value only task<void> is a reasonable type.
            typedef pplx::task<void> _TaskType;

            task_vector() {}

            ~task_vector()
            {
                wait();
            }

            template <class _Ty>
            void run(const _Ty& _Param)
            {
                _TaskType _Task(_Param);
                {
                    pplx::extensibility::scoped_critical_section_t _Lock(_M_lock);
                    _M_tasks.push_back(_Task);
                }
            }

            void wait()
            {
                if (_M_tasks.size() > 0)
                {
                    std::vector<_TaskType> _TaskList;

                    {
                        pplx::extensibility::scoped_critical_section_t _Lock(_M_lock);
                        _TaskList.swap(_M_tasks);
                    }

                    // Wait outside the lock. If there is an exception the wait will short-circuit
                    pplx::when_all(begin(_TaskList), end(_TaskList)).wait();
                }
            }
        private:

            pplx::extensibility::critical_section_t _M_lock;
            std::vector<_TaskType> _M_tasks;
        };

        task_vector m_tasks;

    };

    namespace log 
    {
        /// <summary>
        /// Set the default Log object for the process
        /// </summary>
        /// <param name="log">A pointer to the default Log object</param>
        _ASYNCRTIMP extern void set_default(_Inout_opt_ _Log *log);

        /// <summary>
        /// Set the default Log object for the process
        /// </summary>
        /// <param name="log">A pointer to the default Log object</param>
        _ASYNCRTIMP extern void set_default(std::shared_ptr<_Log> log);

        /// <summary>
        /// Get the default Log object of the process.
        /// </summary>
        /// <returns>A pointer to the default Log object, possibly empty</returns>
        _ASYNCRTIMP extern std::shared_ptr<_Log> get_default();

        /// <summary>
        /// Using the process' default Log object, log a simple message at a certain
        /// level of significance.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="message">The text of the message</param>
        /// <remarks>The code associated with the message will be 0</remarks>
        _ASYNCRTIMP extern void post(log_level level, const utility::string_t &message);

        /// <summary>
        /// Using the process' default Log object, log a simple message at a certain
        /// level of significance.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="code">An application-specific integer code associated with the message</param>
        /// <param name="message">The text of the message</param>
        _ASYNCRTIMP extern void post(log_level level, int code, const utility::string_t &message);

        /// <summary>
        /// Log a message related to a particular http request.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="code">An application-specific integer code associated with the message</param>
        /// <param name="message">The text of the message</param>
        /// <param name="http_msg">The http message this log message is assocated with.</param>
        /// <remarks>
        /// Not all logger implementations needs to implement this method, it defaults to a regular simple
        /// message. Implementations like IIS logger may choose to override and given pertinent information
        /// related to the http request.
        /// </remarks>
        _ASYNCRTIMP extern void post(log_level level, int code, const utility::string_t &message, ::web::http::http_request http_msg);

#ifdef _MS_WINDOWS
        /// <summary>
        /// Log function which gets the windows error message associated with the specified error code and appends
        /// it to the given error message before logging.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="code">An application-specific integer code associated with the message</param>
        /// <param name="message">The text of the message</param>
        /// <returns>The error code parameter passed in.</returns>
        _ASYNCRTIMP int report_error(logging::log_level level, const int error_code, const utility::string_t &error_message);

        /// <summary>
        /// Log function calls GetLastError() and gets the windows error message associated with the specified error 
        /// code and appends it to the given error message before logging.
        /// </summary>
        /// <param name="level">The message's signficance level</param>
        /// <param name="message">The text of the message</param>
        /// <returns>The error code returned from GetLastError()</returns>
        _ASYNCRTIMP int report_error(logging::log_level level, const utility::string_t &error_message);
#endif
    }
}
}
}