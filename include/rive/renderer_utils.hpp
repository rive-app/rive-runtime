/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDERER_UTILS_HPP_
#define _RIVE_RENDERER_UTILS_HPP_

#include "rive/rive_types.hpp"
#include "rive/core/type_conversions.hpp"
#include <string>

template <size_t N, typename T> class AutoSTArray
{
    T m_storage[N];
    T* m_ptr;
    const size_t m_count;

public:
    AutoSTArray(size_t n) : m_count(n)
    {
        m_ptr = m_storage;
        if (n > N)
        {
            m_ptr = new T[n];
        }
    }
    ~AutoSTArray()
    {
        if (m_ptr != m_storage)
        {
            delete[] m_ptr;
        }
    }

    size_t size() const { return m_count; }
    int count() const { return rive::castTo<int>(m_count); }

    T* data() const { return m_ptr; }

    T& operator[](size_t index)
    {
        assert(index < m_count);
        return m_ptr[index];
    }
};

constexpr inline uint32_t make_tag(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (a << 24) | (b << 16) | (c << 8) | d;
}

static inline std::string tag2str(uint32_t tag)
{
    std::string str = "abcd";
    str[0] = (tag >> 24) & 0xFF;
    str[1] = (tag >> 16) & 0xFF;
    str[2] = (tag >> 8) & 0xFF;
    str[3] = (tag >> 0) & 0xFF;
    return str;
}

#endif
