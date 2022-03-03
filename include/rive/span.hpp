/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SPAN_HPP_
#define _RIVE_SPAN_HPP_

#include "rive/rive_types.hpp"
#include <vector>

/*
 *  Span : cheap impl of std::span (which is C++20)
 *
 *  Inspired by Skia's SkSpan
 */

namespace rive {

template <typename T> class Span {
    T*      m_Ptr;
    size_t  m_Size;

public:
    Span() : m_Ptr(nullptr), m_Size(0) {}
    Span(T* ptr, size_t size) : m_Ptr(ptr), m_Size(size) {
        assert(ptr <= ptr + size);
    }

    // We don't modify vec, but we don't want to say const, since that would
    // change .data() to return const T*, and we don't want to change it.
    Span(std::vector<T>& vec) : Span(vec.data(), vec.size()) {}

    constexpr T& operator[](size_t index) const {
        assert(index < m_Size);
        return m_Ptr[index];
    }

    constexpr T* data() const { return m_Ptr; }
    constexpr size_t size() const { return m_Size; }
    constexpr bool empty() const { return m_Size == 0; }
    
    constexpr T* begin() const { return m_Ptr; }
    constexpr T* end() const { return m_Ptr + m_Size; }

    constexpr T& front() const { return (*this)[0]; }
    constexpr T& back()  const { return (*this)[m_Size-1]; }

    // returns byte-size of the entire span
    constexpr size_t size_bytes() const { return m_Size * sizeof(T); }

    constexpr int count() const {
        const int n = static_cast<int>(m_Size);
        assert(n >= 0);
        return n;
    }

    constexpr Span<T> subset(size_t offset, size_t size) const {
        assert(offset <= m_Size);
        assert(size <= m_Size - offset);
        return {m_Ptr + offset, size};
    }
};

} // namespace rive

#endif
