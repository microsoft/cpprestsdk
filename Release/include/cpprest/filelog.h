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
* filelog.h
*
* Basic logging implementations.
* 
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "cpprest/basic_types.h"

#include <queue>
#ifdef _MS_WINDOWS
#else
#include "boost/date_time/posix_time/posix_time_types.hpp"
#endif

#include "cpprest/log.h"

namespace utility { namespace experimental { 
namespace logging
{
        /// <summary>
        /// Silently swallow all messages.
        /// </summary>
        class SilentLog : public _Log
        {
        public:
            /// <summary>
            /// Log a simple message at a certain level of significance.
            /// </summary>
            /// <param name="level">The message's signficance level</param>
            /// <param name="message">The text of the message</param>
            /// <remarks>The code associated with the message will be 0</remarks>
            _ASYNCRTIMP virtual void post(log_level level, const utility::string_t &message);

            /// <summary>
            /// Log a simple message at a certain level of significance.
            /// </summary>
            /// <param name="level">The message's signficance level</param>
            /// <param name="code">An application-specific integer code associated with the message</param>
            /// <param name="message">The text of the message</param>
            _ASYNCRTIMP virtual void post(log_level level, int code, const utility::string_t &message);
        };

        /// <summary>
        /// Log messages to stderr.
        /// </summary>
        class ConsoleLog : public _Log
        {
        public:
            /// <summary>
            /// Create a console log object.
            /// </summary>
            /// <param name="suppress_date">Suppress printing the date when logging messages</param>
            /// <param name="suppress_time">Suppress printing the time when logging messages</param>
            _ASYNCRTIMP ConsoleLog(bool suppress_date = false, bool suppress_time = false);

            /// <summary>
            /// Destructor
            /// </summary>
            virtual ~ConsoleLog()
            {
            }

            /// <summary>
            /// Log a simple message at a certain level of significance.
            /// </summary>
            /// <param name="level">The message's signficance level</param>
            /// <param name="message">The text of the message</param>
            /// <remarks>The code associated with the message will be 0</remarks>
            _ASYNCRTIMP virtual void post(log_level level, const utility::string_t &message);

            /// <summary>
            /// Log a simple message at a certain level of significance.
            /// </summary>
            /// <param name="level">The message's signficance level</param>
            /// <param name="code">An application-specific integer code associated with the message</param>
            /// <param name="message">The text of the message</param>
            _ASYNCRTIMP virtual void post(log_level level, int code, const utility::string_t &message);

        private:

            struct _LogEntry
            {
                _LogEntry(log_level level, int code, const utility::string_t &message)
                    : level(level), code(code), message(message)
#ifdef _MS_WINDOWS
                {
                    GetLocalTime(&time);
                }
#else
                    , time(::time(nullptr)) {}
#endif

#ifdef _MS_WINDOWS
                SYSTEMTIME time;
#else
                time_t time;
#endif

                log_level level; 
                int code; 
                utility::string_t message; 
            };

            void _internal_post(_In_ _LogEntry *);

            bool   m_noDate;
            bool   m_noTime;

            std::string m_path;

            pplx::details::atomic_long m_processing_flag;

            std::queue<_LogEntry *> m_queue;

            pplx::extensibility::critical_section_t m_lock;
        };

        /// <summary>
        /// Log messages to a file folder.
        /// </summary>
        class LocalFileLog : public _Log
        {
        public:
            /// <summary>
            /// Create a local file log object. Logs files are placed into a folder identified
            /// by the 'path' argument and given a standard name.
            /// </summary>
            /// <param name="path">The path of the folder to use for logging</param>
            /// <param name="suppress_date">Suppress printing the date when logging messages</param>
            /// <param name="suppress_time">Suppress printing the time when logging messages</param>
            _ASYNCRTIMP LocalFileLog(const utility::string_t &path, bool suppress_date = false, bool suppress_time = false);

            /// <summary>
            /// Destructor
            /// </summary>
            virtual ~LocalFileLog()
            {
            }

            /// <summary>
            /// Log a simple message at a certain level of significance.
            /// </summary>
            /// <param name="level">The message's signficance level</param>
            /// <param name="message">The text of the message</param>
            /// <remarks>The code associated with the message will be 0</remarks>
            _ASYNCRTIMP virtual void post(log_level level, const utility::string_t &message);

            /// <summary>
            /// Log a simple message at a certain level of significance.
            /// </summary>
            /// <param name="level">The message's signficance level</param>
            /// <param name="code">An application-specific integer code associated with the message</param>
            /// <param name="message">The text of the message</param>
            _ASYNCRTIMP virtual void post(log_level level, int code, const utility::string_t &message);

        private:

            struct _LogEntry
            {
                _LogEntry(log_level level, int code, const utility::string_t &message)
                    : level(level), code(code), message(message)
#ifdef _MS_WINDOWS
                {
                    GetLocalTime(&time);
                }
#else
                    , time(::time(nullptr)) {}
#endif

#ifdef _MS_WINDOWS
                SYSTEMTIME time;
#else
                time_t time;
#endif

                log_level level; 
                int code; 
                utility::string_t message; 
            };

            void _internal_post(utility::ofstream_t &stream, _In_ _LogEntry *);

            bool   m_noDate;
            bool   m_noTime;

            utility::string_t m_path;

           pplx::details::atomic_long m_processing_flag;

            std::queue<_LogEntry *> m_queue;

            pplx::extensibility::critical_section_t m_lock;
        };
    }
}}
