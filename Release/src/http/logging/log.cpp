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
* log.cpp
*
* C++ Actors Library: Message logging
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/log.h"
#include "cpprest/filelog.h"
#include "cpprest/asyncrt_utils.h"

#ifndef _MS_WINDOWS
#include <sys/stat.h> // for mkdir
#include "boost/locale.hpp"
#endif

using namespace web; 
using namespace utility;
using namespace utility::experimental;
using namespace utility::experimental::logging;
using namespace pplx;

std::shared_ptr< logging::_Log> s_default;

void log::post(log_level level, const utility::string_t &message)
{
    auto lg = get_default();
    if ( lg ) lg->post(level, message);
}

void log::post(log_level level, int code, const utility::string_t &message)
{
    auto log = get_default();
    if ( log ) log->post(level, code, message);
}

void log::post(log_level level, int code, const utility::string_t &message, http::http_request http_msg)
{
    auto log = get_default();
    if ( log ) log->post(level, code, message, http_msg);
}

volatile unsigned long s_lock = 0;

void log::set_default(_Inout_opt_ logging::_Log *log)
{
    if ( log == nullptr )
        log = new logging::SilentLog;
    s_default = std::shared_ptr< logging::_Log>(log); 
}

void log::set_default(std::shared_ptr< logging::_Log> log)
{
    if ( !log )
        log = std::make_shared<logging::SilentLog>();
    s_default = log; 
}

std::shared_ptr< logging::_Log> log::get_default()
{ 
    if ( !s_default )
    {
        s_default = std::shared_ptr< logging::_Log>(new logging::ConsoleLog(true, true));
    }
    return s_default; 
}

#ifdef _MS_WINDOWS
int log::report_error(logging::log_level level, const int error_code, const utility::string_t &error_message)
{
    utility::string_t msg(error_message);
    msg.append(U(": "));
    msg.append(utility::details::create_error_message(error_code));
    logging::log::post(level, error_code, msg);
    return error_code;
}
int log::report_error(logging::log_level level, const utility::string_t &error_message)
{
    return report_error(level, GetLastError(), error_message);
}
#endif

void ::logging::_Log::post(log_level level, int code, const utility::string_t &message, http::http_request )
{
    post(level, code, message);
}

logging::ConsoleLog::ConsoleLog(bool suppress_date, bool suppress_time) :
    m_noDate(suppress_date), 
    m_noTime(suppress_time), 
    m_processing_flag(0L)
{
}

logging::LocalFileLog::LocalFileLog(const utility::string_t &path, bool suppress_date, bool suppress_time) : 
    m_noDate(suppress_date), 
    m_noTime(suppress_time), 
    m_processing_flag(0L),
    m_path(path)
{
    this->set_synchronous(false);
#if defined(__cplusplus_winrt)
    _ASSERT(false);
#elif defined(_MS_WINDOWS)
    CreateDirectoryW(path.c_str(), nullptr);
#else
    mkdir(m_path.c_str(), 0777);
#endif
}

#define VERIFY_WRITE(fh, source, expected) { DWORD bytesWritten = 0; if ( !WriteFile(fh, source, (DWORD)expected, &bytesWritten, NULL) || (bytesWritten < (DWORD)expected)) { CloseHandle(fh); return; } }

const char *LevelText(log_level level)
{
    switch(level)
    {
    case LOG_INFO: return "Information";
    case LOG_WARNING: return "Warning";
    case LOG_FATAL: return "Fatal Error";
    case LOG_ERROR: return "Error";
    }
    return "LOG ERROR";
}

#ifdef _MS_WINDOWS
bool _FormatDate(const SYSTEMTIME& st, utility::ostream_t &outputStream)
{
#if _WIN32_WINNT < _WIN32_WINNT_VISTA 
    int nDateStringLength = GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, NULL, 0);
    if(nDateStringLength == 0)
    {
        // error condition
        return false;
    }

    char * pszDate = new char[nDateStringLength];
    if(GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, pszDate, nDateStringLength) == 0)
    {
        // error condition
        return false;
    }  
#else
    int nDateStringLength = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, NULL, NULL, 0, NULL);
    if(nDateStringLength == 0)
    {
        // error condition
        return false;
    }

    wchar_t * pszDate = new wchar_t[nDateStringLength];
    if(GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, NULL, pszDate, nDateStringLength, NULL) == 0)
    {
        // error condition
        return false;
    }  
#endif // _WIN32_WINNT < _WIN32_WINNT_VISTA 
    
    // Append date to the output stream and free resources
    outputStream << pszDate;
    delete[] pszDate;

    return true;
}

bool _FormatTime(const SYSTEMTIME& st, utility::ostream_t& outputStream)
{
#if _WIN32_WINNT < _WIN32_WINNT_VISTA 
    int nTimeStringLength = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, NULL, 0);
    if(nTimeStringLength == 0)
    {
        // error condition
        return false;
    }

    char * pszTime = new char[nTimeStringLength];
    if(GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, pszTime, nTimeStringLength) == 0)
    {
        // error condition
        return false;
    }
#else
    int nTimeStringLength = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, NULL, NULL, 0);
    if(nTimeStringLength == 0)
    {
        // error condition
        return false;
    }

    wchar_t * pszTime = new wchar_t[nTimeStringLength];
    if(GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, NULL, pszTime, nTimeStringLength) == 0)
    {
        // error condition
        return false;
    }  
#endif // _WIN32_WINNT < _WIN32_WINNT_VISTA 

    // Append time to the output stream and free resources
    outputStream << " " << pszTime;
    delete[] pszTime;

    // Assign the output string

    return true;
}
#else
bool _FormatDate(const time_t& pt, std::ostream &outputStream)
{
    outputStream << boost::locale::as::date << pt;
    return true;
}

bool _FormatTime(const time_t& pt, std::ostream &outputStream)
{
    outputStream << boost::locale::as::time << pt;
    return true;
}
#endif

utility::string_t _GetPath(const utility::string_t &base)
{
#ifdef _MS_WINDOWS
    SYSTEMTIME st;
    GetLocalTime(&st);

    utility::ostringstream_t outputStream;

    outputStream << base << U("\\");
    outputStream << st.wYear << U("-");
    if ( st.wMonth < 10 ) 
        outputStream << U("0");
    outputStream << st.wMonth << U("-");
    if ( st.wDay < 10 ) 
        outputStream << U("0");
    outputStream << st.wDay << U(".log");

    return outputStream.str();
#else
    time_t now = time(nullptr);
    std::ostringstream outputStream;
    outputStream << base << boost::locale::as::ftime("\\%Y-%m-%d.log") << now;
    return outputStream.str();
#endif
}

void logging::SilentLog::post(log_level, const utility::string_t &)
{
    // Do nothing.
}

void logging::SilentLog::post(log_level, int, const utility::string_t &)
{
    // Do nothing.
}

void logging::ConsoleLog::_internal_post(_In_ _LogEntry *entry)
{
    int stamp = 0;

    utility::ostream_t& stream = (entry->level > LOG_WARNING) ?
#ifdef _MS_WINDOWS
        std::wcerr : std::wcout;
#else
        std::cerr : std::cout;
#endif

    if ( !m_noDate && _FormatDate(entry->time, stream) ) stamp++;
    if ( !m_noTime && _FormatTime(entry->time, stream) ) stamp++;

    if( stamp > 0 )
    {
        stream << ": ";
    }
    if ( entry->code > -1 )
    {
        if ( entry->level > LOG_INFO )
            stream << LevelText(entry->level) << " " << entry->code << ": " << entry->message << "\n" << std::flush;
        else
            stream << entry->code << ": " << entry->message << "\n" << std::flush;
    }
    else
    {
        if ( entry->level > LOG_INFO )
            stream << LevelText(entry->level) << ": " << entry->message << "\n" << std::flush;
        else
            stream << entry->message << "\n" << std::flush;
    }

    delete entry;
}

void logging::ConsoleLog::post(log_level level, const utility::string_t &message)
{
    if ( level >= m_level )
    {       
        // We run the actual I/O on a separate thread, because there is no
        // point in waiting for it to complete before returning.

        auto entry = new _LogEntry(level, -1, message);

        if ( m_sync ) 
        {
            _internal_post(entry);
            return;
        }

        m_lock.lock();
        
        m_queue.push(entry);

        if ( pplx::details::atomic_exchange(m_processing_flag, 1L) == 0 )
        {
            m_lock.unlock();

            m_tasks.run([=]()
            {
                m_lock.lock();   

                while ( m_queue.size() > 0 )
                {
                    _LogEntry *entry = m_queue.front();
                    m_queue.pop();

                    m_lock.unlock();
                    _internal_post(entry);
                    m_lock.lock();
                }

                pplx::details::atomic_exchange(m_processing_flag, 0L);
            
                m_lock.unlock();
            });
        }
        else
        {
            m_lock.unlock();
        }
    }
}

void logging::ConsoleLog::post(log_level level, int code, const utility::string_t &message)
{
    if ( level >= m_level )
    {       
        // We run the actual I/O on a separate thread, because there is no
        // point in waiting for it to complete before returning.

        auto entry = new _LogEntry(level, code, message);

        if ( m_sync ) 
        {
            _internal_post(entry);
            return;
        }

        m_lock.lock();
        
        m_queue.push(entry);

        if ( pplx::details::atomic_exchange(m_processing_flag, 1L) == 0 )
        {
            m_lock.unlock();

            m_tasks.run([=]()
            {
                m_lock.lock();   

                while ( m_queue.size() > 0 )
                {
                    _LogEntry *entry = m_queue.front();
                    m_queue.pop();

                    m_lock.unlock();
                    _internal_post(entry);
                    m_lock.lock();
                }

                pplx::details::atomic_exchange(m_processing_flag, 0L);
            
                m_lock.unlock();
            });
        }
        else
        {
            m_lock.unlock();
        }
    }
}

void logging::LocalFileLog::_internal_post(utility::ofstream_t &stream, _In_ _LogEntry *entry)
{
    int stamp = 0;

    if ( !m_noDate && _FormatDate(entry->time, stream) ) stamp++;
    if ( !m_noTime && _FormatTime(entry->time, stream) ) stamp++;

    if( stamp > 0 )
    {
        stream << ": ";
    }

    if ( entry->code > -1 )
        stream << LevelText(entry->level) << U(" ") << entry->code << U(": ") << entry->message << U("\n") << std::flush;
    else
        stream << LevelText(entry->level) << U(": ") << entry->message << U("\n") << std::flush;

    delete entry;
}

void logging::LocalFileLog::post(log_level level, const utility::string_t &message)
{
    if ( level >= m_level )
    {       
        // We run the actual I/O on a separate thread, because there is no
        // point in waiting for it to complete before returning.

        auto entry = new _LogEntry(level, -1, message);

        if ( m_sync ) 
        {
            utility::ofstream_t stream;
            stream.open(_GetPath(m_path).c_str(), std::ios::app | std::ios::out);
            _internal_post(stream, entry);
            stream.close();
            return;
        }

        m_lock.lock();
        
        m_queue.push(entry);

        if ( pplx::details::atomic_exchange(m_processing_flag, 1L) == 0 )
        {
            m_lock.unlock();

            m_tasks.run([=]()
            {
                utility::ofstream_t stream;
                stream.open(_GetPath(m_path).c_str(), std::ios::app | std::ios::out);
                
                m_lock.lock();   

                while ( m_queue.size() > 0 )
                {
                    _LogEntry *entry = m_queue.front();
                    m_queue.pop();

                    m_lock.unlock();
                    _internal_post(stream, entry);
                    m_lock.lock();
                }

                stream.close();

                pplx::details::atomic_exchange(m_processing_flag, 0L);
            
                m_lock.unlock();
            });
        }
        else
        {
            m_lock.unlock();
        }
    }
}

void logging::LocalFileLog::post(log_level level, int code, const utility::string_t &message)
{
    if ( level >= m_level )
    {       
        // We run the actual I/O on a separate thread, because there is no
        // point in waiting for it to complete before returning.

        auto entry = new _LogEntry(level, code, message);

        if ( m_sync ) 
        {
            utility::ofstream_t stream;
            stream.open(_GetPath(m_path).c_str(), std::ios::app | std::ios::out);
            _internal_post(stream, entry);
            stream.close();
            return;
        }

        m_lock.lock();
        
        m_queue.push(entry);

        if ( pplx::details::atomic_exchange(m_processing_flag, 1L) == 0 )
        {
            m_lock.unlock();

            m_tasks.run([=]()
            {
                utility::ofstream_t stream;
                stream.open(_GetPath(m_path).c_str(), std::ios::app | std::ios::out);
                
                m_lock.lock();   

                while ( m_queue.size() > 0 )
                {
                    _LogEntry *entry = m_queue.front();
                    m_queue.pop();

                    m_lock.unlock();
                    _internal_post(stream, entry);
                    m_lock.lock();
                }

                stream.close();

                pplx::details::atomic_exchange(m_processing_flag, 0L);

                m_lock.unlock();
            });
        }
        else
        {
            m_lock.unlock();
        }
    }
}
