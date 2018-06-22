/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: Client-side APIs.
*
* This file contains the implementation for Windows Desktop, based on WinHTTP or named pipes
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#include "cpprest/http_headers.h"
#include "http_client_impl.h"

#ifndef CPPREST_TARGET_XP
#include <VersionHelpers.h>
#endif

#undef min
#undef max

namespace web
{
namespace http
{
namespace client
{
namespace details
{

std::shared_ptr<_http_client_communicator> create_named_pipe_client(uri&& base_uri, http_client_config&& client_config);
std::shared_ptr<_http_client_communicator> create_winhttp_client(uri&& base_uri, http_client_config&& client_config);

std::shared_ptr<_http_client_communicator> create_platform_final_pipeline_stage(uri&& base_uri, http_client_config&& client_config)
{
    // TODO: It would be better to use scheme instead of the hostname to select the named pipe client
    if (base_uri.host() == U("pipe"))
        return create_named_pipe_client(std::move(base_uri), std::move(client_config));
    else
        return create_winhttp_client(std::move(base_uri), std::move(client_config));
}

}}}}

