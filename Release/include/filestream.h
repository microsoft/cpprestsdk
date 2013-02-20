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
* filestream.h
*
* Asynchronous File streams
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_FILE_STREAMS_H
#define _CASA_FILE_STREAMS_H

#include "fileio.h"
#include "astreambuf.h"
#include "streams.h"

#ifndef _CONCRT_H
#ifndef _LWRCASE_CNCRRNCY
#define _LWRCASE_CNCRRNCY
// Note to reader: we're using lower-case namespace names everywhere, but the 'Concurrency' namespace
// is capitalized for historical reasons. The alias let's us pretend that style issue doesn't exist.
namespace Concurrency { }
namespace concurrency = Concurrency;
#endif
#endif

#pragma warning(push)
// Suppress unreferenced formal parameter warning as they are required for documentation.
#pragma warning(disable : 4100)
// Suppress no-side-effect recursion warning, since it is safe and template-binding-dependent.
#pragma warning(disable : 4718)

namespace Concurrency { namespace streams
{
    // Forward declarations
    template<typename _CharType> class file_buffer;

    namespace details {

    /// <summary>
    /// Private stream buffer implementation for file streams.
    /// </summary>
    template<typename _CharType>
    class basic_file_buffer : public details::streambuf_state_manager<_CharType>
    {
    public:
        typedef typename basic_streambuf<_CharType>::traits traits;
        typedef typename basic_streambuf<_CharType>::int_type int_type;
        typedef typename basic_streambuf<_CharType>::pos_type pos_type;
        typedef typename basic_streambuf<_CharType>::off_type off_type;

        virtual ~basic_file_buffer() { this->close().wait(); }
    protected:

        /// <summary>
        /// can_seek is used to determine whether a stream buffer supports seeking.
        /// </summary>
        virtual bool can_seek() const { return this->is_open(); }

        /// <summary>
        /// Get the stream buffer size, if one has been set.
        /// </summary>
        /// <param name="direction">The direction of buffering (in or out)</param>
        /// <remarks>An implementation that does not support buffering will always return '0'.</remarks>
        virtual size_t buffer_size(std::ios_base::openmode direction = std::ios_base::in) const
        {
            if ( direction == std::ios_base::in )
                return m_info->m_buffer_size;
            else
                return 0;
        }

        /// <summary>
        /// Set the stream buffer implementation to buffer or not buffer.
        /// </summary>
        /// <param name="size">The size to use for internal buffering, 0 if no buffering should be done.</param>
        /// <param name="direction">The direction of buffering (in or out)</param>
        /// <remarks>An implementation that does not support buffering will silently ignore calls to this function and it will not have
        ///          any effect on what is returned by subsequent calls to buffer_size().</remarks>
        virtual void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in) 
        {
            if ( direction == std::ios_base::out ) return;

            m_info->m_buffer_size = size;

            if ( size == 0 && m_info->m_buffer != nullptr )
            {
               delete m_info->m_buffer;
               m_info->m_buffer = nullptr;
            }
        }

        /// <summary>
        /// For any input stream, in_avail returns the number of characters that are immediately available
        /// to be consumed without blocking. May be used in conjunction with sbumpc() to read data without
        /// incurring the overhead of using tasks.
        /// </summary>
        virtual size_t in_avail() const
        {
            pplx::scoped_recursive_lock lck(m_info->m_lock);

            return _in_avail_unprot();
        }

        size_t _in_avail_unprot() const
        {
            if ( !this->is_open() ) return 0;

            if ( m_info->m_buffer == nullptr || m_info->m_buffill == 0 ) return 0;
            if ( m_info->m_bufoff > m_info->m_rdpos || (m_info->m_bufoff+m_info->m_buffill) < m_info->m_rdpos ) return 0;
#ifdef _MS_WINDOWS
            msl::utilities::SafeInt<size_t> rdpos(m_info->m_rdpos);
            msl::utilities::SafeInt<size_t> buffill(m_info->m_buffill);
            msl::utilities::SafeInt<size_t> bufpos = rdpos - m_info->m_bufoff;
#else

            size_t rdpos(m_info->m_rdpos);
            size_t buffill(m_info->m_buffill);
            size_t bufpos = rdpos - m_info->m_bufoff;
#endif

            return buffill - bufpos;
        }

        _file_info * _close_stream()
        {
            // indicate that we are no longer open
            auto fileInfo = m_info;
            m_info = nullptr;
            return fileInfo;
        }

        static pplx::task<bool> _close_file(_In_ _file_info * fileInfo)
        {
            auto result_tce = pplx::task_completion_event<bool>();
            auto callback = new _filestream_callback_close(result_tce);

            if ( !_close_fsb_nolock(&fileInfo, callback) )
            {
                delete callback;
                return pplx::task_from_result(false);
            }
            return pplx::task<bool>(result_tce);
        }

        pplx::task<bool> _close_read()
        {
            pplx::scoped_recursive_lock lck(m_info->m_lock);
            streambuf_state_manager<_CharType>::_close_read();

            if (this->can_write()) 
            {
                return pplx::task_from_result(true);
            }
            else
            {
                // Neither heads are open. Close the underlying device
                // indicate that we are no longer open
                auto fileInfo = _close_stream();

                return _close_file(fileInfo);
            }
        }

        pplx::task<bool> _close_write()
        {
            pplx::scoped_recursive_lock lck(m_info->m_lock);
            streambuf_state_manager<_CharType>::_close_write();
            if (this->can_read()) 
            {
                // Read head is still open. Just flush the write data
                return flush_internal();
            }
            else
            {
                // Neither heads are open. Close the underlying device

                // We need to flush all writes if the file was opened for writing.
                return flush_internal().then([=](pplx::task<bool> flushTask) -> pplx::task<bool>
                {
                    // swallow exception from flush
                    try
                    {
                        flushTask.wait();
                    }
                    catch(...)
                    {
                    }

                    // indicate that we are no longer open
                    auto fileInfo = this->_close_stream();

                    return this->_close_file(fileInfo);
                });
            }
        }

        /// <summary>
        /// Write a single byte to an output stream.
        /// </summary>
        /// <param name="ch">The byte to write</param>
        /// <returns>The value of the byte. EOF if the write operation fails</returns>
        virtual pplx::task<int_type> _putc(_CharType ch)
        {
            auto result_tce = pplx::task_completion_event<int_type>();
            auto callback = new _filestream_callback_putc(m_info, result_tce, ch);
             
            size_t written = _putc_fsb(m_info, callback, ch, sizeof(_CharType));

            switch(written)
            {
                case size_t(-1):
                {
                delete callback;
                    return pplx::task_from_result<int_type>(traits::eof());
                }
                case sizeof(_CharType):
                {
                    delete callback;
                    return pplx::task_from_result<int_type>(ch);
                }
            }
            return pplx::create_task(result_tce);
        }

        /// <summary>
        /// Allocate a contiguous memory block and return it.
        /// </summary>
        /// <param name="count">The number of characters to allocate.</param>
        /// <returns>A pointer to a block to write to, null if the stream buffer implementation does not support alloc/commit.</returns>
        _CharType* alloc(size_t)
        {
            return nullptr; 
        }

        /// <summary>
        /// Submit a block already allocated by the stream buffer.
        /// </summary>
        /// <param name="ptr">Count of characters to be commited.</param>
        void commit(size_t)
        {
        }

        /// <summary>
        /// Get a pointer to the next already allocated contiguous block of data. Move the read position ahead
        /// by the size of the block.
        /// </summary>
        /// <param name="ptr">A reference to a pointer variable that will hold the address of the block on success.</param>
        /// <param name="count">The number of contiguous characters available at the address in 'ptr.'</param>
        /// <returns>True if the operation suceeded, false otherwise.</returns>
        /// <remarks>
        /// The stream buffer may not de-allocate the block until 'release' is called.
        /// If the end of the stream is reached, the function will return 'true', a null pointer, and a count of zero;
        /// a subsequent read will not succeed.
        /// </remarks>
        virtual bool acquire(_Out_writes_ (count) _CharType*& ptr, _In_ size_t& count)
        {
            ptr = nullptr;
            count = 0;
            return false; 
        }

        /// <summary>
        /// Release a block of data acquired using 'acquire().' This frees the stream buffer to de-allocate the
        /// memory, if it so desires.
        /// </summary>
        /// <param name="ptr">A pointer to the block of data to be released.</param>
        /// <param name="count">The number of characters that were not read.</param>
        virtual void release(_Out_writes_ (count) _CharType *, _In_ size_t)
        {
        }

        /// <summary>
        /// Write a number of characters to the stream.
        /// </summary>
        /// <param name="ptr">A pointer to the block of data to be written.</param>
        /// <param name="count">The number of characters to write.</param>
        /// <returns>The number of characters actually written, which may be less than what was requested. EOF if write the operation fails.</returns>
        virtual pplx::task<size_t> _putn(const _CharType *ptr, size_t count)
        {
            auto result_tce = pplx::task_completion_event<size_t>();
            auto callback = new _filestream_callback_write<size_t>(m_info, result_tce);

            size_t written = _putn_fsb(m_info, callback, ptr, count, sizeof(_CharType));

            if ( written != 0 )
            {
                delete callback;
                if ( written != -1 )
                {
                    written = written/sizeof(_CharType);
                }
                return pplx::task_from_result<size_t>(written);
            }
            return pplx::create_task(result_tce);
        }

        /// <summary>
        /// Read a single byte from the stream and advance the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails.</returns>
        virtual pplx::task<int_type> _bumpc()
        {
            if ( _in_avail_unprot() > 0 )
            {
                pplx::scoped_recursive_lock lck(m_info->m_lock);

                // Check again once the lock is held.

                if ( _in_avail_unprot() > 0 )
                {
                    auto bufoff = m_info->m_rdpos - m_info->m_bufoff;
                    _CharType ch = m_info->m_buffer[bufoff*sizeof(_CharType)];
                    m_info->m_rdpos += 1;
                    return pplx::task_from_result<int_type>(ch);
                }
            }

            auto result_tce = pplx::task_completion_event<int_type>();
            auto callback = new _filestream_callback_bumpc(m_info, result_tce);

            size_t ch = _getn_fsb(m_info, callback, &callback->m_ch, 1, sizeof(_CharType));

            switch (ch)
            {
            case size_t(-1):
                {
                    delete callback;
                    return pplx::task_from_result<int_type>(traits::eof());
                }
                break;
            case sizeof(_CharType):
                {
                    pplx::scoped_recursive_lock lck(m_info->m_lock);
                    m_info->m_rdpos += 1;
                    _CharType ch1 = (_CharType)callback->m_ch;
                    delete callback;
                    return pplx::task_from_result<int_type>(ch1);
                }
                break;
            }

            return pplx::create_task(result_tce);
        }

        /// <summary>
        /// Read a single byte from the stream and advance the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails. 'requires_async' if an asynchronous read is required</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        virtual int_type _sbumpc() 
        {
            if ( m_info->m_atend ) return traits::eof();

            if ( _in_avail_unprot() == 0 ) return traits::requires_async();

            pplx::scoped_recursive_lock lck(m_info->m_lock);

            if ( _in_avail_unprot() == 0 ) return traits::requires_async();

            auto bufoff = m_info->m_rdpos - m_info->m_bufoff;
            _CharType ch = m_info->m_buffer[bufoff*sizeof(_CharType)];
            m_info->m_rdpos += 1;
            return (int_type)ch;
        }

        /// <summary>
        /// Read a single byte from the stream without advancing the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails.</returns>
        pplx::task<int_type> _getc()
        {
            if ( _in_avail_unprot() > 0 )
            {
                pplx::scoped_recursive_lock lck(m_info->m_lock);

                // Check again once the lock is held.

                if ( _in_avail_unprot() > 0 )
                {
                    auto bufoff = m_info->m_rdpos - m_info->m_bufoff;
                    _CharType ch = m_info->m_buffer[bufoff*sizeof(_CharType)];
                    return pplx::task_from_result<int_type>(ch);
                }
            }

            auto result_tce = pplx::task_completion_event<int_type>();
            auto callback = new _filestream_callback_getc(m_info, result_tce);

            size_t ch = _getn_fsb(m_info, callback, &callback->m_ch, 1, sizeof(_CharType));

            switch (ch)
            {
            case size_t(-1):
                {
                    delete callback;
                    return pplx::task_from_result<int_type>(traits::eof());
                }
                break;
            case sizeof(_CharType):
                {
                    pplx::scoped_recursive_lock lck(m_info->m_lock);
                    _CharType ch1 = (_CharType)callback->m_ch;
                    delete callback;
                    return pplx::task_from_result<int_type>(ch1);
                }
                break;
            }
            return pplx::create_task(result_tce);
        }

        /// <summary>
        /// Read a single byte from the stream without advancing the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails. 'requires_async' if an asynchronous read is required</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        int_type _sgetc()
        {
            if ( m_info->m_atend ) return traits::eof();

            if ( _in_avail_unprot() == 0 ) return traits::requires_async();

            pplx::scoped_recursive_lock lck(m_info->m_lock);

            if ( _in_avail_unprot() == 0 ) return traits::requires_async();

            auto bufoff = m_info->m_rdpos - m_info->m_bufoff;
            _CharType ch = m_info->m_buffer[bufoff*sizeof(_CharType)];
            return (int_type)ch;
        }

        /// <summary>
        /// Advance the read position, then return the next character withouth advancing again.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails.</returns>
        virtual pplx::task<int_type> _nextc()
        {
            _seekrdpos_fsb(m_info, m_info->m_rdpos+1, sizeof(_CharType));
            if ( m_info->m_atend )
                return pplx::task_from_result(traits::eof());
            return this->getc();
        }

        /// <summary>
        /// Retreat the read position, then return the current character withouth advancing.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails. 'requires_async' if an asynchronous read is required</returns>
        virtual pplx::task<int_type> _ungetc() 
        {
            if ( m_info->m_rdpos == 0 ) 
                return pplx::task_from_result<int_type>(traits::eof());
            _seekrdpos_fsb(m_info, m_info->m_rdpos-1, sizeof(_CharType));
            return this->getc();
        }

        /// <summary>
        /// Read up to a given number of characters from the stream.
        /// </summary>
        /// <param name="ptr">The address of the target memory area</param>
        /// <param name="count">The maximum number of characters to read</param>
        /// <returns>The number of characters read. O if the end of the stream is reached, EOF if there's some error.</returns>
        virtual pplx::task<size_t> _getn(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
        {
            if ( m_info->m_atend || count == 0 )
                return pplx::task_from_result<size_t>(0);

            if ( _in_avail_unprot() >= count )
            {
                pplx::scoped_recursive_lock lck(m_info->m_lock);

                // Check again once the lock is held.

                if ( _in_avail_unprot() >= count )
                {
                    auto bufoff = m_info->m_rdpos - m_info->m_bufoff;
                    std::memcpy((void *)ptr, this->m_info->m_buffer+bufoff*sizeof(_CharType), count*sizeof(_CharType));
            
                    m_info->m_rdpos += count;
                    return pplx::task_from_result<size_t>(count);
                }
            }

            auto result_tce = pplx::task_completion_event<size_t>();
            auto callback = new _filestream_callback_read(m_info, result_tce);

            size_t read = _getn_fsb(m_info, callback, ptr, count, sizeof(_CharType));

            if ( read != 0 )
            {
                delete callback;
                if ( read != -1 )
                {
                    pplx::scoped_recursive_lock lck(m_info->m_lock);
                    m_info->m_rdpos += read/sizeof(_CharType);
                }
                return pplx::task_from_result<size_t>(read/sizeof(_CharType));
            }
            return pplx::create_task(result_tce);
        }

        /// <summary>
        /// Read up to a given number of characters from the stream.
        /// </summary>
        /// <param name="ptr">The address of the target memory area</param>
        /// <param name="count">The maximum number of characters to read</param>
        /// <returns>The number of characters read. O if the end of the stream is reached or an asynchronous read is required.</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        size_t _sgetn(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
        {
            if ( m_info->m_atend ) return 0;

            if ( count == 0 || in_avail() == 0 ) return 0;

            pplx::scoped_recursive_lock lck(m_info->m_lock);

            size_t available = _in_avail_unprot();
            size_t copy = (count < available) ? count : available;

            auto bufoff = m_info->m_rdpos - m_info->m_bufoff;
            std::memcpy((void *)ptr, this->m_info->m_buffer+bufoff*sizeof(_CharType), copy*sizeof(_CharType));
            
            m_info->m_rdpos += copy;
            m_info->m_atend = (copy < count);
            return copy;
        }

        /// <summary>
        /// Copy up to a given number of characters from the stream.
        /// </summary>
        /// <param name="ptr">The address of the target memory area</param>
        /// <param name="count">The maximum number of characters to copy</param>
        /// <returns>The number of characters copied. O if the end of the stream is reached or an asynchronous read is required.</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        virtual size_t _scopy(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
        {
            return 0;
        }

        /// <summary>
        /// Seek to the given position.
        /// </summary>
        /// <param name="pos">The offset from the beginning of the stream</param>
        /// <param name="direction">The I/O direction to seek (see remarks)</param>
        /// <returns>The position. EOF if the operation fails.</returns>
        /// <remarks>Some streams may have separate write and read cursors. 
        ///          For such streams, the direction parameter defines whether to move the read or the write cursor.</remarks>
        virtual pos_type seekpos(pos_type pos, std::ios_base::openmode mode) 
        {
            if ( mode == std::ios_base::in ) 
            {
                return (pos_type)_seekrdpos_fsb(m_info, size_t(pos), sizeof(_CharType)); 
            }
            else if ( (m_info->m_mode & std::ios::ios_base::app) == 0 )
            {
                return (pos_type)_seekwrpos_fsb(m_info, size_t(pos), sizeof(_CharType)); 
            }
            return (pos_type)std::char_traits<_CharType>::eof(); 
        }

        /// <summary>
        /// Seek to a position given by a relative offset.
        /// </summary>
        /// <param name="offset">The relative position to seek to</param>
        /// <param name="way">The starting point (beginning, end, current) for the seek.</param>
        /// <param name="mode">The I/O direction to seek (see remarks)</param>
        /// <returns>The position. EOF if the operation fails.</returns>
        /// <remarks>Some streams may have separate write and read cursors. 
        ///          For such streams, the mode parameter defines whether to move the read or the write cursor.</remarks>
        virtual pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode) 
        {
            if ( mode == std::ios_base::in ) 
            {
                switch ( way )
                {
                case std::ios_base::beg:
                    return (pos_type)_seekrdpos_fsb(m_info, size_t(offset), sizeof(_CharType));
                case std::ios_base::cur:
                    return (pos_type)_seekrdpos_fsb(m_info, size_t(m_info->m_rdpos+offset), sizeof(_CharType));
                case std::ios_base::end:
                    return (pos_type)_seekrdtoend_fsb(m_info, int64_t(offset), sizeof(_CharType));
                    break;
                }
            }
            else if ( (m_info->m_mode & std::ios::ios_base::app) == 0 )
            {
                switch ( way )
                {
                case std::ios_base::beg:
                    return (pos_type)_seekwrpos_fsb(m_info, size_t(offset), sizeof(_CharType));
                case std::ios_base::cur:
                    return (pos_type)_seekwrpos_fsb(m_info, size_t(m_info->m_wrpos+offset), sizeof(_CharType));
                case std::ios_base::end:
                    return (pos_type)_seekwrpos_fsb(m_info, size_t(-1), sizeof(_CharType));
                    break;
                }
            }
            return (pos_type)traits::eof(); 
        }

        /// <summary>
        /// For output streams, flush any internally buffered data to the underlying medium.
        /// </summary>
        /// <returns>O if the flush succeeds, EOF if not</returns>
        virtual pplx::task<bool> _sync()
        {
            return flush_internal();
        }

    private:
        template<typename _CharType1> friend class ::concurrency::streams::file_buffer;

        pplx::task<bool> flush_internal()
        {
            auto result_tce = pplx::task_completion_event<bool>();
            auto callback = new _filestream_callback_write_b(m_info, result_tce);

            if ( !_sync_fsb(m_info, callback) )
            {
                delete callback;
                return pplx::task_from_result(false);
            }
            return pplx::task<bool>(result_tce);
        }

        basic_file_buffer(_In_ _file_info *info) : m_info(info), streambuf_state_manager<_CharType>(info->m_mode) { }

#if !defined(__cplusplus_winrt)
        static pplx::task<std::shared_ptr<basic_streambuf<_CharType>>> open(
            const utility::string_t &_Filename,
            std::ios_base::openmode _Mode = std::ios_base::out,
#ifdef _MS_WINDOWS
            int _Prot = (int)std::ios_base::_Openprot
#else
            int _Prot = 0 // unsupported on Linux, for now        
#endif
            )
        {
            auto result_tce = pplx::task_completion_event<std::shared_ptr<basic_streambuf<_CharType>>>();
            auto callback = new _filestream_callback_open(result_tce);
            _open_fsb_str(callback, _Filename.c_str(), _Mode, _Prot);
            return pplx::create_task(result_tce);
        }
#else
        static pplx::task<std::shared_ptr<basic_streambuf<_CharType>>> open(
            ::Windows::Storage::StorageFile^ file,
            std::ios_base::openmode _Mode = std::ios_base::out)
        {
            auto result_tce = pplx::task_completion_event<std::shared_ptr<basic_streambuf<_CharType>>>();
            auto callback = new _filestream_callback_open(result_tce);
            _open_fsb_stf_str(callback, file, _Mode, 0);
            return pplx::create_task(result_tce);
        }
#endif

#pragma region Completion callback interface implementations
        class _filestream_callback_open : public details::_filestream_callback
        {
        public:
            _filestream_callback_open(pplx::task_completion_event<std::shared_ptr<basic_streambuf<_CharType>>> op) : m_op(op) { }

            virtual void on_opened(_In_ _file_info *info)
            {
                m_op.set(std::shared_ptr<basic_file_buffer<_CharType>>(new basic_file_buffer<_CharType>(info)));
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            pplx::task_completion_event<std::shared_ptr<basic_streambuf<_CharType>>> m_op;
        };

        class _filestream_callback_close : public details::_filestream_callback
        {
        public:
            _filestream_callback_close(pplx::task_completion_event<bool> op) : m_op(op) { }

            virtual void on_closed(bool result)
            {
                m_op.set(result);
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            pplx::task_completion_event<bool> m_op;
        };

        template<typename ResultType>
        class _filestream_callback_write : public details::_filestream_callback
        {
        public:
            _filestream_callback_write(_In_ _file_info *info, pplx::task_completion_event<ResultType> op) : m_info(info), m_op(op) { }

            virtual void on_completed(size_t result)
            {
                m_op.set((ResultType)result / sizeof(_CharType));
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            _file_info *m_info;
            pplx::task_completion_event<ResultType> m_op;
        };

        class _filestream_callback_write_b : public details::_filestream_callback
        {
        public:
            _filestream_callback_write_b(_In_ _file_info *info, pplx::task_completion_event<bool> op) : m_info(info), m_op(op) { }

            virtual void on_completed(size_t result)
            {
                m_op.set(result == 0);
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            _file_info *m_info;
            pplx::task_completion_event<bool> m_op;
        };


        class _filestream_callback_read : public details::_filestream_callback
        {
        public:
            _filestream_callback_read(_In_ _file_info *info, pplx::task_completion_event<size_t> op) : m_op(op), m_info(info) { }

            virtual void on_completed(size_t result)
            {
                result = result / sizeof(_CharType);
                m_info->m_rdpos += result;
                m_op.set(result);
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            _file_info *m_info;
            pplx::task_completion_event<size_t> m_op;
        };

        class _filestream_callback_putc : public details::_filestream_callback
        {
        public:
            _filestream_callback_putc(_In_ _file_info *info, pplx::task_completion_event<int_type> op, _CharType ch) : m_info(info), m_ch(ch), m_op(op) { }

            virtual void on_completed(size_t result)
            {
                if ( result == sizeof(_CharType) )
                {
                    m_op.set(m_ch);   
                }
                else
                {
                    m_op.set(traits::eof());
                }
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            _file_info *m_info;
            pplx::task_completion_event<int_type> m_op;
            int_type             m_ch;
        };

        class _filestream_callback_bumpc : public details::_filestream_callback
        {
        public:
            _filestream_callback_bumpc(_In_ _file_info *info, pplx::task_completion_event<int_type> op) : m_op(op), m_ch(0), m_info(info) { }

            virtual void on_completed(size_t result)
            {
                if ( result == sizeof(_CharType) )
                {
                    m_info->m_rdpos += 1;
                    m_op.set(m_ch);   
                }
                else
                {
                    m_op.set(traits::eof());
                }
                delete this;
            }

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

            int_type           m_ch;

        private:
            _file_info *m_info;
            pplx::task_completion_event<int_type> m_op;
        };

        class _filestream_callback_getc : public details::_filestream_callback
        {
        public:
            _filestream_callback_getc(_In_ _file_info *info, pplx::task_completion_event<int_type> op) : m_op(op), m_ch(0), m_info(info) { }

            virtual void on_completed(size_t result)
            {
                if ( result == sizeof(_CharType) )
                {
                    m_op.set(m_ch);   
                }
                else
                {
                    m_op.set(traits::eof());
                }
                delete this;
            }

            int_type           m_ch;

            virtual void on_error(const std::exception_ptr & e)
            {
                m_op.set_exception(e);
                delete this;
            }

        private:
            _file_info *m_info;
            pplx::task_completion_event<int_type> m_op;
        };
#pragma endregion

        _file_info *m_info;
    };

    } // namespace details

    /// <summary>
    /// Stream buffer for file streams.
    /// </summary>
    template<typename _CharType>
    class file_buffer
    {
    public:
#if !defined(__cplusplus_winrt)
        /// <summary>
        /// Open a new stream buffer representing the given file.
        /// </summary>
        /// <param name="file_name">The name of the file</param>
        /// <param name="mode">The opening mode of the file</param>
        /// <param name="prot">The file protection mode</param>
        /// <returns>An opened stream buffer.</returns>
        static pplx::task<streambuf<_CharType>> open(
            const utility::string_t &file_name,
            std::ios_base::openmode mode = std::ios_base::out,
#ifdef _MS_WINDOWS
            int prot = _SH_DENYRD
#else
            int prot = 0 // unsupported on Linux
#endif
            )
        {
            auto bfb = details::basic_file_buffer<_CharType>::open(file_name, mode, prot);
            return bfb.then([](pplx::task<std::shared_ptr<details::basic_streambuf<_CharType>>> op) -> streambuf<_CharType>
                {
                    return streambuf<_CharType>(op.get());
                });
        }
#else
        /// <summary>
        /// Open a new stream buffer representing the given file.
        /// </summary>
        /// <param name="file">The StorageFile instance</param>
        /// <param name="mode">The opening mode of the file</param>
        /// <param name="prot">The file protection mode</param>
        /// <returns>An opened stream buffer.</returns>
        static pplx::task<streambuf<_CharType>> open(
            ::Windows::Storage::StorageFile^ file,
            std::ios_base::openmode mode = std::ios_base::out)
        {
            auto bfb = details::basic_file_buffer<_CharType>::open(file, mode);
            return bfb.then([](pplx::task<std::shared_ptr<details::basic_streambuf<_CharType>>> op) -> streambuf<_CharType>
                {
                    return streambuf<_CharType>(op.get());
                });
        }
#endif
    };


    /// <summary>
    /// File stream class containing factory functions for file streams.
    /// </summary>
    template<typename _CharType>
    class file_stream
    {
    public:
       
#if !defined(__cplusplus_winrt)
        /// <summary>
        /// Open a new input stream representing the given file.
        /// The file should already exist on disk, or an exception will be thrown.
        /// </summary>
        /// <param name="file_name">The name of the file</param>
        /// <param name="mode">The opening mode of the file</param>
        /// <param name="prot">The file protection mode</param>
        /// <returns>An opened stream.</returns>
        static pplx::task<streams::basic_istream<_CharType>> open_istream(
            const utility::string_t &file_name, 
            std::ios_base::openmode mode = std::ios_base::in,
#ifdef _MS_WINDOWS
            int prot = (int) std::ios_base::_Openprot
#else
            int prot = 0
#endif
            )
        {
            mode |= std::ios_base::in;
            return streams::file_buffer<_CharType>::open(file_name, mode, prot)
                .then([](streams::streambuf<_CharType> buf) -> basic_istream<_CharType>
                {
                    return basic_istream<_CharType>(buf);
                });
        }

        /// <summary>
        /// Open a new ouput stream representing the given file.
        /// If the file does not exist, it will be create unless the folder or directory
        /// where it is to be found also does not exist.
        /// </summary>
        /// <param name="file_name">The name of the file</param>
        /// <param name="mode">The opening mode of the file</param>
        /// <param name="prot">The file protection mode</param>
        /// <returns>An opened stream.</returns>
        static pplx::task<streams::basic_ostream<_CharType>> open_ostream(
            const utility::string_t &file_name, 
            std::ios_base::openmode mode = std::ios_base::out,
#ifdef _MS_WINDOWS
            int prot = (int) std::ios_base::_Openprot
#else
            int prot = 0
#endif
            )
        {
            mode |= std::ios_base::out;
            return streams::file_buffer<_CharType>::open(file_name, mode, prot)
                .then([](streams::streambuf<_CharType> buf) -> basic_ostream<_CharType>
                {
                    return basic_ostream<_CharType>(buf);
                });
        }
#else
        /// <summary>
        /// Open a new input stream representing the given file.
        /// The file should already exist on disk, or an exception will be thrown.
        /// </summary>
        /// <param name="file">The StorageFile reference representing the file</param>
        /// <param name="mode">The opening mode of the file</param>
        /// <returns>An opened stream.</returns>
        static pplx::task<streams::basic_istream<_CharType>> open_istream(
            ::Windows::Storage::StorageFile^ file, 
            std::ios_base::openmode mode = std::ios_base::in)
        {
            mode |= std::ios_base::in;
            return streams::file_buffer<_CharType>::open(file, mode)
                .then([](streams::streambuf<_CharType> buf) -> basic_istream<_CharType>
                {
                    return basic_istream<_CharType>(buf);
                });
        }

        /// <summary>
        /// Open a new ouput stream representing the given file.
        /// If the file does not exist, it will be create unless the folder or directory
        /// where it is to be found also does not exist.
        /// </summary>
        /// <param name="file">The StorageFile reference representing the file</param>
        /// <param name="mode">The opening mode of the file</param>
        /// <returns>An opened stream.</returns>
        static pplx::task<streams::basic_ostream<_CharType>> open_ostream(
            ::Windows::Storage::StorageFile^ file, 
            std::ios_base::openmode mode = std::ios_base::out)
        {
            mode |= std::ios_base::out;
            return streams::file_buffer<_CharType>::open(file, mode)
                .then([](streams::streambuf<_CharType> buf) -> basic_ostream<_CharType>
                {
                    return basic_ostream<_CharType>(buf);
                });
        }
#endif
    };

    typedef file_stream<uint8_t> fstream;
}} // namespace concurrency::streams

#pragma warning(pop) // 4100
#endif  /* _CASA_FILE_STREAMS_H */
