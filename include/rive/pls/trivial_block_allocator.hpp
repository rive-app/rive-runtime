/*
 * Copyright 2023 Rive
 */

#pragma once

#include <cassert>
#include <memory>
#include <vector>

namespace rive
{
// Fast block allocator for trivially-destructible types.
class TrivialBlockAllocator
{
public:
    TrivialBlockAllocator(size_t initialBlockSize)
    {
        // Make it so the second allocation doubles initialBlockSize once it happens.
        m_currentBlockSize = initialBlockSize;
        pushBlock(initialBlockSize);
    }

    void* alloc(size_t size)
    {
        // Align all allocations on 8-byte boundaries.
        size = (size + 7) & ~size_t(7);
        if (m_currentBlockUsage + size > m_currentBlockSize)
        {
            // Grow with a fibonacci function.
            pushBlock(std::max(m_currentBlockSize + m_lastBlockSize, size));
        }
        void* ret = &m_blocks.back()[m_currentBlockUsage];
        m_currentBlockUsage += size;
        // Since we round up all allocation sizes, allocations should be 8-byte aligned.
        assert((reinterpret_cast<uintptr_t>(ret) & 7) == 0);
        return ret;
    }

    template <typename T, typename... Args> T* make(Args&&... args)
    {
        // We don't call destructors on objects we allocate here.
        static_assert(std::is_trivially_destructible<T>::value);
        return new (alloc(sizeof(T))) T(std::forward<Args>(args)...);
    }

private:
    void pushBlock(size_t size)
    {
        m_blocks.push_back(std::unique_ptr<uint8_t[]>(new uint8_t[size]));
        m_lastBlockSize = m_currentBlockSize;
        m_currentBlockSize = size;
        m_currentBlockUsage = 0;
    }

    std::vector<std::unique_ptr<uint8_t[]>> m_blocks;
    size_t m_lastBlockSize;
    size_t m_currentBlockSize;
    size_t m_currentBlockUsage;
};
} // namespace rive
