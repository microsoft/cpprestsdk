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
//
// Compilation tests to cover templates for WinRT async object integration with PPL tasks.
//
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
***/

#include "pch.h"

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)

#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof(x[0]))
#endif // SIZEOF_ARRAY

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::FileProperties;
using namespace Windows::Storage::Streams;
using namespace Windows::System::Threading;
using namespace Windows::Networking;
using namespace Windows::Networking::Sockets;
using namespace pplx;

#pragma optimize("", off)
template<typename StarterTaskType, typename OpType, typename ProgressType>
void ThenTests()
{
    task<StarterTaskType> t1;

    task<OpType> t2 = t1.then([](StarterTaskType) -> IAsyncOperation<OpType>^ {
        return nullptr;
    });

    task<OpType> t3 = t1.then([](StarterTaskType) -> IAsyncOperationWithProgress<OpType,ProgressType>^ {
        return nullptr;
    });

    task<void> t4 = t1.then([](StarterTaskType) -> IAsyncAction^ {
        return nullptr;
    });

    task<void> t5 = t1.then([](StarterTaskType) -> IAsyncActionWithProgress<ProgressType>^ {
        return nullptr;
    });
}

template<typename OpType, typename ProgressType>
void CtorTests()
{
    task<OpType> t2([]() -> IAsyncOperation<OpType>^ {
        return nullptr;
    });
    t2 = create_task([]() -> IAsyncOperation<OpType>^ {
        return nullptr;
    });

    task<OpType> t3([]() -> IAsyncOperationWithProgress<OpType,ProgressType>^ {
        return nullptr;
    });
    t3 = create_task([]() -> IAsyncOperationWithProgress<OpType,ProgressType>^ {
        return nullptr;
    });

    task<void> t4 ([]() -> IAsyncAction^ {
        return nullptr;
    });
    t4 = create_task([]() -> IAsyncAction^ {
        return nullptr;
    });

    task<void> t5 ([]() -> IAsyncActionWithProgress<ProgressType>^ {
        return nullptr;
    });
    t5 = create_task([]() -> IAsyncActionWithProgress<ProgressType>^ {
        return nullptr;
    });
}

value struct MyValType{int x;};
ref struct MyRefType{};

void TestCreateAsyncAndCreateTaskFromAsync()
{
    // The original repro:
    create_async( []() -> Platform::String^ {
        return "aa";
    });

    // *** Functor tests ***
    // A-void
    {
        std::function<void(void)> f;
        IAsyncAction^ op = create_async(f);

    }
    // A-void / ct
    {
        std::function<void(cancellation_token)> f;
        IAsyncAction^ op = create_async(f);
        task<void> t = create_task(op, cancellation_token::none());
    }

    {   // AP-builtin
        std::function<void(progress_reporter<int>)> f;
        IAsyncActionWithProgress<int>^ op = create_async(f);
        task<void> t = create_task(op);
    }
    {   // AP-ref
        std::function<void(progress_reporter<MyRefType^>)> f;
        IAsyncActionWithProgress<MyRefType^>^ op = create_async(f);
        task<void> t = create_task(op);
    }
    {   // AP-val
        std::function<void(progress_reporter<MyValType>)> f;
        IAsyncActionWithProgress<MyValType>^ op = create_async(f);
        task<void> t = create_task(op);
    }
    {   // AP-builtin / ct
        std::function<void(progress_reporter<int>, cancellation_token)> f;
        IAsyncActionWithProgress<int>^ op = create_async(f);
        task<void> t = create_task(op, cancellation_token::none());
    }
    {   // AP-ref / ct
        std::function<void(progress_reporter<MyRefType^>, cancellation_token)> f;
        IAsyncActionWithProgress<MyRefType^>^ op = create_async(f);
        task<void> t = create_task(op, cancellation_token::none());
    }
    {   // AP-val / ct
        std::function<void(progress_reporter<MyValType>, cancellation_token)> f;
        IAsyncActionWithProgress<MyValType>^ op = create_async(f);
        task<void> t = create_task(op, cancellation_token::none());
    }

    {   // O-void-builtin
        std::function<int(void)> f;
        IAsyncOperation<int>^ op = create_async(f);
        task<int> t = create_task(op);
    }
    {   // O-builtin / ct
        std::function<int(cancellation_token)> f;
        IAsyncOperation<int>^ op = create_async(f);
        task<int> t = create_task(op, cancellation_token::none());
    }
    {
        // O-void-ref
        std::function<MyRefType^(void)> f;
        IAsyncOperation<MyRefType^>^ op = create_async(f);
        task<MyRefType^> t = create_task(op);
    }
    {   // O-ref / ct
        std::function<MyRefType^(cancellation_token)> f;
        IAsyncOperation<MyRefType^>^ op = create_async(f);
        task<MyRefType^> t = create_task(op, cancellation_token::none());
    }
    {   // O-void-val
        std::function<MyValType(void)> f;
        IAsyncOperation<MyValType>^ op = create_async(f);
        task<MyValType> t = create_task(op);
    }
    {   // O-val / ct
        std::function<MyValType(cancellation_token)> f;
        IAsyncOperation<MyValType>^ op = create_async(f);
        task<MyValType> t = create_task(op, cancellation_token::none());
    }

    {    // OP-builtin-builtin
        std::function<int(progress_reporter<int>)> f;
        IAsyncOperationWithProgress<int,int>^ op = create_async(f);
        task<int> t = create_task(op);
    }
    {   // OP-builtin-builtin / ct
        std::function<int(progress_reporter<int>, cancellation_token)> f;
        IAsyncOperationWithProgress<int, int>^ op = create_async(f);
        task<int> t = create_task(op, cancellation_token::none());
    }
    {   // OP-ref-builtin
        std::function<int(progress_reporter<MyRefType^>)> f;
        IAsyncOperationWithProgress<int,MyRefType^>^ op = create_async(f);
        task<int> t = create_task(op);
    }
    {   // OP-ref-builtin / ct
        std::function<int(progress_reporter<MyRefType^>, cancellation_token)> f;
        IAsyncOperationWithProgress<int, MyRefType^>^ op = create_async(f);
        task<int> t = create_task(op, cancellation_token::none());
    }
    {   // OP-val-builtin
        std::function<int(progress_reporter<MyValType>)> f;
        IAsyncOperationWithProgress<int,MyValType>^ op = create_async(f);
        task<int> t = create_task(op);
    }
    {   // OP-val-builtin / ct
        std::function<int(progress_reporter<MyValType>, cancellation_token)> f;
        IAsyncOperationWithProgress<int, MyValType>^ op = create_async(f);
        task<int> t = create_task(op, cancellation_token::none());
    }

    {    // OP-builtin-ref
        std::function<MyRefType^(progress_reporter<int>)> f;
        IAsyncOperationWithProgress<MyRefType^,int>^ op = create_async(f);
        task<MyRefType^> t = create_task(op);
    }
    {   // OP-builtin-ref / ct
        std::function<MyRefType^(progress_reporter<int>, cancellation_token)> f;
        IAsyncOperationWithProgress<MyRefType^, int>^ op = create_async(f);
        task<MyRefType^> t = create_task(op, cancellation_token::none());
    }
    {   // OP-ref-ref
        std::function<MyRefType^(progress_reporter<MyRefType^>)> f;
        IAsyncOperationWithProgress<MyRefType^,MyRefType^>^ op = create_async(f);
        task<MyRefType^> t = create_task(op);

        // This is redundant, but I'm adding it for good measure
        auto lambda = [](progress_reporter<MyRefType^>) -> MyRefType^ { return nullptr; };
        IAsyncOperationWithProgress<MyRefType^,MyRefType^>^ op2 = create_async(lambda);
    }
    {   // OP-ref-ref / ct
        std::function<MyRefType^(progress_reporter<MyRefType^>, cancellation_token)> f;
        IAsyncOperationWithProgress<MyRefType^, MyRefType^>^ op = create_async(f);
        task<MyRefType^> t = create_task(op, cancellation_token::none());

        // This is redundant, but I'm adding it for good measure
        auto lambda = [](progress_reporter<MyRefType^>, cancellation_token) -> MyRefType^ { return nullptr; };
        IAsyncOperationWithProgress<MyRefType^, MyRefType^>^ op2 = create_async(lambda);
    }
    {   // OP-val-ref
        std::function<MyRefType^(progress_reporter<MyValType>)> f;
        IAsyncOperationWithProgress<MyRefType^,MyValType>^ op = create_async(f);
        task<MyRefType^> t = create_task(op);
    }
    {   // OP-val-ref / ct
        std::function<MyRefType^(progress_reporter<MyValType>, cancellation_token)> f;
        IAsyncOperationWithProgress<MyRefType^,MyValType>^ op = create_async(f);
        task<MyRefType^> t = create_task(op, cancellation_token::none());
    }

    {    // OP-builtin-val
        std::function<MyValType(progress_reporter<int>)> f;
        IAsyncOperationWithProgress<MyValType,int>^ op = create_async(f);
        task<MyValType> t = create_task(op);
    }
    {   // OP-builtin-val / ct
        std::function<MyValType(progress_reporter<int>, cancellation_token)> f;
        IAsyncOperationWithProgress<MyValType, int>^ op = create_async(f);
        task<MyValType> t = create_task(op, cancellation_token::none());
    }
    {   // OP-ref-val
        std::function<MyValType(progress_reporter<MyRefType^>)> f;
        IAsyncOperationWithProgress<MyValType,MyRefType^>^ op = create_async(f);
        task<MyValType> t = create_task(op);
    }
    {   // OP-ref-val / ct
        std::function<MyValType(progress_reporter<MyRefType^>, cancellation_token)> f;
        IAsyncOperationWithProgress<MyValType, MyRefType^>^ op = create_async(f);
        task<MyValType> t = create_task(op, cancellation_token::none());
    }
    {   // OP-val-val
        std::function<MyValType(progress_reporter<MyValType>)> f;
        IAsyncOperationWithProgress<MyValType,MyValType>^ op = create_async(f);
        task<MyValType> t = create_task(op);
    }
    {   // OP-val-val / ct
        std::function<MyValType(progress_reporter<MyValType>, cancellation_token)> f;
        IAsyncOperationWithProgress<MyValType, MyValType>^ op = create_async(f);
        task<MyValType> t = create_task(op, cancellation_token::none());
    }

    // *** Function pointer tests ***
    {
        void (__cdecl *pf1)(void) = nullptr;
        IAsyncAction^ op1 = create_async(pf1);

        void (__cdecl *pf1c)(cancellation_token) = nullptr;
        IAsyncAction^ op1c = create_async(pf1c);

        void (__stdcall *pf2)(void) = nullptr;
        IAsyncAction^ op2 = create_async(pf2);

        void (__stdcall *pf2c)(cancellation_token) = nullptr;
        IAsyncAction^ op2c = create_async(pf2c);

        void (__fastcall *pf3)(void) = nullptr;
        IAsyncAction^ op3 = create_async(pf3);

        void (__fastcall *pf3c)(cancellation_token) = nullptr;
        IAsyncAction^ op3c = create_async(pf3c);
    }

    {
        void (__cdecl *pf1)(progress_reporter<MyRefType^>) = nullptr;
        IAsyncActionWithProgress<MyRefType^>^ op1 = create_async(pf1);

        void (__cdecl *pf1c)(progress_reporter<MyRefType^>, cancellation_token) = nullptr;
        IAsyncActionWithProgress<MyRefType^>^ op1c = create_async(pf1c);

        void (__stdcall *pf2)(progress_reporter<MyRefType^>) = nullptr;
        IAsyncActionWithProgress<MyRefType^>^ op2 = create_async(pf2);

        void (__stdcall *pf2c)(progress_reporter<MyRefType^>, cancellation_token) = nullptr;
        IAsyncActionWithProgress<MyRefType^>^ op2c = create_async(pf2c);

        void (__fastcall *pf3)(progress_reporter<MyRefType^>) = nullptr;
        IAsyncActionWithProgress<MyRefType^>^ op3 = create_async(pf3);

        void (__fastcall *pf3c)(progress_reporter<MyRefType^>, cancellation_token) = nullptr;
        IAsyncActionWithProgress<MyRefType^>^ op3c = create_async(pf3c);
    }

    {
        void (__cdecl *pf1)(progress_reporter<MyValType>) = nullptr;
        IAsyncActionWithProgress<MyValType>^ op1 = create_async(pf1);

        void (__cdecl *pf1c)(progress_reporter<MyValType>, cancellation_token) = nullptr;
        IAsyncActionWithProgress<MyValType>^ op1c = create_async(pf1c);

        void (__stdcall *pf2)(progress_reporter<MyValType>) = nullptr;
        IAsyncActionWithProgress<MyValType>^ op2 = create_async(pf2);

        void (__stdcall *pf2c)(progress_reporter<MyValType>, cancellation_token) = nullptr;
        IAsyncActionWithProgress<MyValType>^ op2c = create_async(pf2c);

        void (__fastcall *pf3)(progress_reporter<MyValType>) = nullptr;
        IAsyncActionWithProgress<MyValType>^ op3 = create_async(pf3);

        void (__fastcall *pf3c)(progress_reporter<MyValType>, cancellation_token) = nullptr;
        IAsyncActionWithProgress<MyValType>^ op3c = create_async(pf3c);
    }

    {
        int (__cdecl *pf1)(void) = nullptr;
        IAsyncOperation<int>^ op1 = create_async(pf1);

        int (__cdecl *pf1c)(cancellation_token) = nullptr;
        IAsyncOperation<int>^ op1c = create_async(pf1c);

        int (__stdcall *pf2)(void) = nullptr;
        IAsyncOperation<int>^ op2 = create_async(pf2);

        int (__stdcall *pf2c)(cancellation_token) = nullptr;
        IAsyncOperation<int>^ op2c = create_async(pf2c);

        int (__fastcall *pf3)(void) = nullptr;
        IAsyncOperation<int>^ op3 = create_async(pf3);

        int (__fastcall *pf3c)(void) = nullptr;
        IAsyncOperation<int>^ op3c = create_async(pf3c);
    }

#if 0 // blocked by TFS#288791 and TFS#246576
    {
        MyRefType^ (__cdecl *pf1)(void) = nullptr;
        IAsyncOperation<MyRefType^>^ op1 = create_async(pf1);

        MyRefType^ (__cdecl *pf1c)(cancellation_token) = nullptr;
        IAsyncOperation<MyRefType^>^ op1c = create_async(pf1c);

        MyRefType^ (__stdcall *pf2)(void) = nullptr;
        IAsyncOperation<MyRefType^>^ op2 = create_async(pf2);

        MyRefType^ (__stdcall *pf2c)(cancellation_token) = nullptr;
        IAsyncOperation<MyRefType^>^ op2c = create_async(pf2c);

        MyRefType^ (__fastcall *pf3)(void) = nullptr;
        IAsyncOperation<MyRefType^>^ op3 = create_async(pf3);

        MyRefType^ (__fastcall *pf3c)(cancellation_token) = nullptr;
        IAsyncOperation<MyRefType^>^ op3c = create_async(pf3c);
    }

    {
        MyValType (__cdecl *pf1)(void) = nullptr;
        IAsyncOperation<MyValType>^ op1 = create_async(pf1);

        MyValType (__cdecl *pf1c)(cancellation_token) = nullptr;
        IAsyncOperation<MyValType>^ op1c = create_async(pf1c);

        MyValType (__stdcall *pf2)(void) = nullptr;
        IAsyncOperation<MyValType>^ op2 = create_async(pf2);

        MyValType (__stdcall *pf2c)(cancellation_token) = nullptr;
        IAsyncOperation<MyValType>^ op2c = create_async(pf2c);

        MyValType (__fastcall *pf3)(void) = nullptr;
        IAsyncOperation<MyValType>^ op3 = create_async(pf3);

        MyValType (__fastcall *pf3c)(cancellation_token) = nullptr;
        IAsyncOperation<MyValType>^ op3c = create_async(pf3c);
    }
#endif

    {
        int (__cdecl *pf1)(progress_reporter<int>) = nullptr;
        IAsyncOperationWithProgress<int,int>^ op1 = create_async(pf1);

        int (__cdecl *pf1c)(progress_reporter<int>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, int>^ op1c = create_async(pf1c);

        int (__stdcall *pf2)(progress_reporter<int>) = nullptr;
        IAsyncOperationWithProgress<int,int>^ op2 = create_async(pf2);

        int (__stdcall *pf2c)(progress_reporter<int>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, int>^ op2c = create_async(pf2c);

        int (__fastcall *pf3)(progress_reporter<int>) = nullptr;
        IAsyncOperationWithProgress<int,int>^ op3 = create_async(pf3);

        int (__fastcall *pf3c)(progress_reporter<int>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, int>^ op3c = create_async(pf3c);
    }

    {
        int (__cdecl *pf1)(progress_reporter<MyValType>) = nullptr;
        IAsyncOperationWithProgress<int,MyValType>^ op1 = create_async(pf1);

        int (__cdecl *pf1c)(progress_reporter<MyValType>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, MyValType>^ op1c = create_async(pf1c);

        int (__stdcall *pf2)(progress_reporter<MyValType>) = nullptr;
        IAsyncOperationWithProgress<int,MyValType>^ op2 = create_async(pf2);

        int (__stdcall *pf2c)(progress_reporter<MyValType>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, MyValType>^ op2c = create_async(pf2c);

        int (__fastcall *pf3)(progress_reporter<MyValType>) = nullptr;
        IAsyncOperationWithProgress<int,MyValType>^ op3 = create_async(pf3);

        int (__fastcall *pf3c)(progress_reporter<MyValType>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, MyValType>^ op3c = create_async(pf3c);
    }

    {
        int (__cdecl *pf1)(progress_reporter<MyRefType^>) = nullptr;
        IAsyncOperationWithProgress<int,MyRefType^>^ op1 = create_async(pf1);

        int (__cdecl *pf1c)(progress_reporter<MyRefType^>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, MyRefType^>^ op1c = create_async(pf1c);

        int (__stdcall *pf2)(progress_reporter<MyRefType^>) = nullptr;
        IAsyncOperationWithProgress<int,MyRefType^>^ op2 = create_async(pf2);

        int (__stdcall *pf2c)(progress_reporter<MyRefType^>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, MyRefType^>^ op2c = create_async(pf2c);

        int (__fastcall *pf3)(progress_reporter<MyRefType^>) = nullptr;
        IAsyncOperationWithProgress<int,MyRefType^>^ op3 = create_async(pf3);

        int (__fastcall *pf3c)(progress_reporter<MyRefType^>, cancellation_token) = nullptr;
        IAsyncOperationWithProgress<int, MyRefType^>^ op3c = create_async(pf3c);
    }
}


void TestTaskTypeTraits()
{
    task<int> t1;
    typedef decltype(t1) TaskType;
    static_assert(std::is_same<TaskType::result_type,int>::value, "result_type must be int!");

    static_assert(std::is_same<task<String^>::result_type,String^>::value, "result_type must be String^!");
}

void Tests()
{
#pragma warning(disable: 4127)

    // Compile-only, never run
    if( false )
    {
        ThenTests<int,int,int>();
        ThenTests<int,String^,int>();
        ThenTests<int,int,String^>();
        ThenTests<int,String^,String^>();

        ThenTests<String^,int,int>();
        ThenTests<String^,String^,int>();
        ThenTests<String^,int,String^>();
        ThenTests<String^,String^,String^>();

        CtorTests<int,int>();
        CtorTests<int,String^>();
        CtorTests<String^,int>();
        CtorTests<String^,String^>();
    }
}
#pragma optimize("", on)
namespace unittests
{
void compile_tests()
{
    TestCreateAsyncAndCreateTaskFromAsync();
    TestTaskTypeTraits();
    Tests();
}
}