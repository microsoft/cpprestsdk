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
* astreambuf.h
*
* Asynchronous I/O: stream buffer. This is an extension to the PPL concurrency features and therefore
* lives in the Concurrency namespace.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include <ios>
#include <memory>
#include <cstring>
#include <math.h>
#include <pplxtasks.h>
#include "basic_types.h"
#include "asyncrt_utils.h"

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

#ifndef _MS_WINDOWS
// TFS 579628 - 1206: figure out how to avoid having this specialization for Linux (beware of 64-bit Linux)
namespace std {
    template<>
    struct char_traits<unsigned char> : private char_traits<char>
    {
    public:
        typedef unsigned char char_type;

        using char_traits<char>::eof;
        using char_traits<char>::int_type;
        using char_traits<char>::off_type;
        using char_traits<char>::pos_type;

        static size_t length(const unsigned char* str)
        {
            return char_traits<char>::length(reinterpret_cast<const char*>(str));
        }

        static void assign(unsigned char& left, const unsigned char& right) { left = right; }
        static unsigned char* assign(unsigned char* left, size_t n, unsigned char value)
        {
            return reinterpret_cast<unsigned char*>(char_traits<char>::assign(reinterpret_cast<char*>(left), n, static_cast<char>(value)));
        }

        static unsigned char* copy(unsigned char* left, const unsigned char* right, size_t n)
        {
            return reinterpret_cast<unsigned char*>(char_traits<char>::copy(reinterpret_cast<char*>(left), reinterpret_cast<const char*>(right), n));
        }

        static unsigned char* move(unsigned char* left, const unsigned char* right, size_t n)
        {
            return reinterpret_cast<unsigned char*>(char_traits<char>::move(reinterpret_cast<char*>(left), reinterpret_cast<const char*>(right), n));
        }
    };
}
#endif // _MS_WINDOWS

namespace Concurrency { namespace streams
{
    /// <summary>
    /// Extending the standard char_traits type with one that adds values and types
    /// that are unique to casablanca streams.
    /// </summary>
    template<typename _CharType>
    struct char_traits : std::char_traits<_CharType>
    {
        /// <summary>
        /// Some synchronous functions will return this value if the operation
        /// requires an asynchronous call in a given situation.
        /// </summary>
        static typename std::char_traits<_CharType>::int_type requires_async() { return std::char_traits<_CharType>::eof()-1; }
    };

    namespace details {

    /// <summary>
    /// Stream buffer base class.
    /// </summary>
    template<typename _CharType>
    class basic_streambuf
    {
    public:
        typedef _CharType char_type;
        typedef ::concurrency::streams::char_traits<_CharType> traits;

        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;


        /// <summary>
        /// Virtual constructor for stream buffers.
        /// </summary>
        virtual ~basic_streambuf() { }

        /// <summary>
        /// can_read is used to determine whether a stream buffer will support read operations (get).
        /// </summary>
        virtual bool can_read() const = 0;

        /// <summary>
        /// can_write is used to determine whether a stream buffer will support write operations (put).
        /// </summary>       
        virtual bool can_write() const = 0;
        
        /// <summary>
        /// can_seek is used to determine whether a stream buffer supports seeking.
        /// </summary>
        virtual bool can_seek() const = 0;

        /// <summary>
        /// is_eof is used to determine whether a read head has reached the buffer.
        /// </summary>
        virtual bool is_eof() const = 0;

        /// <summary>
        /// Get the stream buffer size, if one has been set.
        /// </summary>
        /// <param name="direction">The direction of buffering (in or out)</param>
        /// <returns>The size of the internal buffer (for the given direction).</returns>
        /// <remarks>An implementation that does not support buffering will always return 'false'.</remarks>
        virtual size_t buffer_size(std::ios_base::openmode direction = std::ios_base::in) const = 0;

        /// <summary>
        /// Set the stream buffer implementation to buffer or not buffer.
        /// </summary>
        /// <param name="size">The size to use for internal buffering, 0 if no buffering should be done.</param>
        /// <param name="direction">The direction of buffering (in or out)</param>
        /// <remarks>An implementation that does not support buffering will silently ignore calls to this function and it will not have
        ///          any effect on what is returned by subsequent calls to buffer_size().</remarks>
        virtual void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in) = 0;

        /// <summary>
        /// For any input stream, in_avail returns the number of characters that are immediately available
        /// to be consumed without blocking. May be used in conjunction with sbumpc() to read data without
        /// incurring the overhead of using tasks.
        /// </summary>
        virtual size_t in_avail() const = 0;

        /// <summary>
        /// Is the stream buffer open?
        /// </summary>
        /// <remarks>No separation is made between open for reading and open for writing.</remarks>
        virtual bool is_open() const = 0;

        /// <summary>
        /// Close the stream buffer, preventing further reade or write operations.
        /// </summary>
        virtual pplx::task<bool> close(std::ios_base::openmode mode = (std::ios_base::in | std::ios_base::out)) = 0;


        /// <summary>
        /// Close the stream buffer with an exception.
        /// </summary>
        /// <param name="eptr"> Pointer to the exception. </param>
        virtual pplx::task<bool> close(std::ios_base::openmode mode, std::exception_ptr eptr) = 0;


        /// <summary>
        /// Write a single byte to an output stream.
        /// </summary>
        /// <param name="ch">The byte to write</param>
        /// <returns>The value of the byte. EOF if the write operation fails</returns>
        virtual pplx::task<int_type> putc(_CharType ch) = 0;

        /// <summary>
        /// Write a number of characters to the stream.
        /// </summary>
        /// <param name="ptr">A pointer to the block of data to be written.</param>
        /// <param name="count">The number of characters to write.</param>
        /// <returns>The number of characters actually written, which may be less than what was requested. EOF if write the operation fails.</returns>
        virtual pplx::task<size_t> putn(const _CharType *ptr, size_t count) = 0;

        /// <summary>
        /// Read a single byte from the stream and advance the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails.</returns>
        virtual pplx::task<int_type> bumpc() = 0;

        /// <summary>
        /// Read a single byte from the stream and advance the read position.
        /// </summary>
        /// <returns>The value of the byte. -1 if the read fails. -2 if an asynchronous read is required</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        virtual int_type sbumpc() = 0;

        /// <summary>
        /// Read a single byte from the stream without advancing the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails.</returns>
        virtual pplx::task<int_type> getc() = 0;

        /// <summary>
        /// Read a single byte from the stream without advancing the read position.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails. 'requires_async' if an asynchronous read is required</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        virtual int_type sgetc() = 0;

        /// <summary>
        /// Advance the read position, then return the next character without advancing again.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails.</returns>
        virtual pplx::task<int_type> nextc() = 0;

        /// <summary>
        /// Retreat the read position, then return the current character without advancing.
        /// </summary>
        /// <returns>The value of the byte. EOF if the read fails. 'requires_async' if an asynchronous read is required</returns>
        virtual pplx::task<int_type> ungetc() = 0;

        /// <summary>
        /// Read up to a given number of characters from the stream.
        /// </summary>
        /// <param name="ptr">The address of the target memory area</param>
        /// <param name="count">The maximum number of characters to read</param>
        /// <returns>The number of characters read. O if the end of the stream is reached.</returns>
        virtual pplx::task<size_t> getn(_Out_writes_(count) _CharType *ptr, _In_ size_t count) = 0;

        /// <summary>
        /// Copy up to a given number of characters from the stream, synchronously.
        /// </summary>
        /// <param name="ptr">The address of the target memory area</param>
        /// <param name="count">The maximum number of characters to read</param>
        /// <returns>The number of characters copy. O if the end of the stream is reached or an asynchronous read is required.</returns>
        /// <remarks>This is a synchronous operation, but is guaranteed to never block.</remarks>
        virtual size_t scopy(_Out_writes_(count) _CharType *ptr, _In_ size_t count) = 0;

        /// <summary>
        /// Seek to the given position.
        /// </summary>
        /// <param name="pos">The offset from the beginning of the stream</param>
        /// <param name="direction">The I/O direction to seek (see remarks)</param>
        /// <returns>The position. EOF if the operation fails.</returns>
        /// <remarks>Some streams may have separate write and read cursors. 
        ///          For such streams, the direction parameter defines whether to move the read or the write cursor.</remarks>
        virtual pos_type seekpos(pos_type pos, std::ios_base::openmode direction) = 0; 

        /// <summary>
        /// Seek to a position given by a relative offset.
        /// </summary>
        /// <param name="offset">The relative position to seek to</param>
        /// <param name="way">The starting point (beginning, end, current) for the seek.</param>
        /// <param name="mode">The I/O direction to seek (see remarks)</param>
        /// <returns>The position. EOF if the operation fails.</returns>
        /// <remarks>Some streams may have separate write and read cursors. 
        ///          For such streams, the mode parameter defines whether to move the read or the write cursor.</remarks>
        virtual pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode) = 0;     

        /// <summary>
        /// For output streams, flush any internally buffered data to the underlying medium.
        /// </summary>
        /// <returns>true if the flush succeeds, false if not</returns>
        virtual pplx::task<bool> sync() = 0;

        //
        // Efficient read and write.
        //
        // The following routines are intended to be used for more efficient, copy-free, reading and
        // writing of data from/to the stream. Rather than having the caller provide a buffer into which
        // data is written or from which it is read, the stream buffer provides a pointer directly to the
        // internal data blocks that it is using. Since not all stream buffers use internal data structures
        // to copy data, the functions may not be supported by all. An application that wishes to use this
        // functionality should therefore first try them and check for failure to support. If there is
        // such failure, the application should fall back on the copying interfaces (putn / getn)
        //

        /// <summary>
        /// Allocate a contiguous memory block and return it.
        /// </summary>
        /// <param name="count">The number of characters to allocate.</param>
        /// <returns>A pointer to a block to write to, null if the stream buffer implementation does not support alloc/commit.</returns>
        virtual _CharType* alloc(size_t count) = 0;

        /// <summary>
        /// Submit a block already allocated by the stream buffer.
        /// </summary>
        /// <param name="count">The count of data to be committed.</param>
        virtual void commit(size_t count) = 0;

        /// <summary>
        /// Get a pointer to the next already allocated contiguous block of data. 
        /// </summary>
        /// <param name="ptr">A reference to a pointer variable that will hold the address of the block on success.</param>
        /// <param name="count">The number of contiguous characters available at the address in 'ptr.'</param>
        /// <returns>True if the operation suceeded, false otherwise.</returns>
        /// <remarks>
        /// A return of false does not necessarily indicate that a subsequent read operation would fail, only that
        /// there is no block to return immediately or that the stream buffer does not support the operation.
        /// The stream buffer may not de-allocate the block until 'release' is called.
        /// If the end of the stream is reached, the function will return 'true', a null pointer, and a count of zero;
        /// a subsequent read will not succeed.
        /// </remarks>
        virtual bool acquire(_Out_writes_(count) _CharType*& ptr, _In_ size_t& count) = 0;

        /// <summary>
        /// Release a block of data acquired using 'acquire().' This frees the stream buffer to de-allocate the
        /// memory, if it so desires. Move the read position ahead by the count
        /// </summary>
        /// <param name="ptr">A pointer to the block of data to be released.</param>
        /// <param name="count">The number of characters that were read.</param>
        virtual void release(_Out_writes_(count) _CharType *ptr, _In_ size_t count) = 0;

        /// <summary>
        /// Retrieve the stream buffer exception_ptr if it has been set.
        /// </summary>
        /// <returns> Pointer to the exception, if it has been set; otherwise, nullptr will be returned</returns>
        virtual std::exception_ptr exception() const = 0;
    };


    template<typename _CharType>
    class streambuf_state_manager : public basic_streambuf<_CharType>, public std::enable_shared_from_this<streambuf_state_manager<_CharType>>
    {
    public:
        typedef typename details::basic_streambuf<_CharType>::traits traits;
        typedef typename details::basic_streambuf<_CharType>::int_type int_type;
        typedef typename details::basic_streambuf<_CharType>::pos_type pos_type;
        typedef typename details::basic_streambuf<_CharType>::off_type off_type;


        virtual bool can_read() const 
        { 
            return m_stream_can_read; 
        }
   
        virtual bool can_write() const 
        { 
            return m_stream_can_write; 
        }

        virtual bool is_open() const
        {
            return can_read() || can_write(); 
        }

        virtual pplx::task<bool> close(std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        {
            pplx::task<bool> readClosed = pplx::task_from_result(false), writeClosed = readClosed;

            if (mode & std::ios_base::in && can_read())
                readClosed = _close_read();

            if (mode & std::ios_base::out && can_write())
                writeClosed = _close_write();

            // We will return true if read or write operations succeeded
            return (readClosed && writeClosed).then([](std::vector<bool> res) { return res[0] || res[1]; });
        }

        virtual pplx::task<bool> close(std::ios_base::openmode mode, std::exception_ptr eptr)
        {
            if (m_currentException == nullptr)
                m_currentException = eptr;
            return close(mode);
        }

        virtual bool is_eof() const
        {
            return m_stream_read_eof;
        }

        virtual pplx::task<int_type> putc(_CharType ch)
        {
            if (!can_write())
                return create_exception_checked_value_task<int_type>(traits::eof());

            return create_exception_checked_task<int_type>(_putc(ch), [](int_type) {
                return false; // no EOF for write
            });
        }

        virtual pplx::task<size_t> putn(const _CharType *ptr, size_t count)
        {
            if (!can_write()) 
                return create_exception_checked_value_task<size_t>(0);
            if (count == 0)
                return pplx::task_from_result<size_t>(0);

            return create_exception_checked_task<size_t>(_putn(ptr, count), [](size_t) {
                return false; // no EOF for write
            });
        }

        virtual pplx::task<int_type> bumpc()
        {
            if (!can_read())
                return create_exception_checked_value_task<int_type>(streambuf_state_manager<_CharType>::traits::eof());

            return create_exception_checked_task<int_type>(_bumpc(), [](int_type val) {
                return val == streambuf_state_manager<_CharType>::traits::eof();
            });
        }

        virtual int_type sbumpc()
        {
            if (!can_read())
                return traits::eof();
            return check_sync_read_eof(_sbumpc());
        }

        virtual pplx::task<int_type> getc()
        {
            if (!can_read())
                return create_exception_checked_value_task<int_type>(traits::eof());

            return create_exception_checked_task<int_type>(_getc(), [](int_type val) {
                return val == streambuf_state_manager<_CharType>::traits::eof();
            });
        }

        virtual int_type sgetc()
        {
            if (!can_read())
                return traits::eof();
            return check_sync_read_eof(_sgetc());
        }

        virtual pplx::task<int_type> nextc()
        {
            if (!can_read())
                return create_exception_checked_value_task<int_type>(traits::eof());

            return create_exception_checked_task<int_type>(_nextc(), [](int_type val) {
                return val == streambuf_state_manager<_CharType>::traits::eof();
            });
        }

        virtual pplx::task<int_type> ungetc()
        {
            if (!can_read())
                return create_exception_checked_value_task<int_type>(traits::eof());

            return create_exception_checked_task<int_type>(_ungetc(), [](int_type) {
                return false;
            });
        }

        virtual pplx::task<size_t> getn(_Out_writes_(count) _CharType *ptr, _In_ size_t count)
        {
            if (!can_read())
                return create_exception_checked_value_task<size_t>(0);
            if (count == 0)
                return pplx::task_from_result<size_t>(0);

            return create_exception_checked_task<size_t>(_getn(ptr, count), [](size_t val) {
                return val == 0;
            });			
        }

        virtual size_t scopy(_Out_writes_(count) _CharType *ptr, _In_ size_t count)
        {
            if (!can_read()) 
                return 0;
                
            return _scopy(ptr, count);
        }
            
        virtual pplx::task<bool> sync()
        {
            if (!can_write())
                return create_exception_checked_value_task(false);
            return create_exception_checked_task<bool>(_sync(), [](bool) {
                return false;
            });		
        }

        /// <summary>
        /// Retrieve the stream buffer exception_ptr if it has been set.
        /// </summary>
        /// <returns> Pointer to the exception, if it has been set; otherwise, nullptr will be returned</returns>
        virtual std::exception_ptr exception() const
        {
            return m_currentException;
        }

#pragma region dependencies
    public:
        virtual bool can_seek() const = 0;
        virtual size_t buffer_size(std::ios_base::openmode direction = std::ios_base::in) const = 0;
        virtual void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in) = 0;
        virtual size_t in_avail() const = 0;
        virtual pos_type seekpos(pos_type pos, std::ios_base::openmode direction) = 0; 
        virtual pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode) = 0;     
        virtual _CharType* alloc(size_t count) = 0;
        virtual void commit(size_t count) = 0;
        virtual bool acquire(_Out_writes_(count) _CharType*& ptr, _In_ size_t& count) = 0;
        virtual void release(_Out_writes_(count) _CharType *ptr, _In_ size_t count) = 0;
    protected:
        virtual pplx::task<int_type> _putc(_CharType ch) = 0;
        virtual pplx::task<size_t> _putn(const _CharType *ptr, size_t count) = 0;
        virtual pplx::task<int_type> _bumpc() = 0;
        virtual int_type _sbumpc() = 0;
        virtual pplx::task<int_type> _getc() = 0;
        virtual int_type _sgetc() = 0;
        virtual pplx::task<int_type> _nextc() = 0;
        virtual pplx::task<int_type> _ungetc() = 0;
        virtual pplx::task<size_t> _getn(_Out_writes_(count) _CharType *ptr, _In_ size_t count) = 0;
        virtual size_t _scopy(_Out_writes_(count) _CharType *ptr, _In_ size_t count) = 0;
        virtual pplx::task<bool> _sync() = 0;

        /// <summary>
        /// The real read head close operation, implementation should override it if there is any resource to be released.
        /// </summary>
        virtual pplx::task<bool> _close_read()
        {
            m_stream_can_read = false;
            return pplx::task_from_result(true);
        }

        /// <summary>
        /// The real write head close operation, implementation should override it if there is any resource to be released.
        /// </summary>
        virtual pplx::task<bool> _close_write()
        {
            m_stream_can_write = false;
            return pplx::task_from_result(true);
        }

#pragma endregion

    protected:
        streambuf_state_manager(std::ios_base::openmode mode)
        {
            m_stream_can_read = (mode & std::ios_base::in) != 0;
            m_stream_can_write = (mode & std::ios_base::out) != 0;
            m_stream_read_eof = false;
        }
            
        std::exception_ptr m_currentException;
        // The in/out mode for the buffer
        bool m_stream_can_read, m_stream_can_write, m_stream_read_eof;


    private:
        template<typename _CharType1>
        pplx::task<_CharType1> create_exception_checked_value_task(const _CharType1 &val) const
        {
            // TFS 579620 - 1210: make sure default-constructed exception_ptr is nullptr
            if (this->exception() == nullptr)
                return pplx::task_from_result<_CharType1>(static_cast<_CharType1>(val));
            else
                return pplx::task_from_exception<_CharType1>(this->exception());
        }

        // Set exception and eof states for async read
        template<typename _CharType1>
        pplx::task<_CharType1> create_exception_checked_task(pplx::task<_CharType1> result, std::function<bool(_CharType1)> eof_test, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        {
            auto thisPointer = this->shared_from_this();
            // Workaround VC 2010 bug
            auto func2 = [thisPointer](bool) -> _CharType1 {
                std::rethrow_exception(thisPointer->exception());
                return _CharType1();
            };

            auto func1 = [=](pplx::task<_CharType1> t1) -> pplx::task<_CharType1> {
                try {
                    thisPointer->m_stream_read_eof = eof_test(t1.get());
                } catch (...) {
                    return thisPointer->close(mode, std::current_exception()).then(func2);
                }
                if (thisPointer->m_stream_read_eof && !(thisPointer->exception() == nullptr))
                    std::rethrow_exception(thisPointer->exception());
                return t1;
            };

            if ( result.is_done() )
            {
                // If the data is already available, we should avoid scheduling a continuation, so we do it inline.
                return func1(result);
            }
            else
            {
                return result.then(func1);
            }
        }

        // Set eof states for sync read
        int_type check_sync_read_eof(int_type ch)
        {
            m_stream_read_eof = ch == traits::eof();
            return ch;
        }

    };

    } // namespace details

    // Forward declarations
    template<typename _CharType> class basic_istream;
    template<typename _CharType> class basic_ostream;

    /// <summary>
    /// Reference-counted stream buffer
    /// </summary>
    /// <remarks>
    /// The rationale for refcounting is discussed in the accompanying design
    /// documentation.
    /// </remarks>
    template<typename _CharType>
    class streambuf : public details::basic_streambuf<_CharType>
    {
    public:
        typedef typename details::basic_streambuf<_CharType>::traits    traits;
        typedef typename details::basic_streambuf<_CharType>::int_type  int_type;
        typedef typename details::basic_streambuf<_CharType>::pos_type  pos_type;
        typedef typename details::basic_streambuf<_CharType>::off_type  off_type;
        typedef typename details::basic_streambuf<_CharType>::char_type char_type;

        template <typename _CharType2> friend class streambuf;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="ptr">A pointer to the concrete stream buffer implementation</param>
        streambuf(_In_ const std::shared_ptr<details::basic_streambuf<_CharType>> &ptr) : m_buffer(ptr) {}

        /// <summary>
        /// Default constructor
        /// </summary>
        streambuf() { }

        /// <summary>
        /// Copy constructor
        /// </summary>
        /// <param name="other">The source object</param>
        streambuf(const streambuf &other) : m_buffer(other.m_buffer) { }

        /// <summary>
        /// Converter Constructor
        /// </summary>
        /// <param name="ptr">The source buffer to be converted</param>
        template <typename AlterCharType>
        streambuf(const streambuf<AlterCharType> &other) : 
            m_buffer(std::static_pointer_cast<details::basic_streambuf<_CharType>>(std::static_pointer_cast<void>(other.m_buffer)))
        {
            static_assert(std::is_same<pos_type, typename details::basic_streambuf<AlterCharType>::pos_type>::value
                && std::is_same<off_type, typename details::basic_streambuf<AlterCharType>::off_type>::value
                && std::is_integral<_CharType>::value && std::is_integral<AlterCharType>::value
                && std::is_integral<int_type>::value && std::is_integral<typename details::basic_streambuf<AlterCharType>::int_type>::value
                && sizeof(_CharType) == sizeof(AlterCharType)
                && sizeof(int_type) == sizeof(typename details::basic_streambuf<AlterCharType>::int_type),
                "incompatible stream character types");
        }

        /// <summary>
        /// Move constructor
        /// </summary>
        /// <param name="other">The source object</param>
        streambuf(streambuf &&other) : m_buffer(std::move(other.m_buffer)) { }

        /// <summary>
        /// Assignment operator
        /// </summary>
        /// <param name="other">The source object</param>
        streambuf & operator =(const streambuf &other) { m_buffer = other.m_buffer; return *this; }
       
        /// <summary>
        /// Move operator
        /// </summary>
        /// <param name="other">The source object</param>
        streambuf & operator =(streambuf &&other) { m_buffer = std::move(other.m_buffer); return *this; }

        /// <summary>
        /// Constructs an input stream head for this stream buffer.
        /// </summary>
        /// <returns>basic_istream</returns>
        concurrency::streams::basic_istream<_CharType> create_istream() const
        {
            if (!can_read()) throw std::runtime_error("stream buffer not set up for input of data");
            return concurrency::streams::basic_istream<_CharType>(*this);
        }

        /// <summary>
        /// Constructs an output stream head for this stream buffer.
        /// </summary>
        /// <returns>basic_ostream</returns>
        concurrency::streams::basic_ostream<_CharType> create_ostream() const
        {
            if (!can_write()) throw std::runtime_error("stream buffer not set up for output of data");
            return concurrency::streams::basic_ostream<_CharType>(*this);
        }

        /// <summary>
        /// Test whether the stream buffer has been initialized or not.
        /// </summary>
        operator bool() const { return (bool)m_buffer; }

        /// <summary>
        /// Destructor
        /// </summary>
        virtual ~streambuf() { }

        std::shared_ptr<details::basic_streambuf<_CharType>> get_base() const
        {
            if (!m_buffer)
            {
                throw std::invalid_argument("Invalid streambuf object");
            }

            return m_buffer;
        }

#pragma region Function forwarding

        virtual bool can_read() const { return get_base()->can_read(); }
        virtual bool can_write() const { return  get_base()->can_write(); }
        virtual bool can_seek() const { return get_base()->can_seek(); }

        virtual size_t buffer_size(std::ios_base::openmode direction = std::ios_base::in) const { return get_base()->buffer_size(direction); }

        virtual void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in) { get_base()->set_buffer_size(size,direction); }

        virtual size_t in_avail() const { return get_base()->in_avail(); }

        virtual bool is_open() const { return get_base()->is_open(); }

        virtual bool is_eof() const { return get_base()->is_eof(); }

        virtual pplx::task<bool> close(std::ios_base::openmode mode = (std::ios_base::in | std::ios_base::out))
        {
            // We preserve the check here to workaround a Dev10 compiler crash
            auto buffer = get_base();
            return buffer ? buffer->close(mode) : pplx::task_from_result(false); 
        }

        virtual pplx::task<bool> close(std::ios_base::openmode mode, std::exception_ptr eptr)
        {
            // We preserve the check here to workaround a Dev10 compiler crash
            auto buffer = get_base();
            return buffer ? buffer->close(mode, eptr) : pplx::task_from_result(false); 
        }

        virtual pplx::task<int_type> putc(_CharType ch)
        {
            return get_base()->putc(ch); 
        }

        virtual _CharType* alloc(size_t count)
        {
            return get_base()->alloc(count); 
        }

        virtual void commit(size_t count)
        {
            get_base()->commit(count);
        }

        virtual bool acquire(_Out_writes_(count) _CharType*& ptr, _In_ size_t& count)
        {
            ptr = nullptr;
            count = 0;
            return get_base()->acquire(ptr, count); 
        }

        virtual void release(_Out_writes_(count) _CharType *ptr, _In_ size_t count)
        {
            get_base()->release(ptr, count);
        }

        virtual pplx::task<size_t> putn(const _CharType *ptr, size_t count)
        {
            return get_base()->putn(ptr, count); 
        }

        virtual pplx::task<int_type> bumpc()
        {
            return get_base()->bumpc(); 
        }

        virtual typename details::basic_streambuf<_CharType>::int_type sbumpc()
        {
            return get_base()->sbumpc(); 
        }

        virtual pplx::task<int_type> getc()
        {
            return get_base()->getc(); 
        }

        virtual typename details::basic_streambuf<_CharType>::int_type sgetc()
        {
            return get_base()->sgetc(); 
        }

        pplx::task<int_type> nextc()
        {
            return get_base()->nextc();  
        }

        pplx::task<int_type> ungetc()
        {
            return get_base()->ungetc();  
        }

        virtual pplx::task<size_t> getn(_Out_writes_(count) _CharType *ptr, _In_ size_t count)
        {
            return get_base()->getn(ptr, count); 
        }

        virtual size_t scopy(_Out_writes_(count) _CharType *ptr, _In_ size_t count)
        {
            return get_base()->scopy(ptr, count); 
        }

        virtual typename details::basic_streambuf<_CharType>::pos_type seekpos(typename details::basic_streambuf<_CharType>::pos_type pos, std::ios_base::openmode direction)
        {
            return get_base()->seekpos(pos, direction); 
        }

        virtual typename details::basic_streambuf<_CharType>::pos_type seekoff(typename details::basic_streambuf<_CharType>::off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode)
        {
            return get_base()->seekoff(offset, way, mode);
        }

        virtual pplx::task<bool> sync()
        {
            return get_base()->sync();
        }

        virtual std::exception_ptr exception() const
        {
            return get_base()->exception();
        }

#pragma endregion

    private:
        std::shared_ptr<details::basic_streambuf<_CharType>> m_buffer;

    };

}}

#pragma warning(pop) // 4100
