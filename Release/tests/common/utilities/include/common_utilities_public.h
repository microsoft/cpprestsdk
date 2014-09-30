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
* common_utilities.h -- Common definitions for public test utility headers
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#if !defined(_WIN32) && !defined(__cplusplus_winrt)
#define TEST_UTILITY_API
#endif // !_WIN32 && !__cplusplus_winrt

#ifndef TEST_UTILITY_API
#ifdef COMMONUTILITIES_EXPORTS
#define TEST_UTILITY_API __declspec(dllexport)
#else // COMMONUTILITIES_EXPORTS
#define TEST_UTILITY_API __declspec(dllimport)
#endif // COMMONUTILITIES_EXPORTS
#endif // TEST_UTILITY_API
