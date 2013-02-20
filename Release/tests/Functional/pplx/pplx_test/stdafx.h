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
* stdafx.h
*
* Pre-compiled headers
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

// This header is required to define _MS_WINDOWS
#include "xxpublic.h"

#ifdef _MS_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif

#include <vector>
#include <fstream>
#include <memory>
#include <stdio.h>
#include <time.h>


#include "pplx.h"
#ifndef _MS_WINDOWS
#  include "threadpool.h"
#endif
#if defined(_MS_WINDOWS) && _MSC_VER >= 1700
#include "pplxconv.h"
#endif
#include "asyncrt_utils.h"
#include "pplxtasks.h"
#include "unittestpp.h"
#include "os_utilities.h"
