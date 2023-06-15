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
    TrivialBlockAllocator(size_t initialBlockSize) : m_initialBlockSize(initialBlockSize) {}

    void* alloc(size_t allocSize)
    {
        // Align all allocations on 8-byte boundaries.
        allocSize = (allocSize + 7) & ~size_t(7);

        // Ensure there is room for this allocation in our newest block, pushing a new one if
        // needed.
        if (m_currentBlockUsage + allocSize > m_currentBlockSize)
        {
            // Grow with a fibonacci function.
            size_t fib = m_fibMinus2 + m_fibMinus1;
            m_fibMinus2 = m_fibMinus1;
            m_fibMinus1 = fib;

            size_t blockSize = std::max(fib * m_initialBlockSize, allocSize);
            m_blocks.push_back(std::unique_ptr<char[]>(new char[blockSize]));
            m_currentBlockSize = blockSize;
            m_currentBlockUsage = 0;
        }

        char* ret = &m_blocks.back()[m_currentBlockUsage];
        m_currentBlockUsage += allocSize;
        // Since we round up all allocation sizes, allocations should be 8-byte aligned.
        assert((reinterpret_cast<uintptr_t>(ret) & 7) == 0);
        assert(ret + allocSize <= m_blocks.back().get() + m_currentBlockSize);
        return ret;
    }

    template <typename T, typename... Args> T* make(Args&&... args)
    {
        // We don't call destructors on objects that get allocated here. We just free the blocks
        // at the end. So objects must be trivially destructible.
        static_assert(std::is_trivially_destructible<T>::value);
        return new (alloc(sizeof(T))) T(std::forward<Args>(args)...);
    }

private:
    const size_t m_initialBlockSize;

    // Grow block sizes using a fibonacci function.
    size_t m_fibMinus2 = 0;
    size_t m_fibMinus1 = 1;

    std::vector<std::unique_ptr<char[]>> m_blocks;
    size_t m_currentBlockSize = 0;
    size_t m_currentBlockUsage = 0;
};
} // namespace rive
