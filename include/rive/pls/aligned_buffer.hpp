/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/rive_types.hpp"

#ifdef __clang__
#include <mm_malloc.h>
#else
#include <cstdlib>
#endif

namespace rive::pls
{
// Wraps a memory allocation whose address is aligned at a multiple of
// "AlignmentInElements * sizeof(T)" bytes.
// This allows for optimal SSE/NEON loads.
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
            freeBuffer();
            // Round "capacity" up to a multiple of "AlignmentInElements".
            capacity =
                ((capacity + AlignmentInElements - 1) / AlignmentInElements) * AlignmentInElements;
#if defined(__clang__)
            m_buffer = reinterpret_cast<T*>(
                _mm_malloc(capacity * sizeof(T), AlignmentInElements * sizeof(T)));
#elif defined(_WIN32)
            m_buffer = reinterpret_cast<T*>(
                _aligned_malloc(capacity * sizeof(T), AlignmentInElements * sizeof(T)));
#else
            m_buffer = reinterpret_cast<T*>(
                std::aligned_alloc(AlignmentInElements * sizeof(T), capacity * sizeof(T)));
#endif
            m_capacity = capacity;
        }
    }

    ~AlignedBuffer() { freeBuffer(); }

private:
    void freeBuffer()
    {
#if defined(__clang__)
        _mm_free(m_buffer);
#elif defined(_WIN32)
        _aligned_free(m_buffer);
#else
        std::free(m_buffer);
#endif
    }

    size_t m_capacity = 0;
    T* m_buffer = nullptr;
};
} // namespace rive::pls
