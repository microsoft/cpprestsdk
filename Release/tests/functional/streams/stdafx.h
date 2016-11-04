/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* stdafx.h
*
* Pre-compiled headers
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <vector>
#include <fstream>
#include <memory>
#include <stdio.h>
#include <time.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#include <ppltasks.h>
namespace pplx = Concurrency;
#else 
#include "pplx/pplxtasks.h"
#endif

#include "cpprest/asyncrt_utils.h"

#include "cpprest/producerconsumerstream.h"
#include "cpprest/rawptrstream.h"
#include "cpprest/containerstream.h"
#include "cpprest/interopstream.h"
#include "cpprest/streams.h"
#include "cpprest/filestream.h"

#include "os_utilities.h"


#include "unittestpp.h"
#include "streams_tests.h"
