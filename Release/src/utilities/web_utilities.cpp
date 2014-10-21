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
* Credential and proxy utilities.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt)
#include <Wincrypt.h>
#endif

#include "cpprest\web_utilities.h"

namespace web
{
namespace details
{
#if defined(_MS_WINDOWS) && !defined(__cplusplus_winrt)
void zero_memory_deleter::operator()(::utility::string_t *data) const
{
    SecureZeroMemory(reinterpret_cast<void *>(const_cast<::utility::string_t::value_type *>(data->data())), data->size());
    delete data;
}

win32_encryption::win32_encryption(const std::wstring &data) :
    m_numCharacters(data.size())
{
    const auto dataNumBytes = data.size() * sizeof(std::wstring::value_type);
    m_buffer.resize(dataNumBytes);
    memcpy_s(m_buffer.data(), m_buffer.size(), data.c_str(), dataNumBytes);

    // Buffer must be a multiple of CRYPTPROTECTMEMORY_BLOCK_SIZE
    const auto mod = m_buffer.size() % CRYPTPROTECTMEMORY_BLOCK_SIZE;
    if (mod != 0)
    {
        m_buffer.resize(m_buffer.size() + CRYPTPROTECTMEMORY_BLOCK_SIZE - mod);
    }
    if (!CryptProtectMemory(reinterpret_cast<void *>(m_buffer.data()), m_buffer.size(), CRYPTPROTECTMEMORY_SAME_PROCESS))
    {
        throw ::utility::details::create_system_error(GetLastError());
    }
}

win32_encryption::~win32_encryption()
{
    SecureZeroMemory(m_buffer.data(), m_buffer.size());
}

password_string win32_encryption::decrypt() const
{
    // Copy the buffer and decrypt to avoid having to re-encrypt.
    auto data = password_string(new std::wstring(reinterpret_cast<const std::wstring::value_type *>(m_buffer.data()), m_buffer.size() / 2));
    if (!CryptUnprotectMemory(
        reinterpret_cast<void *>(const_cast<std::wstring::value_type *>(data->c_str())),
        m_buffer.size(),
        CRYPTPROTECTMEMORY_SAME_PROCESS))
    {
        throw ::utility::details::create_system_error(GetLastError());
    }
    data->resize(m_numCharacters);
    return std::move(data);
}
#endif
}

}