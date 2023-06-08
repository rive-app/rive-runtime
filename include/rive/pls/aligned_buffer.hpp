/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/rive_types.hpp"
#include  <stdlib.h>

namespace rive::pls
{
// Wraps a memory allocation whose address is aligned at a multiple of
// "AlignmentInElements * sizeof(T)" bytes.
template <size_t AlignmentInElements, typename T> class AlignedBuffer
{
public:
    size_t capacity() const { return m_capacity; }
    RIVE_ALWAYS_INLINE T& operator[](size_t idx) { return m_buffer[idx]; }
    RIVE_ALWAYS_INLINE T* get() { return m_buffer; }
    template <typename U> U* cast() { return reinterpret_cast<U*>(m_buffer); }

    void resize(size_t capacity)
    {
        if (m_capacity != capacity)
        {
            // Align at 128 bits for optimal SSE/NEON loads.
            free(m_buffer);
            m_buffer = reinterpret_cast<T*>(
                _aligned_malloc(capacity * sizeof(T), AlignmentInElements * sizeof(T)));
            m_capacity = capacity;
        }
    }

    ~AlignedBuffer() { free(m_buffer); }

private:
    size_t m_capacity = 0;
    T* m_buffer = nullptr;
};
} // namespace rive::pls
