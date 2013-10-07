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

#ifndef __PREFIX_H
#define __PREFIX_H

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

#include "unittestpp.h"
#include "streams_tests.h"

template class concurrency::streams::file_buffer<char>;
template class concurrency::streams::file_buffer<wchar_t>;
template class concurrency::streams::streambuf<char>;
template class concurrency::streams::streambuf<wchar_t>;

template class concurrency::streams::rawptr_buffer<char>;
template class concurrency::streams::rawptr_buffer<wchar_t>;
template class concurrency::streams::rawptr_buffer<uint8_t>;
template class concurrency::streams::rawptr_buffer<utf16char>;

template class concurrency::streams::container_buffer<std::vector<uint8_t>>;
template class concurrency::streams::container_buffer<std::vector<char>>;
template class concurrency::streams::container_buffer<std::vector<utf16char>>;

template class concurrency::streams::producer_consumer_buffer<char>;
template class concurrency::streams::producer_consumer_buffer<uint8_t>;
template class concurrency::streams::producer_consumer_buffer<utf16char>;

template class concurrency::streams::container_stream<std::basic_string<char>>;
template class concurrency::streams::container_stream<std::basic_string<wchar_t>>;


#endif