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
* pplxinterface.h
*
* PPL interfaces
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _PPLXINTERFACE_H
#define _PPLXINTERFACE_H

#include <memory>

namespace pplx
{

/// <summary>
///     An elementary abstraction for a task, defined as <c>void (__cdecl * TaskProc)(void *)</c>. A <c>TaskProc</c> is called to
///     invoke the body of a task.
/// </summary>
/**/
typedef void (__cdecl * TaskProc)(void *);

/// <summary>
///     Scheduler Interface
/// </summary>
struct __declspec(novtable) scheduler
{
    virtual void schedule( TaskProc, void* ) = 0;
};

/// <summary>
///     Represents a pointer to a scheduler. This class exists to allow the
///     the specification of a shared lifetime by using shared_ptr or just
///     a plain reference by using raw pointer.
/// </summary>
struct scheduler_ptr
{
    /// <summary>
    /// Creates a scheduler pointer from shared_ptr to scheduler
    /// </summary>
    explicit scheduler_ptr(std::shared_ptr<pplx::scheduler> scheduler) : m_sharedScheduler(std::move(scheduler))
    {
        m_scheduler = m_sharedScheduler.get();
    }

    /// <summary>
    /// Creates a scheduler pointer from raw pointer to scheduler
    /// </summary>
    explicit scheduler_ptr(_In_opt_ pplx::scheduler * pScheduler) : m_scheduler(pScheduler)
    {
    }

    /// <summary>
    /// Behave like a pointer
    /// </summary>
    pplx::scheduler *operator->() const
    {
        return get();
    }

    /// <summary>
    ///  Returns the raw pointer to the scheduler
    /// </summary>
    pplx::scheduler * get() const
    {
        return m_scheduler;
    }

    /// <summary>
    /// Test whether the scheduler pointer is non-null
    /// </summary>
    operator bool() const { return get() != nullptr; }

private:

    std::shared_ptr<pplx::scheduler> m_sharedScheduler;
    pplx::scheduler * m_scheduler;
};

} // namespace pplx
#endif // _PPLXINTERFACE_H
