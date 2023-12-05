/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * This file defines the checked iterator iterator template that originated from MSVC STL's
 * stdext::checked_iterator_iterator, which is now deprecated.
 *
 * For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/
#pragma once

#ifndef CHECKED_ARRAY_ITERATOR_H
#define CHECKED_ARRAY_ITERATOR_H

#include <iterator>

#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
namespace Concurrency
{
namespace streams
{
namespace details
{
namespace ext
{
template<class Ptr>
class checked_array_iterator
{ // wrap a pointer with checking
private:
    using pointee_type_ = std::remove_pointer_t<Ptr>;
    static_assert(std::is_pointer_v<Ptr> && std::is_object_v<pointee_type_>,
                  "checked_array_iterator requires pointers to objects");

public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::remove_cv_t<pointee_type_>;
    using difference_type = std::ptrdiff_t;
    using pointer = Ptr;
    using reference = pointee_type_&;
#ifdef __cpp_lib_concepts
    using iterator_concept = std::contiguous_iterator_tag;
#endif // defined(__cpp_lib_concepts)

    constexpr checked_array_iterator() = default;

    constexpr checked_array_iterator(const Ptr arr, const size_t size, const size_t ind = 0) noexcept
        : m_data(arr), m_size(size), m_index(ind)
    {
        _STL_VERIFY(ind <= size, "checked_array_iterator construction index out of range");
    }

    template<class T = pointee_type_, std::enable_if_t<!std::is_const_v<T>, int> = 0>
    constexpr operator checked_array_iterator<const T*>() const noexcept
    {
        return checked_array_iterator<const T*> {m_data, m_size, m_index};
    }

    _NODISCARD constexpr Ptr base() const noexcept { return m_data + m_index; }

    _NODISCARD constexpr reference operator*() const noexcept { return *operator->(); }

    _NODISCARD constexpr pointer operator->() const noexcept
    {
        _STL_VERIFY(m_data, "cannot dereference value-initialized or null checked_array_iterator");
        _STL_VERIFY(m_index < m_size, "cannot dereference end checked_array_iterator");
        return m_data + m_index;
    }

    constexpr checked_array_iterator& operator++() noexcept
    {
        _STL_VERIFY(m_data, "cannot increment value-initialized or null checked_array_iterator");
        _STL_VERIFY(m_index < m_size, "cannot increment checked_array_iterator past end");
        ++m_index;
        return *this;
    }

    constexpr checked_array_iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr checked_array_iterator& operator--() noexcept
    {
        _STL_VERIFY(m_data, "cannot decrement value-initialized or null checked_array_iterator");
        _STL_VERIFY(m_index != 0, "cannot decrement checked_array_iterator before begin");
        --m_index;
        return *this;
    }

    constexpr checked_array_iterator operator--(int) noexcept
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr checked_array_iterator& operator+=(const difference_type off) noexcept
    {
        if (off != 0)
        {
            _STL_VERIFY(m_data, "cannot seek value-initialized or null checked_array_iterator");
        }

        if (off < 0)
        {
            _STL_VERIFY(m_index >= size_t {0} - static_cast<size_t>(off),
                        "cannot seek checked_array_iterator before begin");
        }

        if (off > 0)
        {
            _STL_VERIFY(m_size - m_index >= static_cast<size_t>(off), "cannot seek checked_array_iterator after end");
        }

        m_index += off;
        return *this;
    }

    _NODISCARD constexpr checked_array_iterator operator+(const difference_type off) const noexcept
    {
        auto tmp = *this;
        tmp += off;
        return tmp;
    }

    _NODISCARD_FRIEND constexpr checked_array_iterator operator+(const difference_type off,
                                                                 const checked_array_iterator<Ptr>& next_iter) noexcept
    {
        return next_iter + off;
    }

    constexpr checked_array_iterator& operator-=(const difference_type off) noexcept
    {
        if (off != 0)
        {
            _STL_VERIFY(m_data, "cannot seek value-initialized or null checked_array_iterator");
        }

        if (off > 0)
        {
            _STL_VERIFY(m_index >= static_cast<size_t>(off), "cannot seek checked_array_iterator before begin");
        }

        if (off < 0)
        {
            _STL_VERIFY(m_size - m_index >= size_t {0} - static_cast<size_t>(off),
                        "cannot seek checked_array_iterator after end");
        }

        m_index -= off;
        return *this;
    }

    _NODISCARD constexpr checked_array_iterator operator-(const difference_type off) const noexcept
    {
        auto tmp = *this;
        tmp -= off;
        return tmp;
    }

    _NODISCARD constexpr difference_type operator-(const checked_array_iterator& rhs) const noexcept
    {
        _STL_VERIFY(m_data == rhs.m_data && m_size == rhs.m_size,
                    "cannot subtract incompatible checked_array_iterators");
        return static_cast<difference_type>(m_index - rhs.m_index);
    }

    _NODISCARD constexpr reference operator[](const difference_type off) const noexcept { return *(*this + off); }

    _NODISCARD constexpr bool operator==(const checked_array_iterator& rhs) const noexcept
    {
        _STL_VERIFY(m_data == rhs.m_data && m_size == rhs.m_size,
                    "cannot compare incompatible checked_array_iterators for equality");
        return m_index == rhs.m_index;
    }

#if _HAS_CXX20
    _NODISCARD constexpr _STD strong_ordering operator<=>(const checked_array_iterator& rhs) const noexcept
    {
        _STL_VERIFY(m_data == rhs.m_data && m_size == rhs.m_size,
                    "cannot compare incompatible checked_array_iterators");
        return m_index <=> rhs.m_index;
    }
#else  // ^^^ _HAS_CXX20 / !_HAS_CXX20 vvv
    _NODISCARD constexpr bool operator!=(const checked_array_iterator& rhs) const noexcept { return !(*this == rhs); }

    _NODISCARD constexpr bool operator<(const checked_array_iterator& rhs) const noexcept
    {
        _STL_VERIFY(m_data == rhs.m_data && m_size == rhs.m_size,
                    "cannot compare incompatible checked_array_iterators");
        return m_index < rhs.m_index;
    }

    _NODISCARD constexpr bool operator>(const checked_array_iterator& rhs) const noexcept { return rhs < *this; }

    _NODISCARD constexpr bool operator<=(const checked_array_iterator& rhs) const noexcept { return !(rhs < *this); }

    _NODISCARD constexpr bool operator>=(const checked_array_iterator& rhs) const noexcept { return !(*this < rhs); }
#endif // !_HAS_CXX20

    friend constexpr void _Verify_range(const checked_array_iterator& first,
                                        const checked_array_iterator& last) noexcept
    {
        _STL_VERIFY(last.m_data == last.m_data && first.m_size == last.m_size, "mismatching checked_array_iterators");
        _STL_VERIFY(last.m_index <= last.m_index, "transposed checked_array_iterator range");
    }

    constexpr void _Verify_offset(const difference_type off) const noexcept
    {
        if (off < 0)
        {
            _STL_VERIFY(m_index >= size_t {0} - static_cast<size_t>(off),
                        "cannot seek checked_array_iterator iterator before begin");
        }

        if (off > 0)
        {
            _STL_VERIFY(m_size - m_index >= static_cast<size_t>(off),
                        "cannot seek checked_array_iterator iterator after end");
        }
    }

    using _Prevent_inheriting_unwrap = checked_array_iterator;

    _NODISCARD constexpr Ptr _Unwrapped() const noexcept { return m_data + m_index; }

    constexpr void _Seek_to(Ptr it) noexcept { m_index = static_cast<size_t>(it - m_data); }

private:
    Ptr m_data = nullptr; // beginning of array
    size_t m_size = 0;    // size of array
    size_t m_index = 0;   // offset into array
};
} // namespace ext
} // namespace details
} // namespace streams
} // namespace Concurrency
#endif

#endif
