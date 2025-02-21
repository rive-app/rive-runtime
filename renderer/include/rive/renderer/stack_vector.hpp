/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/math/math_types.hpp"

namespace rive
{

template <typename T, uint32_t MAX_CAPACITY> class StackVector
{
public:
    void clear() { m_size = 0; }

    const T& operator[](uint32_t index) const
    {
        assert(index < m_size);
        return m_data[index];
    }

    T& operator[](uint32_t index)
    {
        assert(index < m_size);
        return m_data[index];
    }

    T& push_back(const T& ele)
    {
        T* ret = push(1);
        *ret = ele;
        return *ret;
    }

    T* push_back_n(uint32_t numEles, const T* srcData)
    {
        T* dst = push(numEles);
        if (srcData != nullptr)
        {
            memcpy(dst, srcData, numEles * sizeof(T));
        }
        return dst;
    }

    const T* data() const { return m_data; }
    const uint32_t size() const { return m_size; }

private:
    uint32_t m_size = 0;
    T m_data[MAX_CAPACITY];

    bool hasRoomFor(size_t itemCount)
    {
        return m_size + itemCount <= MAX_CAPACITY;
    }

    T* push(size_t count)
    {
        assert(hasRoomFor(count));
        T* ret = &m_data[m_size];
        m_size += count;
        return ret;
    }

    static_assert(std::is_pod<T>::value ==
                  true); // Currently only supports POD types
};
} // namespace rive
