/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SPAN_HPP_
#define _RIVE_SPAN_HPP_

#include "rive/rive_types.hpp"

#include <initializer_list>
#include <type_traits>
#include <iterator>

/*
 *  Span : cheap impl of std::span (which is C++20)
 *
 *  Inspired by Skia's SkSpan
 */

namespace rive
{

template <typename T> class Span
{
    T* m_Ptr;
    size_t m_Size;

public:
    Span() : m_Ptr(nullptr), m_Size(0) {}
    Span(T* ptr, size_t size) : m_Ptr(ptr), m_Size(size) { assert(ptr <= ptr + size); }

    // Handle Span<foo> --> Span<const foo>
    template <typename U, typename = typename std::enable_if<std::is_same<const U, T>::value>::type>
    constexpr Span(const Span<U>& that) : Span(that.data(), that.size())
    {}
    template <typename Container> constexpr Span(Container& c) : Span(c.data(), c.size()) {}

    T& operator[](size_t index) const
    {
        assert(index < m_Size);
        return m_Ptr[index];
    }

    constexpr T* data() const { return m_Ptr; }
    constexpr size_t size() const { return m_Size; }
    constexpr bool empty() const { return m_Size == 0; }

    constexpr T* begin() const { return m_Ptr; }
    constexpr T* end() const { return m_Ptr + m_Size; }

    constexpr T& front() const { return (*this)[0]; }
    constexpr T& back() const { return (*this)[m_Size - 1]; }

    // returns byte-size of the entire span
    constexpr size_t size_bytes() const { return m_Size * sizeof(T); }

    constexpr size_t count() const { return m_Size; }

    Span<T> subset(size_t offset, size_t size) const
    {
        assert(offset <= m_Size);
        assert(size <= m_Size - offset);
        return {m_Ptr + offset, size};
    }

    // Makes rive::Span std::Container compatible
    // https://en.cppreference.com/w/cpp/named_req/Container
    typedef typename std::remove_cv<T>::type value_type;
    typedef T& reference;
    typedef T const& const_reference;
    typedef T* iterator;
    typedef T const* const_iterator;
    typedef std::ptrdiff_t difference_type;
    typedef size_t size_type;
};

template <typename T> Span<T> make_span(T* ptr, size_t size) { return Span<T>(ptr, size); }

} // namespace rive

#endif
