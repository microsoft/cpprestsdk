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
* Basic tests for pplx scheduler.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

using namespace utility;

extern pplx::details::atomic_long s_flag;

#ifdef _MS_WINDOWS

class pplx_dflt_scheduler : public pplx::scheduler_interface
{
    struct _Scheduler_Param
    {
        pplx::TaskProc_t m_proc;
        void * m_param;

        _Scheduler_Param(pplx::TaskProc_t proc, void * param)
            : m_proc(proc), m_param(param)
        {
        }
    };

    static void CALLBACK DefaultWorkCallbackTest(PTP_CALLBACK_INSTANCE, PVOID pContext, PTP_WORK)
    {
        auto schedulerParam = (_Scheduler_Param *)(pContext);

        schedulerParam->m_proc(schedulerParam->m_param);

        delete schedulerParam;
    }

    virtual void schedule(pplx::TaskProc_t proc, void* param)
    {
        pplx::details::atomic_increment(s_flag);
        auto schedulerParam = new _Scheduler_Param(proc, param);
        auto work = CreateThreadpoolWork(DefaultWorkCallbackTest, schedulerParam, NULL);

        if (work == nullptr)
        {
            delete schedulerParam;
            throw utility::details::create_system_error(GetLastError());
        }

        SubmitThreadpoolWork(work);
        CloseThreadpoolWork(work);
    }
};


pplx_dflt_scheduler g_pplx_dflt_scheduler;


pplx::scheduler_interface& get_pplx_dflt_scheduler()
{
    return g_pplx_dflt_scheduler;
}

#else

class pplx_dflt_scheduler : public pplx::scheduler_interface
{

    crossplat::threadpool m_pool;


    virtual void schedule(pplx::TaskProc_t proc, void* param)
    {
        pplx::details::atomic_increment(s_flag);
        m_pool.schedule([=]() -> void { proc(param); } );

    }

public:
    pplx_dflt_scheduler() : m_pool(4) {}
};

pplx_dflt_scheduler g_pplx_dflt_scheduler;

pplx::scheduler_interface& get_pplx_dflt_scheduler()
{
    return g_pplx_dflt_scheduler;
}

#endif
