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
* pplx.h
*
* Parallel Patterns Library
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _PPLX_H
#define _PPLX_H

#if (defined(_MSC_VER) && (_MSC_VER >= 1800)) 
#error This file must not be included for Visual Studio 12 or later
#endif

#ifndef _MS_WINDOWS
#if defined(WIN32) || defined(__cplusplus_winrt)
#define _MS_WINDOWS
#endif
#endif // _MS_WINDOWS

#ifdef _PPLX_EXPORT
#define _PPLXIMP __declspec(dllexport)
#else
#define _PPLXIMP __declspec(dllimport)
#endif

// Use PPLx
#ifdef _MS_WINDOWS
#include <windows_compat.h>
#include "pplxwin.h"
#else
#include <linux_compat.h>
#include "pplxlinux.h"
#endif // _MS_WINDOWS

// Common implementation across all the non-concrt versions
#include "pplxcancellation_token.h"
#include <functional>

// conditional expression is constant
#pragma warning(disable: 4127)

#pragma pack(push,_CRT_PACKING)

/// <summary>
///     The <c>pplx</c> namespace provides classes and functions that give you access to the Concurrency Runtime,
///     a concurrent programming framework for C++. For more information, see <see cref="Concurrency Runtime"/>.
/// </summary>
/**/
namespace pplx
{

/// <summary>
/// Sets the ambient scheduler to be used by the PPL constructs.
/// </summary>
_PPLXIMP void __cdecl set_ambient_scheduler(std::shared_ptr<pplx::scheduler_interface> _Scheduler);

/// <summary>
/// Gets the ambient scheduler to be used by the PPL constructs
/// </summary>
_PPLXIMP std::shared_ptr<pplx::scheduler_interface> __cdecl get_ambient_scheduler();

/// <summary>
///     This class describes an exception thrown by the PPL tasks layer in order to force the current task
///     to cancel. It is also thrown by the <c>get()</c> method on <see cref="task Class">task</see>, for a
///     canceled task.
/// </summary>
/// <seealso cref="task::get Method"/>
/// <seealso cref="cancel_current_task Method"/>
/**/
class task_canceled : public std::exception
{
private:
    std::string _message;

public:
    /// <summary>
    ///     Constructs a <c>task_canceled</c> object.
    /// </summary>
    /// <param name="_Message">
    ///     A descriptive message of the error.
    /// </param>
    /**/
    explicit task_canceled(_In_z_ const char * _Message) throw()
        : _message(_Message)
    {
    }

    /// <summary>
    ///     Constructs a <c>task_canceled</c> object.
    /// </summary>
    /**/
    task_canceled() throw()
        : exception()
    {
    }

    ~task_canceled() throw () {}

    const char* what() const _noexcept
    {
        return _message.c_str();
    }
};

/// <summary>
///     This class describes an exception thrown when an invalid operation is performed that is not more accurately
///     described by another exception type thrown by the Concurrency Runtime.
/// </summary>
/// <remarks>
///     The various methods which throw this exception will generally document under what circumstances they will throw it.
/// </remarks>
/**/
class invalid_operation : public std::exception
{
private:
    std::string _message;

public:
    /// <summary>
    ///     Constructs an <c>invalid_operation</c> object.
    /// </summary>
    /// <param name="_Message">
    ///     A descriptive message of the error.
    /// </param>
    /**/
    invalid_operation(_In_z_ const char * _Message) throw()
        : _message(_Message)
    {
    }

    /// <summary>
    ///     Constructs an <c>invalid_operation</c> object.
    /// </summary>
    /**/
    invalid_operation() throw()
        : exception()
    {
    }
    
    ~invalid_operation() throw () {}

    const char* what() const _noexcept
    {
        return _message.c_str();
    }
};


namespace details
{
    //
    // An internal exception that is used for cancellation. Users do not "see" this exception except through the
    // resulting stack unwind. This exception should never be intercepted by user code. It is intended
    // for use by the runtime only.
    //
    class _Interruption_exception : public std::exception
    {
    public:
        _Interruption_exception(){}
    };

    template<typename _T>
    struct _AutoDeleter
    {
        _AutoDeleter(_T *_PPtr) : _Ptr(_PPtr) {}
        ~_AutoDeleter () { delete _Ptr; } 
        _T *_Ptr;
    };

    struct _TaskProcHandle
    {
        _TaskProcHandle()
        {
        }

        virtual ~_TaskProcHandle() {}
        virtual void invoke() const = 0;

        static void __cdecl _RunChoreBridge(void * _Parameter)
        {
            auto _PTaskHandle = static_cast<_TaskProcHandle *>(_Parameter);
            _AutoDeleter<_TaskProcHandle> _AutoDeleter(_PTaskHandle);
            _PTaskHandle->invoke();
        }
    };

    enum _TaskInliningMode
    {
        // Disable inline scheduling
        _NoInline = 0,
        // Let runtime decide whether to do inline scheduling or not
        _DefaultAutoInline = 16,
        // Always do inline scheduling
        _ForceInline = -1,
    };

    // This is an abstraction that is built on top of the scheduler to provide these additional functionalities
    // - Ability to wait on a work item
    // - Ability to cancel a work item
    // - Ability to inline work on invocation of RunAndWait
    class _TaskCollectionImpl 
    {
    public:

        typedef _TaskProcHandle _TaskProcHandle_t;

        _TaskCollectionImpl(scheduler_ptr _PScheduler) 
            : _M_pScheduler(_PScheduler)
        {
        }

        void _ScheduleTask(_TaskProcHandle_t* _PTaskHandle, _TaskInliningMode _InliningMode)
        {
            if (_InliningMode == _ForceInline)
            {
                _TaskProcHandle_t::_RunChoreBridge(_PTaskHandle);
            }
            else
            {
                _M_pScheduler->schedule(_TaskProcHandle_t::_RunChoreBridge, _PTaskHandle);
            }
        }

        void _Cancel()
        {
            // No cancellation support
        }

        void _RunAndWait()
        {
            // No inlining support yet
            _Wait();
        }

        void _Wait()
        {
            _M_Completed.wait();
        }

        void _Complete()
        {
            _M_Completed.set();
        }

        scheduler_ptr _GetScheduler() const
        {
            return _M_pScheduler;
        }

        // Fire and forget
        static void _RunTask(TaskProc_t _Proc, void * _Parameter, _TaskInliningMode _InliningMode)
        {
            if (_InliningMode == _ForceInline)
            {
                _Proc(_Parameter);
            }
            else
            {
                // Schedule the work on the ambient scheduler
                get_ambient_scheduler()->schedule(_Proc, _Parameter);
            }
        }

        static bool __cdecl _Is_cancellation_requested()
        {
            // We do not yet have the ability to determine the current task. So return false always
            return false;
        }
    private:

        extensibility::event_t _M_Completed;
        scheduler_ptr _M_pScheduler;
    };

    // For create_async lambdas that return a (non-task) result, we oversubscriber the current task for the duration of the
    // lambda.
    struct _Task_generator_oversubscriber  {};

    typedef _TaskCollectionImpl _TaskCollection_t;
    typedef _TaskInliningMode _TaskInliningMode_t;
    typedef _Task_generator_oversubscriber _Task_generator_oversubscriber_t;

} // namespace details

} // namespace pplx

#pragma pack(pop)

#endif // _PPLX_H
