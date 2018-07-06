/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* htto_overlapped: Class used to wrap OVERLAPPED I/O with any HTTP I/O.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

namespace web
{
namespace http
{
namespace experimental
{
namespace details
{
/// <summary>
/// 
/// </summary>
class http_overlapped : public OVERLAPPED
{
public:
    void set_http_io_completion(std::function<void(DWORD, DWORD)> http_io_completion)
    {
        ZeroMemory(this, sizeof(OVERLAPPED));
        m_http_io_completion = http_io_completion;
    }

    /// <summary>
    /// Callback for all I/O completions.
    /// </summary>
    static void CALLBACK io_completion_callback(
        PTP_CALLBACK_INSTANCE instance,
        PVOID context,
        PVOID pOverlapped,
        ULONG result,
        ULONG_PTR numberOfBytesTransferred,
        PTP_IO io)
    {
        CASABLANCA_UNREFERENCED_PARAMETER(io);
        CASABLANCA_UNREFERENCED_PARAMETER(context);
        CASABLANCA_UNREFERENCED_PARAMETER(instance);

        http_overlapped *p_http_overlapped = (http_overlapped *)pOverlapped;
        p_http_overlapped->m_http_io_completion(result, (DWORD) numberOfBytesTransferred);
    }

private:
    std::function<void(DWORD, DWORD)> m_http_io_completion;
};

} // namespace details;
} // namespace experimental
}} // namespace web::http
