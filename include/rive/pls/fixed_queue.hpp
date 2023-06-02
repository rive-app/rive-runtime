/*
 * Copyright 2022 Rive
 */

#pragma once

namespace rive::pls
{
// Fast, simple queue that does not make intermediate memory allocations. The capacity is set during
// resizeAndRewind(), and push_back() may only be called up to "m_capacity" times before the queue
// must be rewound again.
template <typename T> class FixedQueue
{
public:
    void resizeAndRewind(size_t newCapacity)
    {
        if (newCapacity != m_capacity)
        {
            delete[] m_data;
            m_data = new T[newCapacity];
            m_capacity = newCapacity;
        }
        rewind();
    }
    void rewind() { m_front = m_end = m_data; }

    size_t capacity() const { return m_capacity; }

    T& push_back()
    {
        assert(m_end < m_data + m_capacity);
        return *m_end++;
    }

    T& push_back(const T& t) { return push_back() = t; }

    T* push_back_n(size_t n)
    {
        assert(m_end + n <= m_data + m_capacity);
        T* ptr = m_end;
        m_end += n;
        return ptr;
    }

    const T& pop_front()
    {
        assert(m_front < m_end);
        return *m_front++;
    }

    const T* pop_front_n(size_t n)
    {
        assert(m_front + n <= m_end);
        const T* ptr = m_front;
        m_front += n;
        return ptr;
    }

private:
    size_t m_capacity = 0;
    T* m_data = nullptr;
    T* m_front = nullptr;
    T* m_end = nullptr;
};
} // namespace rive::pls
