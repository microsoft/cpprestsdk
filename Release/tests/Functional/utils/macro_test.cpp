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
* macro_test.cpp
*
* Tests cases for macro name conflicts.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "cpprest/base_uri.h"
#include "cpprest/basic_types.h"
#include "cpprest/http_client.h"
#include "cpprest/http_msg.h"
#include "cpprest/http_helpers.h"
#include "cpprest/json.h"
#include "cpprest/uri_builder.h"
#include "compat/windows_compat.h"

namespace tests { namespace functional { namespace utils_tests {

    template<typename U>
    void macro_U_Test()
    {
        U();
    }

    SUITE(macro_test)
    {
        TEST(U_test)
        {
            macro_U_Test<int>();
        }
    }
}}}