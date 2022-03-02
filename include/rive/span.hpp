/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SPAN_HPP_
#define _RIVE_SPAN_HPP_

#include <assert.h>
#include <cstddef>
#include <type_traits>

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

    constexpr size_t totalBytes() const { return m_Size * sizeof(T); }

    constexpr Span<T> subset(size_t offset, size_t size) const {
        assert(offset <= m_Size);
        assert(size <= m_Size - offset);
        return {m_Ptr + offset, size};
    }
};

} // namespace rive

#endif
