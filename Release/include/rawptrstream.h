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
* rawptrstream.h
*
* This file defines a stream buffer that is based on a raw pointer and block size. Unlike a vector-based
* stream buffer, the buffer cannot be expanded or contracted, it has a fixed capacity.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_RAWPTR_STREAMS_
#define _CASA_RAWPTR_STREAMS_

#include <vector>
#include <queue>
#include <algorithm>
#include <iterator>
#include "pplxtasks.h"
#include "astreambuf.h"
#include "streams.h"
#ifdef _MS_WINDOWS
#include <safeint.h>
#endif

#ifndef _CONCRT_H
#ifndef _LWRCASE_CNCRRNCY
#define _LWRCASE_CNCRRNCY
// Note to reader: we're using lower-case namespace names everywhere, but the 'Concurrency' namespace
// is capitalized for historical reasons. The alias let's us pretend that style issue doesn't exist.
namespace Concurrency { }
namespace concurrency = Concurrency;
#endif
#endif

// Suppress unreferenced formal parameter warning as they are required for documentation
#pragma warning(push)
#pragma warning(disable : 4100)

namespace Concurrency { namespace streams {

    // Forward declarations
    template <typename _CharType> class rawptr_buffer;

    namespace details {

    /// <summary>
    /// The basic_rawptr_buffer class serves as a memory-based steam buffer that supports both writing and reading
    /// sequences of characters to and from a fixed-size block.
    /// </summary>
    template<typename _CharType>
    class basic_rawptr_buffer : public streams::details::streambuf_state_manager<_CharType>
    {
    public:
        typedef _CharType char_type;

        typedef typename basic_streambuf<_CharType>::traits traits;
        typedef typename basic_streambuf<_CharType>::int_type int_type;
        typedef typename basic_streambuf<_CharType>::pos_type pos_type;
        typedef typename basic_streambuf<_CharType>::off_type off_type;

        /// <summary>
        /// Constructor
        /// </summary>
        basic_rawptr_buffer() 
            : streambuf_state_manager<_CharType>(std::ios_base::in | std::ios_base::out),
              m_data(nullptr),
              m_current_position(0),
              m_size(0)
        {
        }

        /// <summary>
        /// Destructor
        /// </summary>
        virtual ~basic_rawptr_buffer()
        { 
            this->_close_read();
            this->_close_write();
        }

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
        virtual size_t buffer_size(std::ios_base::openmode = std::ios_base::in) const
        {
            return 0;
        }

        /// <summary>
        /// Set the stream buffer implementation to buffer or not buffer.
        /// </summary>
        /// <param name="size">The size to use for internal buffering, 0 if no buffering should be done.</param>
        /// <param name="direction">The direction of buffering (in or out)</param>
        /// <remarks>An implementation that does not support buffering will silently ignore calls to this function and it will not have
        ///          any effect on what is returned by subsequent calls to buffer_size().</remarks>
        virtual void set_buffer_size(size_t , std::ios_base::openmode = std::ios_base::in) 
        {
            return;
        }

        /// <summary>
        /// For any input stream, in_avail returns the number of characters that are immediately available
        /// to be consumed without blocking. May be used in conjunction with sbumpc() and sgetn() to 
        /// read data without incurring the overhead of using tasks.
        /// </summary>
        virtual size_t in_avail() const
        {
            // See the comment in seek around the restiction that we do not allow read head to 
            // seek beyond the current size.
            _PPLX_ASSERT(m_current_position <= m_size);
#ifdef _MS_WINDOWS
            msl::utilities::SafeInt<size_t> readhead(m_current_position);
            msl::utilities::SafeInt<size_t> writeend(m_size);
            return (size_t)(writeend - readhead); 
#else
            return m_size - m_current_position;
#endif
        }

        /// <summary>
        /// can_read is used to determine whether a stream buffer will support read operations (get).
        /// </summary>
        virtual bool can_read() const
        {
            return streambuf_state_manager<_CharType>::can_read() && m_current_position < m_size;
        }

        /// <summary>
        /// Close the stream buffer, preventing further reade or write operations.
        /// </summary>
        virtual pplx::task<bool> close(std::ios_base::openmode mode)
        {
            bool readClosed = false;
            bool writeClosed = false;

            if (mode & std::ios_base::in)
            {
                readClosed = this->_close_read().get(); // Safe to call get() here.
            }

            if (mode & std::ios_base::out)
            {
                writeClosed = this->_close_write().get(); // Safe to call get() here.
            }

            if ( !this->can_read() && !this->can_write() )
            {
                m_data = nullptr;
            }

            // We will return true if either of the read or write heads were closed
            return pplx::task_from_result(readClosed || writeClosed);
        }

        virtual pplx::task<bool> _sync()
        {
            return pplx::task_from_result(true);
        }

        virtual pplx::task<int_type> _putc(_CharType ch)
        {
            int_type retVal = (this->write(&ch, 1) == 1) ? static_cast<int_type>(ch) : traits::eof();
            return pplx::task_from_result<int_type>(retVal);
        }

        virtual pplx::task<size_t> _putn(const _CharType *ptr, size_t count)
        {
            return pplx::task_from_result<size_t>(this->write(ptr, count));
        }

        _CharType* alloc(size_t count)
        {
            if (!this->can_write()) return nullptr;

            _PPLX_ASSERT(m_current_position <= m_size);

#ifdef _MS_WINDOWS
            msl::utilities::SafeInt<size_t> readhead(m_current_position);
            msl::utilities::SafeInt<size_t> writeend(m_size);
            size_t space_left = (size_t)(writeend - readhead); 
#else
            size_t space_left = m_size - m_current_position;
#endif
            if (space_left < count) return nullptr;

            // Let the caller copy the data
            return (_CharType*)(m_data+m_current_position);
        }

        void commit(size_t actual)
        {
            // Update the write position and satisfy any pending reads
            update_current_position(m_current_position+actual);
        }

        virtual bool acquire(_CharType*& ptr, size_t& count)
        {
            if (!can_read()) return false;

            count = in_avail();

            if (count > 0)
            {
                ptr = (_CharType*)(m_data+m_current_position);
                return true;
            }
            else
            {
                ptr = nullptr;

                // If the in_avail is 0, we want to return 'false' if the stream buffer
                // is still open, to indicate that a subsequent attempt could
                // be successful. If we return true, it will indicate that the end
                // of the stream has been reached.
                return !this->is_open(); 
            }
        }

        virtual void release(_Out_writes_ (count) _CharType *, _In_ size_t count)
        {
            update_current_position(m_current_position + count);
        }

        virtual pplx::task<size_t> _getn(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
        {
            return pplx::task_from_result(this->read(ptr, count));
        }

        size_t _sgetn(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
        { 
            return this->read(ptr, count);
        }

        virtual size_t _scopy(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
        {
            return this->read(ptr, count, false);
        }

        virtual pplx::task<int_type> _bumpc()
        {
            return pplx::task_from_result(this->read_byte(true));
        }
        
        virtual int_type _sbumpc()
        {
            return this->read_byte(true);
        }

        virtual pplx::task<int_type> _getc()
        {
            return pplx::task_from_result(this->read_byte(false));
        }
        
        int_type _sgetc()
        {
            return this->read_byte(false);
        }

        virtual pplx::task<int_type> _nextc()
        {
            if (m_current_position >= m_size-1) 
                return pplx::task_from_result(basic_streambuf<_CharType>::traits::eof());

            this->read_byte(true);
            return pplx::task_from_result(this->read_byte(false));
        }
        
        virtual pplx::task<int_type> _ungetc()
        {
            auto pos = seekoff(-1, std::ios_base::cur, std::ios_base::in);
            if ( pos == (pos_type)traits::eof())
                return pplx::task_from_result(traits::eof());
            return this->getc();
        }

        virtual pos_type seekpos(pos_type position, std::ios_base::openmode mode)
        {
            pos_type beg(0);
            pos_type end(m_size);

            if (position >= beg)
            {
                auto pos = static_cast<size_t>(position);

                // Read head
                if ((mode & std::ios_base::in) && this->can_read()) 
                {
                    if (position <= end)
                    {
                        // We do not allow reads to seek beyond the end or before the start position.
                        update_current_position(pos);
                        return static_cast<pos_type>(m_current_position);
                    }
                }

                // Write head
                if ((mode & std::ios_base::out) && this->can_write()) 
                {
                    // Update write head and satisfy read requests if any
                    update_current_position(pos);

                    return static_cast<pos_type>(m_current_position);
                }
            }

            return static_cast<pos_type>(traits::eof());
        }
        
        virtual pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode) 
        {
            pos_type beg = 0;
            pos_type cur = static_cast<pos_type>(m_current_position);
            pos_type end = static_cast<pos_type>(m_size);

            switch ( way )
            {
            case std::ios_base::beg:
                return seekpos(beg + offset, mode);

            case std::ios_base::cur:
                return seekpos(cur + offset, mode);

            case std::ios_base::end:
                return seekpos(end + offset, mode);

            default:
                return static_cast<pos_type>(traits::eof());
            }
        }

    private:
        template<typename _CharType1> friend class ::concurrency::streams::rawptr_buffer;

        /// <summary>
        /// Constructor
        /// </summary>
        basic_rawptr_buffer(const _CharType* data, size_t size) 
            : streambuf_state_manager<_CharType>(std::ios_base::in),
              m_data(const_cast<_CharType*>(data)),
              m_current_position(0),
              m_size(size)
        {
            validate_mode(std::ios_base::in);
        }

        /// <summary>
        /// Constructor
        /// </summary>
        basic_rawptr_buffer(_CharType* data, size_t size, std::ios_base::openmode mode) 
            : streambuf_state_manager<_CharType>(mode),
              m_data(data),
              m_current_position(0),
              m_size(size)
        {
            validate_mode(mode);
        }

        static void validate_mode(std::ios_base::openmode mode)
        {
            // Disallow simultaneous use of the stream buffer for writing and reading.
            if ((mode & std::ios_base::in) && (mode & std::ios_base::out))
                throw std::invalid_argument("this combination of modes on raw pointer stream not supported");
        }

        /// <summary>
        /// Determine if the request can be satisfied.
        /// </summary>
        bool can_satisfy(size_t count)
        {
            // We can always satisfy a read, at least partially, unless the
            // read position is at the very end of the buffer.
            return (in_avail() > 0);
        }

        /// <summary>
        /// Reads a byte from the stream and returns it as int_type.
        /// Note: This routine must only be called if can_satisfy() returns true.
        /// </summary>
        int_type read_byte(bool advance = true)
        {
            _CharType value;
            auto read_size = this->read(&value, 1, advance);
            return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
        }

        /// <summary>
        /// Reads up to count characters into ptr and returns the count of characters copied.
        /// The return value (actual characters copied) could be <= count.
        /// Note: This routine must only be called if can_satisfy() returns true.
        /// </summary>
        size_t read(_Out_writes_ (count) _CharType *ptr, _In_ size_t count, bool advance = true)
        {
            if (!can_satisfy(count))
                return 0;

            SafeSize request_size(count);
            SafeSize read_size = request_size.Min(in_avail());

            size_t newPos = m_current_position + read_size;

            auto readBegin = m_data + m_current_position;
            auto readEnd = m_data + newPos;
            
#ifdef _MS_WINDOWS
            // Avoid warning C4996: Use checked iterators under SECURE_SCL
            std::copy(readBegin, readEnd, stdext::checked_array_iterator<_CharType *>(ptr, count));
#else
            std::copy(readBegin, readEnd, ptr);
#endif // _MS_WINDOWS

            if (advance)
            {
                update_current_position(newPos);
            }

            return (size_t) read_size;
        }

        /// <summary>
        /// Write count characters from the ptr into the stream buffer
        /// </summary>
        size_t write(const _CharType *ptr, size_t count)
        {
            if (!this->can_write() || (count == 0)) return 0;

            size_t newSize = m_current_position + count;

            // Copy the data
#ifdef _MS_WINDOWS
            // Avoid warning C4996: Use checked iterators under SECURE_SCL
            std::copy(ptr, ptr + count, stdext::checked_array_iterator<_CharType *>(m_data, m_size, m_current_position));
#else
            std::copy(ptr, ptr + count, m_data+m_current_position);
#endif // _MS_WINDOWS

            // Update write head and satisfy pending reads if any
            update_current_position(newSize);

            return count;
        }

        /// <summary>
        /// Updates the current read or write position
        /// </summary>
        void update_current_position(size_t newPos)
        {
            // The new write head
            m_current_position = newPos;

            _PPLX_ASSERT(m_current_position <= m_size);
        }

        // The actual memory block
        _CharType* m_data;

        // The size of the memory block
        size_t m_size;

        // Read/write head
        size_t m_current_position;
    };

    } // namespace details

    /// <summary>
    /// The rawptr_buffer class serves as a memory-based stream buffer that supports reading
    /// sequences of characters to and from a fixed-size block.
    /// </summary>
    template<typename _CharType>
    class rawptr_buffer : public streambuf<_CharType>
    {
    public:
        typedef _CharType char_type;

        /// <summary>
        /// Create a rawptr_buffer given a pointer to a memory block and the size of the block.
        /// </summary>
        /// <param name="data">The address (pointer to) the memory block.</param>
        /// <param name="size">The memory block size.</param>
        rawptr_buffer(const char_type* data, size_t size) 
            : streambuf<char_type>(std::shared_ptr<details::basic_rawptr_buffer<char_type>>(new details::basic_rawptr_buffer<char_type>(data, size)))
        {
        }

        /// <summary>
        /// Create a rawptr_buffer given a pointer to a memory block and the size of the block.
        /// </summary>
        /// <param name="data">The address (pointer to) the memory block.</param>
        /// <param name="size">The memory block size.</param>
        rawptr_buffer(char_type* data, size_t size, std::ios_base::openmode mode = std::ios::out) 
            : streambuf<char_type>(std::shared_ptr<details::basic_rawptr_buffer<char_type>>(new details::basic_rawptr_buffer<char_type>(data, size, mode)))
        {
        }

        /// <summary>
        /// Default constructor.
        /// </summary>
        rawptr_buffer()
        {
        }
    };   

    /// <summary>
    /// The rawptr_stream class is used to create memory-backed streams that support both writing and reading
    /// sequences of characters to and from a fixed-size block.
    /// </summary>
    template<typename _CharType>
    class rawptr_stream 
    {
    public:
        typedef _CharType char_type;
        typedef rawptr_buffer<_CharType>  buffer_type;

        /// <summary>
        /// Create a rawptr-stream given a pointer to a read-only memory block and the size of the block.
        /// </summary>
        /// <param name="data">The address (pointer to) the memory block.</param>
        /// <param name="size">The memory block size.</param>
        static concurrency::streams::basic_istream<char_type> open_istream(const char_type* data, size_t size)
        {
            return concurrency::streams::basic_istream<char_type>(buffer_type(data, size));
        }

        /// <summary>
        /// Create a rawptr-stream given a pointer to a writable memory block and the size of the block.
        /// </summary>
        /// <param name="data">The address (pointer to) the memory block.</param>
        /// <param name="size">The memory block size.</param>
        static concurrency::streams::basic_istream<char_type> open_istream(char_type* data, size_t size)
        {
            return concurrency::streams::basic_istream<char_type>(buffer_type(data, size, std::ios::in));
        }

        /// <summary>
        /// Create a rawptr-stream given a pointer to a writable memory block and the size of the block.
        /// </summary>
        /// <param name="data">The address (pointer to) the memory block.</param>
        /// <param name="size">The memory block size.</param>
        static concurrency::streams::basic_ostream<char_type> open_ostream(char_type* data, size_t size)
        {
            return concurrency::streams::basic_ostream<char_type>(buffer_type(data, size, std::ios::out));
        }
    }; 

}} // namespaces

#pragma warning(pop) // 4100
#endif  /* _CASA_RAWPTR_STREAMS_ */
