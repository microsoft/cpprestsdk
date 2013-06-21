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
* Basic tests for PPLX operations.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

pplx::details::atomic_long s_flag;

pplx::scheduler_interface& get_pplx_dflt_scheduler();

namespace tests { namespace functional { namespace pplx_tests {

SUITE(pplx_op_tests)
{

TEST(task_from_value)
{
    auto val = pplx::task_from_result<int>(17);

    VERIFY_ARE_EQUAL(val.get(), 17);
}

TEST(task_from_value_with_continuation)
{
    auto val = pplx::task_from_result<int>(17);

    int v = 0;

    auto t = val.then([&](int x) { v = x; });
    t.wait();

    VERIFY_ARE_EQUAL(v, 17);
}

TEST(create_task)
{
    auto val = pplx::create_task([]() { return 17; });

    VERIFY_ARE_EQUAL(val.get(), 17);
}

TEST(create_task_with_continuation)
{
    auto val = pplx::create_task([]() { return 17; });

    int v = 0;

    auto t = val.then([&](int x) { v = x; });
    t.wait();

    VERIFY_ARE_EQUAL(v, 17);
}

TEST(task_from_event)
{
    pplx::task_completion_event<int> tce;
    auto val = pplx::create_task(tce);
    tce.set(17);

    VERIFY_ARE_EQUAL(val.get(), 17);
}

TEST(task_from_event_with_continuation)
{
    pplx::task_completion_event<int> tce;
    auto val = pplx::create_task(tce);

    int v = 0;

    auto t = val.then(
        [&](int x) { v = x; });

    tce.set(17);
    t.wait();

    VERIFY_ARE_EQUAL(v, 17);
}

TEST(task_from_event_is_done)
{
    pplx::task_completion_event<long> tce;
    auto val = pplx::create_task(tce);

    pplx::details::atomic_long v(0);

    auto t = val.then(
        [&](long x) { pplx::details::atomic_exchange(v, x); });

    // The task should not have started yet.
    bool isDone = t.is_done();
    VERIFY_ARE_EQUAL(isDone, false);

    // Start the task
    tce.set(17);

    // Wait for the lambda to finish running
    while (!t.is_done())
    {
        // Yield.
    }

    // Verify that the lambda did run
    VERIFY_ARE_EQUAL(v, 17);

    // Wait for the task.
    t.wait();

    VERIFY_ARE_EQUAL(v, 17);
}

TEST(task_from_event_with_exception)
{
    pplx::task_completion_event<long> tce;
    auto val = pplx::create_task(tce);

    pplx::details::atomic_long v(0);

    auto t = val.then(
        [&](long x) { pplx::details::atomic_exchange(v, x); });

    // Start the task
    tce.set_exception(pplx::invalid_operation());

    // Wait for the lambda to finish running
    while (!t.is_done())
    {
        // Yield.
    }

    // Verify that the lambda did run
    VERIFY_ARE_EQUAL(v, 0);

    // Wait for the task.
    try
    {
        t.wait();
    }
    catch (pplx::invalid_operation)
    {
    }
    catch(std::exception_ptr)
    {
        v = 1;
    }

    VERIFY_ARE_EQUAL(v, 0);
}

TEST(schedule_task_hold_then_release)
{
    pplx::details::atomic_long flag(0);

    pplx::task<void> t1([&flag]() {
        while (flag == 0);
    });

    pplx::details::atomic_exchange(flag, 1L);
    t1.wait();
}

// TFS # 521911
TEST(schedule_two_tasks)
{
    pplx::details::atomic_exchange(s_flag, 0L);

    auto nowork = [](){};

    auto defaultTask = pplx::create_task(nowork);
    defaultTask.wait();
    VERIFY_ARE_EQUAL(s_flag, 0);

    pplx::task_completion_event<void> tce;
    auto t = pplx::create_task(tce, get_pplx_dflt_scheduler());

    // 2 continuations to be scheduled on the scheduler.
    // Note that task "t" is not scheduled.
    auto t1 = t.then(nowork).then(nowork);

    tce.set();
    t1.wait();

    VERIFY_ARE_EQUAL(s_flag, 2);
}

TEST(task_throws_exception)
{
    pplx::extensibility::event_t ev;
    bool caught = false;

    // Ensure that exceptions thrown from user lambda
    // are indeed propagated and do not escape out of
    // the task.
    auto t1 = pplx::create_task([&ev]()
    {
        ev.set();
        throw std::logic_error("Should not escape");
    });

    auto t2 = t1.then([]()
    {
        VERIFY_IS_TRUE(false);
    });

    // Ensure that we do not inline the work on this thread
    ev.wait();
    
    try
    {
        t2.wait();
    }
    catch(std::exception&)
    {
        caught = true;
    }

    VERIFY_IS_TRUE(caught);
}

pplx::task<int> make_unwrapped_task()
{
    pplx::task<int> t1([]() {
        return 10;
    });

    return pplx::task<int>([t1]() {
        return t1;
    });
}

TEST(unwrap_task)
{
    pplx::task<int> t = make_unwrapped_task();
    int n = t.get();
    VERIFY_ARE_EQUAL(n,10);
}

TEST(task_from_event_with_tb_continuation)
{
    volatile long flag = 0;

    pplx::task_completion_event<int> tce;
    auto val = pplx::create_task(tce).then(
        [=,&flag](pplx::task<int> op) -> short
        {
            flag = 1;
            return (short)op.get();
        });

    tce.set(17);

    VERIFY_ARE_EQUAL(val.get(), 17);
    VERIFY_ARE_EQUAL(flag, 1);
}

class fcc_370010 
{
public:
    fcc_370010(pplx::task_completion_event<bool> op) : m_op(op) { }

    virtual void on_closed(bool result)
    {
        m_op.set(result);
        delete this;
    }

private:
    pplx::task_completion_event<bool> m_op;
};

TEST(BugRepro370010)
{
    auto result_tce = pplx::task_completion_event<bool>();

    auto f = new fcc_370010(result_tce);

    pplx::task<void> dummy([f]()
    {
        f->on_closed(true);
    });

    auto result = pplx::task<bool>(result_tce);

    VERIFY_IS_TRUE(result.get());
}

TEST(event_set_reset_timeout)
{
    pplx::extensibility::event_t ev;

    ev.set();

    // Wait should succeed as the event was set above
    VERIFY_IS_TRUE(ev.wait(0) == 0);

    // wait should succeed as this is manual reset
    VERIFY_IS_TRUE(ev.wait(0) == 0);

    ev.reset();

    // wait should fail as the event is reset (not set)
    VERIFY_IS_TRUE(ev.wait(0) == pplx::extensibility::event_t::timeout_infinite);
}

struct TimerCallbackContext
{
    pplx::details::timer_t simpleTimer;
    pplx::extensibility::event_t ev;
    volatile long count;

    static void __cdecl TimerCallback(void * context)
    {
        auto timerContext = static_cast<TimerCallbackContext *>(context);

        // Not a strong check but is intended to aid in catching potential stack corruption
        VERIFY_ARE_EQUAL(timerContext->count, 1234);

        timerContext->ev.set();
    }

    static void __cdecl StopInTimerCallback(void * context)
    {
        auto timerContext = static_cast<TimerCallbackContext *>(context);
        timerContext->simpleTimer.stop(false);
        timerContext->ev.set();
    }

    static void __cdecl RepeatCallback(void * context)
    {
        auto timerContext = static_cast<TimerCallbackContext *>(context);
        auto count = timerContext->count--;
        if (count == 0)
        {
            timerContext->ev.set();
        }
    }
};

TEST(timer_oneshot)
{
    TimerCallbackContext context;
    context.count = 1234;

    context.simpleTimer.start(1, false, TimerCallbackContext::TimerCallback, &context);

    VERIFY_ARE_EQUAL(context.ev.wait(1000), 0);

    context.simpleTimer.stop(true);
}

TEST(timer_start_stop)
{
    TimerCallbackContext context;
    context.count = 1234;

    context.simpleTimer.start(100, false, TimerCallbackContext::TimerCallback, &context);
    context.simpleTimer.stop(false);

    // Stopping a stopped timer should be ok
    context.simpleTimer.stop(false);

    // restart the timer
    context.simpleTimer.start(1, false, TimerCallbackContext::TimerCallback, &context);
    VERIFY_ARE_EQUAL(context.ev.wait(1000), 0);
    context.simpleTimer.stop(false);
}

TEST(timer_repeat)
{
    TimerCallbackContext context;
    context.count = 10;

    context.simpleTimer.start(1, true, TimerCallbackContext::RepeatCallback, &context);

    VERIFY_ARE_EQUAL(context.ev.wait(1000), 0);

    context.simpleTimer.stop(true);
}

TEST(timer_stop_in_callback)
{
    TimerCallbackContext context;

    context.simpleTimer.start(1, false, TimerCallbackContext::StopInTimerCallback, &context);
    VERIFY_ARE_EQUAL(context.ev.wait(1000), 0);
}

} // SUITE(pplx_op_tests)

}}}   // namespaces
