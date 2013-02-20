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
* streams.h
*
* Asynchronous I/O: streams API, used for formatted input and output, based on unformatted I/O using stream buffers
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_STREAMS_H
#define _CASA_STREAMS_H

#include <astreambuf.h>
#include <iosfwd>
#ifdef _MS_WINDOWS
#include <safeint.h>
#endif

namespace Concurrency { namespace streams
{
    template<typename CharType> class basic_ostream;
    template<typename CharType> class basic_istream;

    namespace details {
    template<typename CharType>
        class basic_ostream_helper
        {
        public:
            basic_ostream_helper(streams::streambuf<CharType> buffer) : m_buffer(buffer) { }

            ~basic_ostream_helper() { }

        private:
            template<typename CharType1> friend class basic_ostream;

            concurrency::streams::streambuf<CharType> m_buffer;
        };

        template<typename CharType>
        class basic_istream_helper
        {
        public:
            basic_istream_helper(streams::streambuf<CharType> buffer) : m_buffer(buffer) { }

            ~basic_istream_helper() { }

        private:
            template<typename CharType1> friend class basic_istream;

            concurrency::streams::streambuf<CharType> m_buffer;
        };

        template <typename CharType>
        struct Value2StringFormatter
        {
            template <typename T>
            static std::basic_string<CharType> format(const T &val)
            {
                std::basic_ostringstream<CharType> ss;
                ss << val;
                return ss.str();
            }
        };
    
        template <>
        struct Value2StringFormatter <uint8_t>
        {
            template <typename T>
            static std::basic_string<uint8_t> format(const T &val)
            {
                std::basic_ostringstream<char> ss;
                ss << val;
                return reinterpret_cast<const uint8_t *>(ss.str().c_str());
            }

            static std::basic_string<uint8_t> format(const utf16string &val)
            {
                return format(utility::conversions::utf16_to_utf8(val));
            }

        };

    }

    template<typename CharType>
    class basic_ostream
    {
    public:

        typedef std::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;

        /// <summary>
        /// Default constructor
        /// </summary>
        basic_ostream() {}

        /// <summary>
        /// Copy constructor
        /// </summary>
        /// <param name="other">The source object</param>
        basic_ostream(const basic_ostream &other) : m_helper(other.m_helper) { }

        /// <summary>
        /// Assignment operator
        /// </summary>
        /// <param name="other">The source object</param>
        basic_ostream & operator =(const basic_ostream &other) { m_helper = other.m_helper; return *this; }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="ptr">A stream buffer</param>
        basic_ostream(streams::streambuf<CharType> buffer) : 
            m_helper(std::make_shared<details::basic_ostream_helper<CharType>>(buffer)) 
		{
			_verify_can_write();
		}

        /// <summary>
        /// Close the stream, preventing further write operations.
        /// </summary>
        pplx::task<bool> close() const
        {
            return is_valid() ?
                helper()->m_buffer.close(std::ios_base::out) : 
                pplx::task_from_result(false); 
        }

        /// <summary>
        /// Close the stream with exception, preventing further write operations.
        /// </summary>
        pplx::task<bool> close(std::exception_ptr eptr) const 
        {
            return is_valid() ? 
                helper()->m_buffer.close(std::ios_base::out, eptr) : 
                pplx::task_from_result(false); 
        }

        /// <summary>
        /// Put a single character into the stream.
        /// </summary>
        /// <param name="ch">A character</param>
        pplx::task<int_type> write(CharType ch)const 
        {
            return helper()->m_buffer.putc(ch);
        }

        /// <summary>
        /// Write a number of characters from a given stream buffer into the stream.
        /// </summary>
        /// <param name="source">A source stream buffer.</param>
        /// <param name="count">The number of characters to write.</param>
        pplx::task<size_t> write(streams::streambuf<CharType> source, size_t count) const 
        {
            auto buffer = helper()->m_buffer;

			_verify_can_write();

			if ( !source.can_read() )
				throw std::runtime_error("source buffer not set up for input of data");

            auto data = buffer.alloc(count);

            if ( data != nullptr )
            {
                auto post_read = 
                    [buffer](pplx::task<size_t> op)-> pplx::task<size_t>
                    {
                        auto b = buffer;
                        b.commit(op.get());
                        return op; 
                    };
                return source.getn(data, count).then(post_read);
            }
            else
            {
                size_t available = 0;

                if ( source.acquire(data, available) && available >= count )
                {
                    auto post_write = 
                        [source,data](pplx::task<size_t> op)-> pplx::task<size_t>
                        {
                            auto s = source;
                            s.release(data,op.get());
                            return op; 
                        };
                    return buffer.putn(data, count).then(post_write);
                }
                else
                {
                    std::shared_ptr<CharType> buf(new CharType[count]);

                    auto post_write = 
                        [buf](pplx::task<size_t> op)-> pplx::task<size_t>
                        {
                            return op; 
                        };
                    auto post_read = 
                        [buf,post_write,buffer](pplx::task<size_t> op) -> pplx::task<size_t>
                        {
                            auto b = buffer;
                            return b.putn(buf.get(), op.get()).then(post_write);
                        };

                    return source.getn(buf.get(), count).then(post_read);
                }
            }
        }

        pplx::task<size_t> print(const std::basic_string<CharType>& str) const
        {
            return (str.size() == 0) 
                ? pplx::task_from_result<size_t>((0)) 
                : streambuf().putn(str.c_str(), str.size());
        }

        template<typename T>
        pplx::task<size_t> print(const T& val) const
        {
            return print(details::Value2StringFormatter<CharType>::format(val));
        }

        /// <summary>
        /// Flush any buffered output data.
        /// </summary>
        pplx::task<bool> flush() const 
        {
			_verify_can_write();
            return helper()->m_buffer.sync();
        }

        /// <summary>
        /// Seek to the specified write position.
        /// </summary>
        /// <param name="pos">An offset relative to the beginning of the stream.</param>
        pos_type seek(pos_type pos) const 
        {
			_verify_can_write();
            return helper()->m_buffer.seekpos(pos, std::ios_base::out);
        }

        /// <summary>
        /// Seek to the specified write position.
        /// </summary>
        /// <param name="off">An offset relative to the beginning, current write position, or the end of the stream.</param>
        /// <param name="way">The starting point (beginning, current, end) for the seek.</param>
        pos_type seek(off_type off, std::ios_base::seekdir way) const
        {
			_verify_can_write();
            return helper()->m_buffer.seekoff(off, way, std::ios_base::out);
        }

        /// <summary>
        /// Get the current write position, i.e. the offset from the beginning of the stream.
        /// </summary>
        pos_type tell() const
        {
			_verify_can_write();
            return helper()->m_buffer.seekoff(0, std::ios_base::cur, std::ios_base::out);
        }

        /// <summary>
        /// Test whether the stream has been initialized with a valid stream buffer.
        /// </summary>
        bool is_valid() const { return (m_helper != nullptr) && ((bool)m_helper->m_buffer); }

        /// <summary>
        /// Test whether the stream has been initialized or not.
        /// </summary>
        operator bool() const { return is_valid(); }

        /// <summary>
        /// Test whether the stream is open for writing.
        /// </summary>
        bool is_open() const { return is_valid() && m_helper->m_buffer.can_write(); }

        /// <summary>
        /// Get the underlying stream buffer.
        /// </summary>
        concurrency::streams::streambuf<CharType> streambuf() const
        {
            return helper()->m_buffer;
        }

    protected:

        void set_helper(std::shared_ptr<details::basic_ostream_helper<CharType>> helper)
        {
            m_helper = helper;
        }

    private:

        void _verify_can_write() const
        {
			if ( !helper()->m_buffer.can_write() )
				throw std::runtime_error("stream not set up for output of data");
        }

        std::shared_ptr<details::basic_ostream_helper<CharType>> helper() const
        {
            if ( !m_helper )
                throw std::logic_error("uninitialized stream object");
            return m_helper;
        }
        
        std::shared_ptr<details::basic_ostream_helper<CharType>> m_helper;
    };

#pragma region Type parsing helpers

    template<typename int_type> 
    struct _type_parser_integral_traits
    {
        typedef std::false_type _is_integral;
        typedef std::false_type _is_unsigned;
    };

#ifdef _MS_WINDOWS
#define _INT_TRAIT(_t,_low,_high)  template<> struct _type_parser_integral_traits<_t>{typedef std::true_type _is_integral;typedef std::false_type _is_unsigned;static const int64_t _min = _low;static const int64_t _max = _high;};
#define _UINT_TRAIT(_t,_low,_high) template<> struct _type_parser_integral_traits<_t>{typedef std::true_type _is_integral;typedef std::true_type  _is_unsigned;static const uint64_t _max = _high;};

    _INT_TRAIT(char,INT8_MIN,INT8_MAX)
    _INT_TRAIT(signed char,INT8_MIN,INT8_MAX)
    _INT_TRAIT(short,INT16_MIN,INT16_MAX)
    _INT_TRAIT(utf16char,INT16_MIN,INT16_MAX)
    _INT_TRAIT(int,INT32_MIN,INT32_MAX)
    _UINT_TRAIT(unsigned char,UINT8_MIN,UINT8_MAX)
    _UINT_TRAIT(unsigned short,UINT16_MIN,UINT16_MAX)
    _UINT_TRAIT(unsigned int,UINT32_MIN,UINT32_MAX)
#else
#define _INT_TRAIT(_t)  template<> struct _type_parser_integral_traits<_t>{typedef std::true_type _is_integral;typedef std::false_type _is_unsigned;static const int64_t _min = std::numeric_limits<_t>::min();static const int64_t _max = std::numeric_limits<_t>::max();};
#define _UINT_TRAIT(_t) template<> struct _type_parser_integral_traits<_t>{typedef std::true_type _is_integral;typedef std::true_type  _is_unsigned;static const uint64_t _max = std::numeric_limits<_t>::max();};

    _INT_TRAIT(char)
    _INT_TRAIT(signed char)
    _INT_TRAIT(short)
    _INT_TRAIT(utf16char)
    _INT_TRAIT(int)
    _UINT_TRAIT(unsigned char)
    _UINT_TRAIT(unsigned short)
    _UINT_TRAIT(unsigned int)
#endif

    template<typename CharType>
    class _type_parser_base
    {
    public:
        typedef typename std::char_traits<CharType>::int_type int_type;

        _type_parser_base()  { }

    protected:
        // Aid in parsing input: skipping whitespace characters.
        static pplx::task<void> _skip_whitespace(streams::streambuf<CharType> buffer);

        // Aid in parsing input: peek at a character at a time, call type-specific code to examine, extract value when done.
        template<typename _ParseState, typename _ReturnType>
        static pplx::task<_ReturnType> _parse_input(streams::streambuf<CharType> buffer,
                                                    std::function<bool(std::shared_ptr<_ParseState>, int_type)> accept_character, 
                                                    std::function<pplx::task<_ReturnType>(std::shared_ptr<_ParseState>)> extract);
    };

    template<typename CharType, typename T>
    class _type_parser
    {
    public:
        static pplx::task<T> parse(streams::streambuf<CharType> buffer)
        {
            typename _type_parser_integral_traits<T>::_is_integral ii;
            typename _type_parser_integral_traits<T>::_is_unsigned ui;
            return _parse(buffer, ii, ui);
        }

    private:
        static pplx::task<T> _parse(streams::streambuf<CharType>, std::false_type, std::false_type)
        {
#ifdef _MS_WINDOWS
            static_assert(false, "type is not supported for extraction from a stream");
#else
            throw std::runtime_error("type is not supported for extraction from a stream");
#endif
        }
        static pplx::task<T> _parse(streams::streambuf<CharType>, std::false_type, std::true_type)
        {
#ifdef _MS_WINDOWS
            static_assert(false, "type is not supported for extraction from a stream");
#else
            throw std::runtime_error("type is not supported for extraction from a stream");
#endif
        }

        static pplx::task<T> _parse(streams::streambuf<CharType> buffer, std::true_type, std::false_type)
        {
            return _type_parser<CharType,int64_t>::parse(buffer).then(
                [] (pplx::task<int64_t> op) -> T
                {
                    int64_t val = op.get();
                    if ( val <= _type_parser_integral_traits<T>::_max && val >= _type_parser_integral_traits<T>::_min )
                        return (T)val;
                    else
                        throw std::range_error("input out of range for target type");
                });
        }

        static pplx::task<T> _parse(streams::streambuf<CharType> buffer, std::true_type, std::true_type)
        {
            return _type_parser<CharType,uint64_t>::parse(buffer).then(
                [] (pplx::task<uint64_t> op) -> T
                {
                    uint64_t val = op.get();
                    if ( val <= _type_parser_integral_traits<T>::_max )
                        return (T)val;
                    else
                        throw std::range_error("input out of range for target type");
                });
        }
    };

#pragma endregion

    template<typename CharType>
    class basic_istream
    {
    public:

        typedef std::char_traits<CharType> traits;
        typedef typename std::char_traits<CharType>::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;


        /// <summary>
        /// Default constructor
        /// </summary>
        basic_istream() {}

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="ptr">A stream buffer</param>
        basic_istream(streams::streambuf<CharType> buffer) : m_helper(std::make_shared<details::basic_istream_helper<CharType>>(buffer)) 
        {
            if ( !buffer.can_read() )
                throw std::runtime_error("stream buffer not set up for input of data");
        }

        /// <summary>
        /// Copy constructor
        /// </summary>
        /// <param name="other">The source object</param>
        basic_istream(const basic_istream &other) : m_helper(other.m_helper) { }

        /// <summary>
        /// Assignment operator
        /// </summary>
        /// <param name="other">The source object</param>
        basic_istream & operator =(const basic_istream &other)
        {
            m_helper = other.m_helper; 
            return *this; 
        }

        /// <summary>
        /// Close the stream, preventing further read operations.
        /// </summary>
        pplx::task<bool> close() const
        {
            return is_valid() ?
                helper()->m_buffer.close(std::ios_base::in) : 
                pplx::task_from_result(false); 
        }

        /// <summary>
        /// Close the stream with exception, preventing further read operations.
        /// </summary>
        pplx::task<bool> close(std::exception_ptr eptr) const
        {
            return is_valid() ?
                m_helper->m_buffer.close(std::ios_base::in, eptr) : 
                pplx::task_from_result(false); 
        }

        /// <summary>
        /// Tests whether last read cause the stream reach EOF.
        /// </summary>
        bool is_eof() const
        {
            return is_valid() ?
                m_helper->m_buffer.is_eof() : 
                false;
        }

        /// <summary>
        /// Get the next character and return it as an int_type. Advance the read position.
        /// </summary>
        /// <returns>The character read</returns>
        pplx::task<int_type> read() const
        {
			_verify_can_read();
            return helper()->m_buffer.bumpc();
        }

        /// <summary>
        /// Read up to 'count' characters and place into the provided buffer.
        /// </summary>
        /// <param name="str">A block of CharType locations, at least 'count' in size</param>
        /// <param name="count">The maximum number of characters to read</param>
        /// <returns>The number of bytes read. EOF if the end of the stream is reached.</returns>
        pplx::task<size_t> read(streams::streambuf<CharType> target, size_t count) const 
        {
			_verify_can_read();
			if ( !target.can_write() )
				throw std::runtime_error("target not set up for output of data");

            auto data = target.alloc(count);

            if ( data != nullptr )
            {
                auto post_read = 
                    [target](pplx::task<size_t> op)-> pplx::task<size_t>
                    {
                        auto t = target;
                        t.commit(op.get());
                        return op; 
                    };
                return helper()->m_buffer.getn(data, count).then(post_read);
            }
            else
            {
                size_t available = 0;

                if ( helper()->m_buffer.acquire(data, available) && available >= count )
                {
                    auto buffer = m_helper->m_buffer;

                    auto post_write = 
                        [buffer,data](pplx::task<size_t> op)-> pplx::task<size_t>
                        {
                            auto b = buffer;
                            b.release(data,op.get());
                            return op; 
                        };
                    return target.putn(data, count).then(post_write);
                }
                else
                {
                    std::shared_ptr<CharType> buf(new CharType[count]);

                    auto post_write = 
                        [buf](pplx::task<size_t> op) -> pplx::task<size_t>
                        {
                            return op; 
                        };
                    auto post_read = 
                        [buf,target,post_write](pplx::task<size_t> op) -> pplx::task<size_t>
                        {
                            auto trg = target;
                            return trg.putn(buf.get(), op.get()).then(post_write);
                        };

                    return helper()->m_buffer.getn(buf.get(), count).then(post_read);
                }
            }
        }

        /// <summary>
        /// Get the next character and return it as an int_type. Do not advance the read position.
        /// </summary>
        /// <returns>The character, widened to an integer. EOF if the operation fails.</returns>
        pplx::task<int_type> peek() const
        {
			_verify_can_read();
            return helper()->m_buffer.getc();
        }

        /// <summary>
        /// Read characters until a delimiter or EOF is found, and place them into the target.
        /// Proceed past the delimiter, but don't include it in the target buffer.
        /// </summary>
        /// <param name="target">An async stream buffer supporting write operations.</param>
        /// <returns>The number of bytes read.</returns>
        pplx::task<size_t> read_to_delim(streams::streambuf<CharType> target, int_type delim) const
        {
            // Capture 'buffer' rather than 'helper' here due to VC++ 2010 limitations.
            auto buffer = helper()->m_buffer;

			_verify_can_read();
			if ( !target.can_write() )
				throw std::runtime_error("target not set up for receiving data");

            int_type req_async = ::concurrency::streams::char_traits<CharType>::requires_async();

            std::shared_ptr<_read_helper> _locals = std::make_shared<_read_helper>();

            // We're having to create all these lambdas because VS 2010 has trouble compiling
            // nested lambdas.
            auto after_putn = 
                [=](pplx::task<size_t> wrote) mutable -> bool
                {
                    _locals->total += wrote.get();
                    _locals->write_pos = 0;
                    target.sync().wait();
                    return true;
                };

            auto flush = 
                [=] () mutable -> pplx::task<bool>
                {
                    return target.putn(_locals->outbuf, _locals->write_pos).then(after_putn);
                };

            auto update = [=] (int_type ch) mutable -> bool 
                { 
                    if ( ch == std::char_traits<CharType>::eof() ) return false;
                    if ( ch == delim ) return false;

                    _locals->outbuf[_locals->write_pos] = static_cast<CharType>(ch);
                    _locals->write_pos += 1;

                    if ( _locals->is_full() )
                    {
                        return flush().get();
                    }

                    return true;
                };

            auto loop = pplx::do_while([=]() mutable -> pplx::task<bool>
                {
                    while ( buffer.in_avail() > 0 )
                    {
                        int_type ch = buffer.sbumpc();

                        if (  ch == req_async )
                            break;
                        
                        if ( !update(ch) )
                            return pplx::task_from_result(false);
                    }

                    return buffer.bumpc().then(update);
                });

            return loop.then([=](bool) mutable -> size_t
                { 
                    flush().wait();
                    return _locals->total; 
                });
        }

        /// <summary>
        /// Read until reaching a newline character. The newline is not included in the target.
        /// </summary>
        /// <param name="target">An async stream buffer supporting write operations.</param>
        /// <returns>The number of bytes read. O if the end of the stream is reached.</returns>
        pplx::task<size_t> read_line(streams::streambuf<CharType> target) const
        {
            // Capture 'buffer' rather than 'helper' here due to VC++ 2010 limitations.
            concurrency::streams::streambuf<CharType> buffer = helper()->m_buffer;

			_verify_can_read();
			if ( !target.can_write() )
				throw std::runtime_error("target not set up for receiving data");

            typename std::char_traits<CharType>::int_type req_async = concurrency::streams::char_traits<CharType>::requires_async();

            std::shared_ptr<_read_helper> _locals = std::make_shared<_read_helper>();

            // We're having to create all these lambdas because VS 2010 has trouble compiling
            // nested lambdas.
            auto after_putn = 
                [=](pplx::task<size_t> wrote) mutable -> bool
                {
                    _locals->total += wrote.get();
                    _locals->write_pos = 0;
                    target.sync().wait();
                    return true;
                };

            auto flush = 
                [=] () mutable -> pplx::task<bool>
                {
                    return target.putn(_locals->outbuf, _locals->write_pos).then(after_putn);
                };

            auto update = [=] (typename std::char_traits<CharType>::int_type ch) mutable -> bool 
                { 
                    if ( ch == std::char_traits<CharType>::eof() ) return false;
                    if ( ch == '\n' ) return false;
                    if ( ch == '\r' ) { _locals->saw_CR = true; return true; }

                    _locals->outbuf[_locals->write_pos] = static_cast<CharType>(ch);
                    _locals->write_pos += 1;

                    if ( _locals->is_full() )
                    {
                        return flush().get();
                    }

                    return true;
                };

            auto return_false = 
                [](pplx::task<typename std::char_traits<CharType>::int_type>) -> pplx::task<bool> 
                {
                    return pplx::task_from_result(false);
                };

            auto update_after_cr = 
                [=] (typename std::char_traits<CharType>::int_type ch) mutable -> pplx::task<bool> 
                { 
                    if ( ch == std::char_traits<CharType>::eof() ) return pplx::task_from_result(false);
                    if ( ch == '\n' )
                        return buffer.bumpc().then(return_false);

                    return pplx::task_from_result(false);
                };

            auto loop = pplx::do_while([=]() mutable -> pplx::task<bool>
                {
                    while ( buffer.in_avail() > 0 )
                    {
#ifndef _MS_WINDOWS // Required by GCC, because std::char_traits<CharType> is a dependent scope
                        typename
#endif
                        std::char_traits<CharType>::int_type ch;

                        if ( _locals->saw_CR )
                        {
                            ch = buffer.sgetc();
                            if ( ch == '\n' )
                                buffer.sbumpc();
                            return pplx::task_from_result(false);
                        }
                        
                        ch = buffer.sbumpc();

                        if (  ch == req_async )
                            break;
                        
                        if ( !update(ch) )
                            return pplx::task_from_result(false);
                    }

                    if ( _locals->saw_CR )
                    {
                        return buffer.getc().then(update_after_cr);
                    }
                    return buffer.bumpc().then(update);
                });

            return loop.then([=](bool) mutable -> size_t
                { 
                    flush().wait();
                    return _locals->total; 
                });
        }

        // To workaround a VS 2010 internal compiler error, we have to do our own
        // "lambda" here...
        class _dev10_ice_workaround
        {
        public:
            _dev10_ice_workaround(streams::streambuf<CharType> buffer,
                                  concurrency::streams::streambuf<CharType> target,
                                  std::shared_ptr<typename basic_istream::_read_helper> locals,
                                  size_t buf_size) 
                : _buffer(buffer), _target(target), _locals(locals), _buf_size(buf_size)
            {
            }
            pplx::task<bool> operator()()
            {
                // We need to capture these, because the object itself may go away
                // before we're done processing the data.
                auto locs = _locals;
                auto trg = _target;

                auto after_putn = 
                    [=](size_t wr) mutable -> bool
                    {
                        locs->total += wr;
                        trg.sync().wait();
                        return true;
                    };

                return _buffer.getn(locs->outbuf, buf_size).then(
                    [=] (size_t rd) mutable -> pplx::task<bool> 
                    { 
                        if ( rd == 0 )
                            return pplx::task_from_result(false);
                        return trg.putn(locs->outbuf, rd).then(after_putn);
                    });
            }
        private:
            size_t _buf_size;
            concurrency::streams::streambuf<CharType> _buffer;
            concurrency::streams::streambuf<CharType> _target;
            std::shared_ptr<typename basic_istream::_read_helper> _locals;
        };

        /// <summary>
        /// Read until reaching the end of the stream.
        /// </summary>
        /// <param name="target">An async stream buffer supporting write operations.</param>
        /// <returns>The number of bytes read. O if the end of the stream is reached.</returns>
        pplx::task<size_t> read_to_end(streams::streambuf<CharType> target) const
        {
            // Capture 'buffer' rather than 'helper' here due to VC++ 2010 limitations.
            auto buffer = helper()->m_buffer;

			_verify_can_read();
			if ( !target.can_write() )
				throw std::runtime_error("target not set up for receiving data");

            std::shared_ptr<_read_helper> _locals = std::make_shared<_read_helper>();

            _dev10_ice_workaround wrkarnd(buffer, target, _locals, buf_size);

            auto loop = pplx::do_while(wrkarnd);

            return loop.then([=](bool) mutable -> size_t
                { 
                    return _locals->total; 
                });
        }

        /// <summary>
        /// Seek to the specified write position.
        /// </summary>
        /// <param name="pos">An offset relative to the beginning of the stream.</param>
        pos_type seek(pos_type pos) const
        {
			_verify_can_read();
            return helper()->m_buffer.seekpos(pos, std::ios_base::in);
        }

        /// <summary>
        /// Seek to the specified write position.
        /// </summary>
        /// <param name="off">An offset relative to the beginning, current write position, or the end of the stream.</param>
        /// <param name="way">The starting point (beginning, current, end) for the seek.</param>
        pos_type seek(off_type off, std::ios_base::seekdir way) const
        {
			_verify_can_read();
            return helper()->m_buffer.seekoff(off, way, std::ios_base::in);
        }

        /// <summary>
        /// Get the current write position, i.e. the offset from the beginning of the stream.
        /// </summary>
        pos_type tell() const
        {
			_verify_can_read();
            return helper()->m_buffer.seekoff(0, std::ios_base::cur, std::ios_base::in);
        }

        /// <summary>
        /// Test whether the stream has been initialized with a valid stream buffer.
        /// </summary>
        bool is_valid() const { return (m_helper != nullptr) && ((bool)m_helper->m_buffer); }

        /// <summary>
        /// Test whether the stream has been initialized or not.
        /// </summary>
        operator bool() const { return is_valid(); }

        /// <summary>
        /// Test whether the stream is open for writing.
        /// </summary>
        bool is_open() const { return is_valid() && m_helper->m_buffer.can_read(); }

        /// <summary>
        /// Get the underlying stream buffer.
        /// </summary>
        concurrency::streams::streambuf<CharType> streambuf() const
        {
            return helper()->m_buffer;
        }

        /// <summary>
        /// Read a value of type T from the stream.
        /// </summary>
        template<typename T>
        pplx::task<T> extract() const
        {
			_verify_can_read();
            return _type_parser<CharType,T>::parse(helper()->m_buffer);
        }

    protected:
        void set_helper(std::shared_ptr<details::basic_istream_helper<CharType>> helper)
        {
            m_helper = helper;
        }

    private:

        void _verify_can_read() const
        {
			if ( !helper()->m_buffer.can_read() )
				throw std::runtime_error("stream not set up for input of data");
        }

        std::shared_ptr<details::basic_istream_helper<CharType>> helper() const
        {
            if ( !m_helper )
                throw std::logic_error("uninitialized stream object");
            return m_helper;
        }

        static const size_t buf_size = 16*1024;

        struct _read_helper
        {
            size_t total;
            CharType outbuf[buf_size];
            size_t write_pos;
            bool saw_CR;

            bool is_full() const 
            {
                return write_pos == buf_size;
            }

            _read_helper() : write_pos(0), total(0), saw_CR(false)
            {
            }
        };

        std::shared_ptr<details::basic_istream_helper<CharType>> m_helper;
    };

    typedef basic_ostream<uint8_t> ostream;
    typedef basic_istream<uint8_t> istream;

    typedef basic_ostream<utf16char> wostream;
    typedef basic_istream<utf16char> wistream;

#pragma region Input of data of various types.
template<typename CharType>
pplx::task<void> concurrency::streams::_type_parser_base<CharType>::_skip_whitespace(streams::streambuf<CharType> buffer)
{
    int_type req_async = concurrency::streams::char_traits<CharType>::requires_async();

    auto update = [=] (int_type ch) mutable -> bool 
        { 
            if (isspace(ch)) 
            {
                buffer.sbumpc();
                return true;
            }

            return false;
        };

    auto loop = pplx::do_while([=]() mutable -> pplx::task<bool>
        {
            while ( buffer.in_avail() > 0 )
            {
                int_type ch = buffer.sgetc();

                if (  ch == req_async )
                    break;
                        
                if ( !update(ch) )
                    return pplx::task_from_result(false);
            }
            return buffer.getc().then(update);
        });

    return loop.then([=](pplx::task<bool> op)
        { 
            op.wait();
        });
}

template<typename CharType>
template<typename _ParseState, typename _ReturnType>
pplx::task<_ReturnType> concurrency::streams::_type_parser_base<CharType>::_parse_input(
    concurrency::streams::streambuf<CharType> buffer,
    std::function<bool(std::shared_ptr<_ParseState>, int_type)> accept_character, 
    std::function<pplx::task<_ReturnType>(std::shared_ptr<_ParseState>)> extract)
{
    std::shared_ptr<_ParseState> state = std::make_shared<_ParseState>();

    auto update_end = [=] (pplx::task<int_type> op) -> bool { op.wait(); return true; };

    auto update = [=] (pplx::task<int_type> op) -> pplx::task<bool> 
            { 
                int_type ch = op.get();
                if ( ch == std::char_traits<CharType>::eof() ) return pplx::task_from_result(false);
                bool accptd = accept_character(state, ch);
                if (!accptd)
                    return pplx::task_from_result(false);
                // We peeked earlier, so now we must advance the position.
                concurrency::streams::streambuf<CharType> buf = buffer;
                return buf.bumpc().then(update_end);
            };

    auto peek_char = [=]() -> pplx::task<bool>
    {
        concurrency::streams::streambuf<CharType> buf = buffer; 

        // If task results are immediately available, there's little need to use ".then(),"
        // so optimize for prompt values.

        auto get_op = buf.getc();
        while (get_op.is_done())
        {
            auto condition = update(get_op);
            if ( !condition.is_done() || !condition.get())
                return condition;

            get_op = buf.getc();
        }

        return get_op.then(update); 
    };

    auto finish = 
        [=](pplx::task<bool> op) -> pplx::task<_ReturnType>
        { 
            op.wait();
            pplx::task<_ReturnType> result = extract(state);
            return result; 
        };

    return _skip_whitespace(buffer).then([=](pplx::task<void> op) -> pplx::task<_ReturnType>
        {
            op.wait();

            return pplx::do_while(peek_char).then(finish);
        });
}

template<typename CharType>
class _type_parser<CharType,std::basic_string<CharType>> : public _type_parser_base<CharType>
{
    typedef typename _type_parser_base<CharType>::int_type int_type;
public:
    static pplx::task<std::string> parse(streams::streambuf<CharType> buffer)
    {
        return concurrency::streams::_type_parser_base<CharType>::template _parse_input<std::basic_string<CharType>, std::string>(buffer, _accept_char, _extract_result);
    }

private:
    static bool _accept_char(std::shared_ptr<std::basic_string<CharType>> state, int_type ch)
    {
        if ( ch == std::char_traits<CharType>::eof() || isspace(ch)) return false;
        state->push_back(CharType(ch));
        return true;
    }
    static pplx::task<std::basic_string<CharType>> _extract_result(std::shared_ptr<std::basic_string<CharType>> state)
    {
        return pplx::task_from_result(*state);
    }
};


template<typename CharType>
class _type_parser<CharType,int64_t> : public _type_parser_base<CharType>
{
public:
    typedef typename _type_parser_base<CharType>::int_type int_type;
    static pplx::task<int64_t> parse(streams::streambuf<CharType> buffer)
    {
        return _type_parser_base<CharType>::template _parse_input<_int64_state, int64_t>(buffer, _accept_char, _extract_result);
    }
private:
    struct _int64_state
    {
        _int64_state() : result(0), correct(false), minus(0) {}

        int64_t result;
        bool correct;
        char minus;       // 0 -- no sign, 1 -- plus, 2 -- minus
    };

    static bool _accept_char(std::shared_ptr<_int64_state> state, int_type ch)
    {
        if ( ch == std::char_traits<CharType>::eof()) return false;
        if ( state->minus == 0 )
        {
            // OK to find a sign.
            if ( !::isdigit(ch) && 
                    ch != int_type('+') && ch != int_type('-') ) return false; 
        }
        else
        {
            if ( !::isdigit(ch) ) return false; 
        }

        // At least one digit was found.
        state->correct = true;

        if ( ch == int_type('+') )
        {
            state->minus = 1;
        }
        else if ( ch == int_type('-') )
        {
            state->minus = 2;
        }
        else
        {
            if (state->minus == 0) state->minus = 1;

            // Shift the existing value by 10, then add the new value.
            bool positive = state->result >= 0;

            state->result *= 10;
            state->result += int64_t(ch-int_type('0'));

            if ( (state->result >= 0) != positive ) 
            {
                state->correct = false;
                return false;
            }
        }
        return true;
    }

    static pplx::task<int64_t> _extract_result(std::shared_ptr<_int64_state> state)
    {
        if (!state->correct)
            throw std::range_error("integer value is too large to fit in 64 bits");

        int64_t result = (state->minus == 2) ? -state->result : state->result;
        return pplx::task_from_result<int64_t>(result);
    }
};

template<typename CharType>
class _type_parser<CharType,uint64_t> : public _type_parser_base<CharType>
{
public:
    typedef typename _type_parser_base<CharType>::int_type int_type;
    static pplx::task<uint64_t> parse(streams::streambuf<CharType> buffer)
    {
        return _type_parser_base<CharType>::template _parse_input<_uint64_state,uint64_t>(buffer, _accept_char, _extract_result);
    }

private:
    struct _uint64_state
    {
        _uint64_state() : result(0), correct(false) {}
        uint64_t result;
        bool correct;
    };

    static bool _accept_char(std::shared_ptr<_uint64_state> state, int_type ch)
    {
        if ( !::isdigit(ch) ) return false; 

        // At least one digit was found.
        state->correct = true;

        // Shift the existing value by 10, then add the new value.
        state->result *= 10;
        state->result += uint64_t(ch-int_type('0'));

        return true;
    }
    static pplx::task<uint64_t> _extract_result(std::shared_ptr<_uint64_state> state)
    {
        if (!state->correct)
            throw std::range_error("integer value is too large to fit in 64 bits");
        return pplx::task_from_result(state->result);
    }
};

template<typename CharType>
class _type_parser<CharType,bool> : public _type_parser_base<CharType>
{
public:
    typedef typename _type_parser_base<CharType>::int_type int_type;
    static pplx::task<bool> parse(streams::streambuf<CharType> buffer)
    {
        return _type_parser_base<CharType>::template _parse_input<_bool_state,bool>(buffer, _accept_char, _extract_result);
    }
private:
    struct _bool_state
    {
        _bool_state() : state(0) { }
        // { 0 -- not started, 1 -- 't', 2 -- 'tr', 3 -- 'tru', 4 -- 'f', 5 -- 'fa', 6 -- 'fal', 7 -- 'fals', 8 -- 'true', 9 -- 'false' }
        short state;
    };

    static bool _accept_char(std::shared_ptr<_bool_state> state, int_type ch)
    {
        switch (state->state)
        {
        case 0:
            if ( ch == int_type('t') ) state->state = 1;
            else if ( ch == int_type('f') ) state->state = 4;
            else return false;
            break;
        case 1:
            if ( ch == int_type('r') ) state->state = 2;
            else return false;
            break;
        case 2:
            if ( ch == int_type('u') ) state->state = 3;
            else return false;
            break;
        case 3:
            if ( ch == int_type('e') ) state->state = 8;
            else return false;
            break;
        case 4:
            if ( ch == int_type('a') ) state->state = 5;
            else return false;
            break;
        case 5:
            if ( ch == int_type('l') ) state->state = 6;
            else return false;
            break;
        case 6:
            if ( ch == int_type('s') ) state->state = 7;
            else return false;
            break;
        case 7:
            if ( ch == int_type('e') ) state->state = 9;
            else return false;
            break;
        case 8:
        case 9:
            return false;
        }
        return true;
    }
    static pplx::task<bool> _extract_result(std::shared_ptr<_bool_state> state)
    {
        bool correct = (state->state == 8 || state->state == 9);
        if (!correct)
        {
            std::runtime_error exc("cannot parse as Boolean value");
            throw exc;
        }
        return pplx::task_from_result(state->state == 8);
    }
};

template<typename CharType>
class _type_parser<CharType,signed char> : public _type_parser_base<CharType>
{
    typedef typename concurrency::streams::streambuf<CharType>::int_type int_type;
public:
    static pplx::task<signed char> parse(streams::streambuf<CharType> buffer)
    {
        return _type_parser_base<CharType>::_skip_whitespace(buffer).then(
            [=](pplx::task<void> op) -> pplx::task<signed char>
            {
                op.wait();
                return _type_parser<CharType,signed char>::_get_char(buffer);
            });
    }
private:
    static pplx::task<signed char> _get_char(streams::streambuf<CharType> buffer)
    {
        concurrency::streams::streambuf<CharType> buf = buffer;
        return buf.bumpc().then(
            [=](pplx::task<typename concurrency::streams::streambuf<CharType>::int_type> op) -> signed char
            {
                int_type val = op.get();
                if (val == std::char_traits<CharType>::eof())
                    throw std::runtime_error("reached end-of-stream while constructing a value");
                return static_cast<signed char>(val);
            });
    }
};

template<typename CharType>
class _type_parser<CharType,unsigned char> : public _type_parser_base<CharType>
{
    typedef typename concurrency::streams::streambuf<CharType>::int_type int_type;
public:
    static pplx::task<unsigned char> parse(streams::streambuf<CharType> buffer)
    {
        return _type_parser_base<CharType>::_skip_whitespace(buffer).then(
            [=](pplx::task<void> op) -> pplx::task<unsigned char>
            {
                op.wait();
                return _type_parser<CharType,unsigned char>::_get_char(buffer);
            });
    }
private:
    static pplx::task<unsigned char> _get_char(streams::streambuf<CharType> buffer)
    {
        concurrency::streams::streambuf<CharType> buf = buffer;
        return buf.bumpc().then(
            [=](pplx::task<typename concurrency::streams::streambuf<CharType>::int_type> op) -> unsigned char
            {
                int_type val = op.get();
                if (val == std::char_traits<CharType>::eof())
                    throw std::runtime_error("reached end-of-stream while constructing a value");
                return static_cast<unsigned char>(val);
            });
    }
};

template<typename CharType>
class _type_parser<CharType,char> : public _type_parser_base<CharType>
{
    typedef typename concurrency::streams::streambuf<CharType>::int_type int_type;
public:
    static pplx::task<char> parse(streams::streambuf<CharType> buffer)
    {
        return _type_parser_base<CharType>::_skip_whitespace(buffer).then(
            [=](pplx::task<void> op) -> pplx::task<char>
            {
                op.wait();
                return _get_char(buffer);
            });
    }
private:
    static pplx::task<char> _get_char(streams::streambuf<CharType> buffer)
    {
        concurrency::streams::streambuf<CharType> buf = buffer;
        return buf.bumpc().then(
            [=](pplx::task<typename concurrency::streams::streambuf<CharType>::int_type> op) -> char
            {
                int_type val = op.get();
                if (val == std::char_traits<CharType>::eof())
                    throw std::runtime_error("reached end-of-stream while constructing a value");
                return char(val);
            });
    }
};

#ifdef _MS_WINDOWS
template<>
class _type_parser<char,std::basic_string<wchar_t>> : public _type_parser_base<char>
{
public:
    static pplx::task<std::wstring> parse(streams::streambuf<char> buffer)
    {
        return _parse_input<std::basic_string<char>,std::basic_string<wchar_t>>(buffer, _accept_char, _extract_result);
    }

private:
    static bool _accept_char(std::shared_ptr<std::basic_string<char>> state, int_type ch)
    {
        if ( ch == std::char_traits<char>::eof() || isspace(ch)) return false;
        state->push_back(char(ch));
        return true;
    }
    static pplx::task<std::basic_string<wchar_t>> _extract_result(std::shared_ptr<std::basic_string<char>> state)
    {
        return pplx::task_from_result(utility::conversions::utf8_to_utf16(*state));
    }
};

template<>
class _type_parser<signed char,std::basic_string<wchar_t>> : public _type_parser_base<signed char>
{
public:
    static pplx::task<std::wstring> parse(streams::streambuf<signed char> buffer)
    {
        return _parse_input<std::basic_string<char>,std::basic_string<wchar_t>>(buffer, _accept_char, _extract_result);
    }

private:
    static bool _accept_char(std::shared_ptr<std::basic_string<char>> state, int_type ch)
    {
        if ( ch == std::char_traits<char>::eof() || isspace(ch)) return false;
        state->push_back(char(ch));
        return true;
    }
    static pplx::task<std::basic_string<wchar_t>> _extract_result(std::shared_ptr<std::basic_string<char>> state)
    {
        return pplx::task_from_result(utility::conversions::utf8_to_utf16(*state));
    }
};

template<>
class _type_parser<unsigned char,std::basic_string<wchar_t>> : public _type_parser_base<unsigned char>
{
public:
    static pplx::task<std::wstring> parse(streams::streambuf<unsigned char> buffer)
    {
        return _parse_input<std::basic_string<char>,std::basic_string<wchar_t>>(buffer, _accept_char, _extract_result);
    }

private:
    static bool _accept_char(std::shared_ptr<std::basic_string<char>> state, int_type ch)
    {
        if ( ch == std::char_traits<char>::eof() || isspace(ch)) return false;
        state->push_back(char(ch));
        return true;
    }
    static pplx::task<std::basic_string<wchar_t>> _extract_result(std::shared_ptr<std::basic_string<char>> state)
    {
        return pplx::task_from_result(utility::conversions::utf8_to_utf16(*state));
    }
};
#endif //_MS_WINDOWS
#pragma endregion

}}

#endif  /* _CASA_STREAMS_H */
