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

#include "pplxdefs.h"
#include <functional>

#ifdef _MS_WINDOWS
#include <windows_compat.h>
#include "pplxwin.h"
#else
#include <linux_compat.h>
#include "pplxlinux.h"
#endif

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
_PPLXIMP void __cdecl set_ambient_scheduler(std::shared_ptr< ::pplx::scheduler> _Scheduler);

/// <summary>
/// Gets the ambient scheduler to be used by the PPL constructs
/// </summary>
_PPLXIMP std::shared_ptr< ::pplx::scheduler> __cdecl get_ambient_scheduler();

/// <summary>
///     Describes the execution status of a <c>task_group</c> or <c>structured_task_group</c> object.  A value of this type is returned
///     by numerous methods that wait on tasks scheduled to a task group to complete.
/// </summary>
/// <seealso cref="task_group Class"/>
/// <seealso cref="task_group::wait Method"/>
/// <seealso cref="task_group::run_and_wait Method"/>
/// <seealso cref="structured_task_group Class"/>
/// <seealso cref="structured_task_group::wait Method"/>
/// <seealso cref="structured_task_group::run_and_wait Method"/>
/**/
enum task_group_status
{
    /// <summary>
    ///     The tasks queued to the <c>task_group</c> object have not completed.  Note that this value is not presently returned by
    ///     the Concurrency Runtime.
    /// </summary>
    /**/
    not_complete,

    /// <summary>
    ///     The tasks queued to the <c>task_group</c> or <c>structured_task_group</c> object completed successfully.
    /// </summary>
    /**/
    completed,

    /// <summary>
    ///     The <c>task_group</c> or <c>structured_task_group</c> object was canceled.  One or more tasks may not have executed.
    /// </summary>
    /**/
    canceled
};

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
    class _Spin_wait
    {
    public:
        _Spin_wait()
        {
        }

        bool spin_once()
        {
            ::pplx::platform::YieldExecution();
            return true;
        }
    };

    /// <summary>
    /// Spin lock to allow for locks to be used in global scope
    /// </summary>
    class _Spin_lock
    {
    public:

        _Spin_lock()
            : _M_lock(0)
        {
        }

        void lock()
        {
            if ( atomic_compare_exchange(_M_lock, 1l, 0l) != 0l )
            {
                _Spin_wait spinWait;
                do 
                {
                    spinWait.spin_once();

                } while ( atomic_compare_exchange(_M_lock, 1l, 0l) != 0l );
            }
        }

        void unlock()
        {
            // fence for release semantics
            atomic_exchange(_M_lock, 0l);
        }

    private:
        atomic_long _M_lock;
    };

    typedef ::pplx::scoped_lock<_Spin_lock> _Scoped_spin_lock;

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

    struct _Chore
    {
    protected:
        // Constructors.
        explicit _Chore(TaskProc _PFunction) : m_pFunction(_PFunction)
        {
        }

        _Chore()
        {
        }

        virtual ~_Chore()
        {
        }

    public:

        // The function which invokes the work of the chore.
        TaskProc m_pFunction;
    };

    // Represents possible results of waiting on a task collection.
    enum _TaskCollectionStatus
    {
        _NotComplete,
        _Completed,
        _Canceled
    };

    // _UnrealizedChore represents an unrealized chore -- a unit of work that scheduled in a work
    // stealing capacity. Some higher level construct (language or library) will map atop this to provide
    // an usable abstraction to clients.
    class _UnrealizedChore : public _Chore
    {
    public:
        // Constructor for an unrealized chore.
        _UnrealizedChore()
        {
        }

        // Sets the attachment state of the chore at the time of stealing.
        void _SetDetached(bool _FDetached);

        // Set flag that indicates whether the scheduler owns the lifetime of the object and is responsible for freeing it.
        // The flag is ignored by _StructuredTaskCollection
        void _SetRuntimeOwnsLifetime(bool fValue) 
        { 
            _M_fRuntimeOwnsLifetime = fValue; 
        }

        // Returns the flag that indicates whether the scheduler owns the lifetime of the object and is responsible for freeing it.
        // The flag is ignored by _StructuredTaskCollection
        bool _GetRuntimeOwnsLifetime() const
        {
            return _M_fRuntimeOwnsLifetime;
        }

        // Allocator to be used when runtime owns lifetime.
        template <typename _ChoreType, typename _Function>
        static _ChoreType * _InternalAlloc(const _Function& _Func)
        {
            // This is always invoked from the PPL layer by the user and can never be attached to the default scheduler. Therefore '_concrt_new' is not required here
            _ChoreType * _Chore = new _ChoreType(_Func);
            _Chore->_M_fRuntimeOwnsLifetime = true;
            return _Chore;
        }

    protected:
         // Invocation bridge between the _UnrealizedChore and PPL.
        template <typename _ChoreType>
        static void __cdecl _InvokeBridge(_In_ void * _PContext)
        {
            auto _PChore = static_cast<_ChoreType *>(_PContext);
            (*_PChore)();
        }

    private:

        typedef void (__cdecl * CHOREFUNC)(_UnrealizedChore * _PChore);

        // Indicates whether the scheduler owns the lifetime of the object and is responsible for freeing it.
        // This flag is ignored by _StructuredTaskCollection
        bool _M_fRuntimeOwnsLifetime;

    };
} // namespace details

} // namespace pplx

#pragma pack(pop)

#endif // _PPLX_H
