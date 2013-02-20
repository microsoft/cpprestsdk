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
* httpt_test_utilities.h -- This is the "one-stop-shop" header for including http test dependencies
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifndef _WINDOWS_
#define _LETS_SEE_IF_WE_INCLUDED_WINDOWS
#endif

#ifndef _WINSOCK2API_
#define _LETS_SEE_IF_WE_INCLUDED_WINSOCK
#endif

#ifndef __HTTP_H__
#define _LETS_SEE_IF_WE_INCLUDED_WINHTTP
#endif

#include "http_test_utilities_public.h"
#include "http_asserts.h"
#include "test_http_server.h"
#include "test_server_utilities.h"
#include "test_http_client.h"

#ifdef _WINDOWS_
#ifdef _LETS_SEE_IF_WE_INCLUDED_WINDOWS
#error "Don't include windows.h in our public headers"
#endif
#endif

#ifdef _WINSOCK2API_
#ifdef _LETS_SEE_IF_WE_INCLUDED_WINSOCK
#error "Don't include winsock2.h in our public headers"
#endif
#endif

#ifdef __HTTP_H__
#ifdef _LETS_SEE_IF_WE_INCLUDED_WINHTTP
#error "Don't include http.h in our public headers"
#endif
#endif
