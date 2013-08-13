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
* fileio_linux.cpp
*
* Asynchronous I/O: stream buffer implementation details
*
* We're going to some lengths to avoid exporting C++ class member functions and implementation details across
* module boundaries, and the factoring requires that we keep the implementation details away from the main header
* files. The supporting functions, which are in this file, use C-like signatures to avoid as many issues as
* possible.
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "cpprest/fileio.h"

using namespace boost::asio;

namespace Concurrency { namespace streams { namespace details {

/***
* ==++==
*
* Scheduler details.
*
* =-=-=-
****/

class io_scheduler;

/// <summary>
/// Scheduler of I/O completions as well as any asynchronous operations that
/// are created internally as opposed to operations created by the application.
/// </summary>
class io_scheduler
{
public:
    /// <summary>
    /// Constructor
    /// </summary>
    io_scheduler()
    : m_outstanding_work(0)
    {
        m_no_outstanding_work.set();
    }

    /// <summary>
    /// Destructor
    /// </summary>
    ~io_scheduler()
    {
        m_no_outstanding_work.wait();
    }

    void submit_io()
    {
        m_no_outstanding_work.reset();
        ++m_outstanding_work; 
    }

    void complete_io()
    {
        if (--m_outstanding_work == 0)
        {
            m_no_outstanding_work.set();
        }
    }

    io_service& service()
    {
        return crossplat::threadpool::shared_instance().service();
    }

private:
    pplx::extensibility::event_t m_no_outstanding_work;

    volatile std::atomic<long> m_outstanding_work;
};

/// <summary>
/// We keep a single instance of the I/O scheduler. In order to create it on first
/// demand, it's referenced through a shared_ptr<T> and retrieved through function call.
/// </summary>
boost::mutex _g_lock;
std::shared_ptr<io_scheduler> _g_scheduler;

/// <summary>
/// Get the I/O scheduler instance.
/// </summary>
std::shared_ptr<io_scheduler> get_scheduler()
{
    boost::lock_guard<boost::mutex> lck(_g_lock);
    if ( !_g_scheduler )
    {
        _g_scheduler = std::make_shared<io_scheduler>();
    }

    return _g_scheduler;
}

/***
* ==++==
*
* Implementation details of the file stream buffer
*
* =-=-=-
****/


/// <summary>
/// The public parts of the file information record contain only what is implementation-
/// independent. The actual allocated record is larger and has details that the implementation
/// require in order to function.
/// </summary>
struct _file_info_impl : _file_info
{
    _file_info_impl(std::shared_ptr<io_scheduler> sched, int handle, std::ios_base::openmode mode, bool buffer_reads) :
        _file_info(mode, 512),
        m_handle(handle),
        m_buffer_reads(buffer_reads),
        m_outstanding_writes(0),
        m_scheduler(sched)
    {
    }

    /// <summary>
    /// The file handle of the file
    /// </summary>
    int  m_handle;

    bool m_buffer_reads;

    /// <summary>
    /// A list of callback waiting to be signalled that there are no outstanding writes.
    /// </summary>
    std::vector<_filestream_callback *> m_sync_waiters;

    std::atomic<long> m_outstanding_writes;

    /// <summary>
    /// A pointer to the scheduler instance used.
    /// </summary>
    /// <remarks>Even though we're using a singleton scheduler instance, it may not always be
    /// the case, so it seems a good idea to keep a pointer here.</remarks>
    std::shared_ptr<io_scheduler> m_scheduler;
};

}}}

using namespace Concurrency::streams::details;

/// <summary>
/// Perform post-CreateFile processing.
/// </summary>
/// <param name="fh">The Win32 file handle</param>
/// <param name="callback">The callback interface pointer</param>
/// <param name="mode">The C++ file open mode</param>
/// <returns>The error code if there was an error in file creation.</returns>
bool _finish_create(int fh, _filestream_callback *callback, std::ios_base::openmode mode, int /* prot */)
{
    if ( fh != -1 ) 
    {
        std::shared_ptr<io_scheduler> sched = get_scheduler();

        // Buffer reads internally if and only if we're just reading (not also writing) and
        // if the file is opened exclusively. If either is false, we're better off just
        // letting the OS do its buffering, even if it means that prompt reads won't
        // happen.
        bool buffer = (mode == std::ios_base::in);
        
        // seek to end if requested
        if (mode & std::ios_base::ate)
        {
            lseek(fh, 0, SEEK_END);
        }

        auto info = new _file_info_impl(sched, fh, mode, buffer);

        if ( mode & std::ios_base::app || mode & std::ios_base::ate )
        {
            info->m_wrpos = (size_t)-1; // Start at the end of the file.
        }

        callback->on_opened(info);
        return true;
    }
    else
    {
        auto exptr = std::make_exception_ptr(std::ios_base::failure("failed to create file"));
        callback->on_error(exptr);
        return false;
    }
}

int get_open_flags(std::ios_base::openmode mode)
{
    int result = 0;
    if (mode & std::ios_base::in)
    {
        if (mode & std::ios_base::out)
        {
            result = O_RDWR;
        }
        else
        {
            result = O_RDONLY;
        }
    }
    else if (mode & std::ios_base::out)
    {
        result = O_WRONLY|O_CREAT;
    }

    if (mode & std::ios_base::app)
    {
        result |= O_APPEND;
    }

    if (mode & std::ios_base::trunc)
    {
        result |= O_TRUNC|O_CREAT;
    }

    return result;
}

/// <summary>
/// Open a file and create a streambuf instance to represent it.
/// </summary>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <param name="filename">The name of the file to open</param>
/// <param name="mode">A creation mode for the stream buffer</param>
/// <param name="prot">A file protection mode to use for the file stream</param>
/// <returns>True if the opening operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully opened, just that the process was started.
/// </remarks>
bool _open_fsb_str(_filestream_callback *callback, const char *filename, std::ios_base::openmode mode, int prot)
{
    if ( callback == nullptr ) return false;
    if ( filename == nullptr ) return false;

    std::string name(filename);

    pplx::create_task([=]() -> void
    {
        int cmode = get_open_flags(mode);
        if(cmode==O_RDWR)
        {
            cmode |= O_CREAT;
        }

        int f = open(name.c_str(), cmode, 0600);

        _finish_create(f, callback, mode, prot);
    });

    return true;
}

/// <summary>
/// Close a file stream buffer.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <returns>True if the closing operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully closed, just that the process was started.
/// </remarks>
bool _close_fsb_nolock(_file_info **info, Concurrency::streams::details::_filestream_callback *callback)
{
    if ( callback == nullptr ) return false;
    if ( info == nullptr || *info == nullptr ) return false;
    
    _file_info_impl *fInfo = (_file_info_impl *)*info;

    if ( fInfo->m_handle == -1 ) return false;

    // Since closing a file may involve waiting for outstanding writes which can take some time
    // if the file is on a network share, the close action is done in a separate task, as
    // CloseHandle doesn't have I/O completion events.
    pplx::create_task([=] () -> void
        {
            bool result = false;

            {
                pplx::extensibility::scoped_recursive_lock_t lock(fInfo->m_lock);

                if ( fInfo->m_handle != -1 )
                {
                    result = close(fInfo->m_handle) != -1;
                }

                if ( fInfo->m_buffer != nullptr )
                {
                    delete[] fInfo->m_buffer;
                }
            }

            delete fInfo;
            if (result)
            {
                callback->on_closed();
            }
            else
            {
                auto exptr = std::make_exception_ptr(std::ios_base::failure("failed to close file"));
                callback->on_error(exptr);
            }
        });

    *info = nullptr;

    return true;
}

bool _close_fsb(_file_info **info, Concurrency::streams::details::_filestream_callback *callback)
{
    if ( callback == nullptr ) return false;
    if ( info == nullptr || *info == nullptr ) return false;
    
    pplx::extensibility::scoped_recursive_lock_t lock((*info)->m_lock);

    return _close_fsb_nolock(info, callback);
}

/// <summary>
/// Initiate an asynchronous (overlapped) write to the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to the data to write</param>
/// <param name="count">The size (in bytes) of the data</param>
/// <returns>0 if the write request is still outstanding, -1 if the request failed, otherwise the size of the data written</returns>
size_t _write_file_async(Concurrency::streams::details::_file_info_impl *fInfo, Concurrency::streams::details::_filestream_callback *callback, const uint8_t *ptr, size_t count, size_t position)
{
    // async file writes are emulated using tasks
    auto sched = get_scheduler();

    ++fInfo->m_outstanding_writes;
    sched->submit_io();

    pplx::create_task([=]() -> void
    {
        size_t abs_position;
        bool must_restore_pos;
        size_t orig_pos;
        if( position == (size_t)-1 )
        {
            orig_pos = lseek(fInfo->m_handle, 0, SEEK_CUR);
            abs_position = lseek(fInfo->m_handle, 0, SEEK_END);
            must_restore_pos = true;
        }
        else
        {
            abs_position = position;
            orig_pos = 0;
            must_restore_pos = false;
        }

        auto bytes_written = pwrite(fInfo->m_handle, ptr, count, abs_position);
        if (bytes_written == -1)
        {
            auto exptr = std::make_exception_ptr(std::ios_base::failure("failed to write file"));
            callback->on_error(exptr);
            perror("failed to write");
        }
        
        if(must_restore_pos)
        {
            lseek(fInfo->m_handle, orig_pos, SEEK_SET);
        }
        
        callback->on_completed(bytes_written);

        {
            pplx::extensibility::scoped_recursive_lock_t lock(fInfo->m_lock);

            // Decrement the counter of outstanding write events.
            if ( --fInfo->m_outstanding_writes == 0 )
            {
                // If this was the last one, signal all objects waiting for it to complete.

                for (auto iter = fInfo->m_sync_waiters.begin(); iter != fInfo->m_sync_waiters.end(); iter++)
                {
                    (*iter)->on_completed(0);
                }
                fInfo->m_sync_waiters.clear();
            }
        }

        delete[] ptr; // free buffer
        sched->complete_io();
    });

    return 0;
}

/// <summary>
/// Initiate an asynchronous (overlapped) read from the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in bytes) of the buffer</param>
/// <param name="offset">The offset in the file to read from</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _read_file_async(Concurrency::streams::details::_file_info_impl *fInfo, Concurrency::streams::details::_filestream_callback *callback, void *ptr, size_t count, size_t offset)
{
    auto sched = get_scheduler();
    sched->submit_io();

    pplx::create_task([=]() -> void
    {
        auto bytes_read = pread(fInfo->m_handle, ptr, count, offset);
        if (bytes_read < 0)
        {
            auto exptr = std::make_exception_ptr(std::ios_base::failure("failed to read file"));
            callback->on_error(exptr);
        }
        else
        {
            callback->on_completed(bytes_read);
        }
        sched->complete_io();
    });

    return 0;
}

template<typename Func>
class _filestream_callback_fill_buffer : public _filestream_callback
{
public:
    _filestream_callback_fill_buffer(_file_info *info, _filestream_callback *callback, Func func) : m_info(info), m_func(func), m_callback(callback) { }

    virtual void on_completed(size_t result)
    {
        m_func(result);
        delete this;
    }
    virtual void on_error(const std::exception &e)
    {
        auto exptr = std::make_exception_ptr(e);
        m_callback->on_error(exptr);
        delete this;
    }
private:
    _file_info *m_info;
    Func        m_func;
    _filestream_callback *m_callback;
};

template<typename Func>
_filestream_callback_fill_buffer<Func> *create_callback(_file_info *info, _filestream_callback *callback, Func func)
{
    return new _filestream_callback_fill_buffer<Func>(info, callback, func);
}

static const size_t PageSize = 512;

size_t _fill_buffer_fsb(_file_info_impl *fInfo, _filestream_callback *callback, size_t count, size_t charSize)
{
    size_t byteCount = count * charSize;
    if ( fInfo->m_buffer == nullptr )
    {
        fInfo->m_bufsize = std::max(PageSize, byteCount);
        fInfo->m_buffer = new char[(size_t)fInfo->m_bufsize];
        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo, callback,
            [=] (size_t result) 
            { 
                pplx::extensibility::scoped_recursive_lock_t lock(fInfo->m_lock);
                fInfo->m_buffill = result / charSize;
                callback->on_completed(result);
            });

        return _read_file_async(fInfo, cb, (uint8_t *)fInfo->m_buffer, fInfo->m_bufsize, fInfo->m_rdpos * charSize);
    }

    // First, we need to understand how far into the buffer we have already read
    // and how much remains.

    size_t bufpos = fInfo->m_rdpos - fInfo->m_bufoff;
    size_t bufrem = fInfo->m_buffill - bufpos;

    if ( bufrem < count )
    {
        fInfo->m_bufsize = std::max(PageSize, byteCount);

        // Then, we allocate a new buffer.

        char *newbuf = new char[(size_t)fInfo->m_bufsize];

        // Then, we copy the unread part to the new buffer and delete the old buffer

        if ( bufrem > 0 )
            memcpy(newbuf, fInfo->m_buffer + bufpos * charSize, bufrem * charSize);

        delete[] fInfo->m_buffer;
        fInfo->m_buffer = newbuf;

        // Then, we read the remainder of the count into the new buffer
        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo, callback,
            [=] (size_t result) 
            { 
                pplx::extensibility::scoped_recursive_lock_t lock(fInfo->m_lock);
                fInfo->m_buffill = result / charSize;
                callback->on_completed(result + bufrem * charSize);
            });

        return _read_file_async(fInfo, cb, (uint8_t*)fInfo->m_buffer + bufrem * charSize, fInfo->m_bufsize - bufrem * charSize, (fInfo->m_rdpos+bufrem)*charSize);
    }
    else
        return byteCount;
}


/// <summary>
/// Read data from a file stream into a buffer
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in bytes) of the buffer</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _getn_fsb(Concurrency::streams::details::_file_info *info, Concurrency::streams::details::_filestream_callback *callback, void *ptr, size_t count, size_t charSize)
{
    if ( callback == nullptr ) return (size_t)-1;
    if ( info == nullptr ) return (size_t)-1;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::extensibility::scoped_recursive_lock_t lock(info->m_lock);

    if ( fInfo->m_handle == -1 ) return (size_t)-1;

    size_t byteCount = count * charSize;

    if ( fInfo->m_buffer_reads )
    {
        auto cb = create_callback(fInfo, callback,
            [=] (size_t read)
            {
                auto copy = std::min(read, byteCount);
                auto bufoff = fInfo->m_rdpos - fInfo->m_bufoff;
                memcpy(ptr, fInfo->m_buffer + bufoff * charSize, copy);
                fInfo->m_atend = copy < byteCount;
                callback->on_completed(copy);
            });

        size_t read = _fill_buffer_fsb(fInfo, cb, count, charSize);

        if ( static_cast<int>(read) > 0 )
        {
            auto copy = std::min(read, byteCount);
            auto bufoff = fInfo->m_rdpos - fInfo->m_bufoff;
            memcpy(ptr, fInfo->m_buffer + bufoff * charSize, copy);
            fInfo->m_atend = copy < byteCount;
            return copy;
        }

        return read;
    }
    else
    {
        return _read_file_async(fInfo, callback, ptr, count, fInfo->m_rdpos * charSize);
    }
}

/// <summary>
/// Write data from a buffer into the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in bytes) of the buffer</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _putn_fsb(Concurrency::streams::details::_file_info *info, Concurrency::streams::details::_filestream_callback *callback, const void *ptr, size_t count, size_t charSize)
{
    if ( callback == nullptr ) return (size_t)-1;
    if ( info == nullptr ) return (size_t)-1;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::extensibility::scoped_recursive_lock_t lock(fInfo->m_lock);

    if ( fInfo->m_handle == -1 ) return (size_t)-1;

    size_t byteSize = count * charSize;
    uint8_t *buf = new uint8_t[byteSize];
    memcpy(buf, ptr, byteSize);

    // To preserve the async write order, we have to move the write head before read.
    auto lastPos = fInfo->m_wrpos;
    if (fInfo->m_wrpos != static_cast<size_t>(-1))
    {
        fInfo->m_wrpos += count;
        lastPos *= charSize;
    }

    return _write_file_async(fInfo, callback, buf, byteSize, lastPos);
}

/// <summary>
/// Write a single byte to the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _putc_fsb(Concurrency::streams::details::_file_info *info, Concurrency::streams::details::_filestream_callback *callback, int ch, size_t charSize)
{
    return _putn_fsb(info, callback, &ch, 1, charSize);
}

/// <summary>
/// Flush all buffered data to the underlying file.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <returns>True if the request was initiated</returns>
bool _sync_fsb(Concurrency::streams::details::_file_info *info, Concurrency::streams::details::_filestream_callback *callback)
{
    if ( callback == nullptr ) return false;
    if ( info == nullptr ) return false;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::extensibility::scoped_recursive_lock_t lock(fInfo->m_lock);

    if ( fInfo->m_handle == -1 ) return false;

    if ( fInfo->m_outstanding_writes > 0 )
        fInfo->m_sync_waiters.push_back(callback);
    else
        callback->on_completed(0);

    return true;
}

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>New file position or -1 if error</returns>
_ASYNCRTIMP size_t _seekrdtoend_fsb(Concurrency::streams::details::_file_info *info, int64_t offset, size_t char_size)
{
    if ( info == nullptr ) return (size_t)-1;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::extensibility::scoped_recursive_lock_t lock(info->m_lock);

    if ( fInfo->m_handle == -1 ) return (size_t)-1;

    if ( fInfo->m_buffer != nullptr )
    {
        delete[] fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }
    
    auto newpos = lseek(fInfo->m_handle, static_cast<__off_t>(offset * char_size), SEEK_END);

    if ( newpos == -1 ) return (size_t)-1;

    fInfo->m_rdpos = newpos / char_size;
    return fInfo->m_rdpos;
}

utility::size64_t _get_size(_In_ concurrency::streams::details::_file_info *info, size_t char_size)
{
    if ( info == nullptr ) return (size_t)-1;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;
    
    pplx::extensibility::scoped_recursive_lock_t lock(info->m_lock);
    
    if ( fInfo->m_handle == -1 ) return (size_t)-1;
    
    if ( fInfo->m_buffer != nullptr )
    {
        delete[] fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }
    
    auto oldpos = lseek(fInfo->m_handle, 0, SEEK_CUR);
    
    if ( oldpos == -1 ) return utility::size64_t(-1);
    
    auto newpos = lseek(fInfo->m_handle, 0, SEEK_END);
    
    if ( newpos == -1 ) return utility::size64_t(-1);
    
    lseek(fInfo->m_handle, oldpos, SEEK_SET);
    
    return utility::size64_t(newpos / char_size);
}

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>New file position or -1 if error</returns>
size_t _seekrdpos_fsb(Concurrency::streams::details::_file_info *info, size_t pos, size_t)
{
    if ( info == nullptr ) return (size_t)-1;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::extensibility::scoped_recursive_lock_t lock(info->m_lock);

    if ( fInfo->m_handle == -1 ) return (size_t)-1;

    if ( pos < fInfo->m_bufoff || pos > (fInfo->m_bufoff+fInfo->m_buffill) )
    {
        delete[] fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }

    fInfo->m_rdpos = pos;
    return fInfo->m_rdpos;
}

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new write location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>New file position or -1 if error</returns>
size_t _seekwrpos_fsb(Concurrency::streams::details::_file_info *info, size_t pos, size_t)
{
    if ( info == nullptr ) return (size_t)-1;
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::extensibility::scoped_recursive_lock_t lock(info->m_lock);

    if ( fInfo->m_handle == -1 ) return (size_t)-1;

    fInfo->m_wrpos = pos;
    return fInfo->m_wrpos;
}
