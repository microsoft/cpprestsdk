/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Pre-compiled headers
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <vector>
#include <fstream>
#include <memory>
#include <stdio.h>
#include <time.h>

#include "pplx/pplxtasks.h"

#if defined(_WIN32)
#include "pplx/pplxconv.h"
#else
#include "pplx/threadpool.h"
#endif

#include "cpprest/asyncrt_utils.h"
#include "unittestpp.h"
#include "os_utilities.h"
