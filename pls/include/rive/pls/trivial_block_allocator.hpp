/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/math/math_types.hpp"
#include <cassert>
#include <memory>
#include <vector>

namespace rive
{
// Fast block allocator for trivially-destructible types.
class TrivialBlockAllocator
{
public:
    TrivialBlockAllocator(size_t initialBlockSize) : m_initialBlockSize(initialBlockSize)
    {
        m_blocks.push_back(std::unique_ptr<char[]>(new char[m_initialBlockSize]));
        reset();
    }

    void reset()
    {
        m_fibMinus2 = 0;
        m_fibMinus1 = 1;
        m_blocks.resize(1);
        m_currentBlockSize = m_initialBlockSize;
        m_currentBlockUsage = 0;
    }

    template <size_t AlignmentInBytes = 8> void* alloc(size_t sizeInBytes)
    {
        uintptr_t start = reinterpret_cast<uintptr_t>(m_blocks.back().get()) + m_currentBlockUsage;
        size_t alignmentPad = math::round_up_to_multiple_of<AlignmentInBytes>(start) - start;

        // Ensure there is room for this allocation in our newest block, pushing a new one if
        // needed.
        if (m_currentBlockUsage + alignmentPad + sizeInBytes > m_currentBlockSize)
        {
            // Grow with a fibonacci function.
            size_t fib = m_fibMinus2 + m_fibMinus1;
            m_fibMinus2 = m_fibMinus1;
            m_fibMinus1 = fib;

            size_t blockSize =
                std::max(fib * m_initialBlockSize, sizeInBytes + AlignmentInBytes - 1);
            m_blocks.push_back(std::unique_ptr<char[]>(new char[blockSize]));
            m_currentBlockSize = blockSize;
            m_currentBlockUsage = 0;

            start = reinterpret_cast<uintptr_t>(m_blocks.back().get());
            alignmentPad = math::round_up_to_multiple_of<AlignmentInBytes>(start) - start;
        }

        char* ret = &m_blocks.back()[m_currentBlockUsage + alignmentPad];
        m_currentBlockUsage += alignmentPad + sizeInBytes;
        assert((reinterpret_cast<uintptr_t>(ret) % AlignmentInBytes) == 0);
        assert(ret + sizeInBytes <= m_blocks.back().get() + m_currentBlockSize);
        return ret;
    }

    void rewindLastAllocation(size_t rewindSizeInBytes)
    {
        assert(rewindSizeInBytes <= m_currentBlockUsage);
        m_currentBlockUsage -= rewindSizeInBytes;
    }

    template <typename T, typename... Args> T* make(Args&&... args)
    {
        // We don't call destructors on objects that get allocated here. We just free the blocks
        // at the end. So objects must be trivially destructible.
        static_assert(std::is_trivially_destructible<T>::value);
        return new (alloc<alignof(T)>(sizeof(T))) T(std::forward<Args>(args)...);
    }

private:
    const size_t m_initialBlockSize;

    // Grow block sizes using a fibonacci function.
    size_t m_fibMinus2;
    size_t m_fibMinus1;

    std::vector<std::unique_ptr<char[]>> m_blocks;
    size_t m_currentBlockSize;
    size_t m_currentBlockUsage;
};

// Basic array allocator for POD types, based on TrivialBlockAllocator.
template <typename T, size_t AlignmentInBytes = alignof(T)>
class TrivialArrayAllocator : private TrivialBlockAllocator
{
    static_assert(std::is_pod<T>::value);

public:
    TrivialArrayAllocator(size_t initialCount) : TrivialBlockAllocator(initialCount * sizeof(T)) {}

    T* alloc(size_t count)
    {
        return reinterpret_cast<T*>(TrivialBlockAllocator::alloc(count * sizeof(T)));
    }

    void rewindLastAllocation(size_t rewindCount)
    {
        TrivialBlockAllocator::rewindLastAllocation(rewindCount * sizeof(T));
    }

    using TrivialBlockAllocator::reset;
};

// Simple linked list whose nodes are allocated on a TrivialBlockAllocator.
template <typename T> class BlockAllocatedLinkedList
{
public:
    size_t count() const
    {
        assert(static_cast<bool>(m_head) == static_cast<bool>(m_tail));
        assert(static_cast<bool>(m_head) == static_cast<bool>(m_tail));
        return m_count;
    }

    bool empty() const { return count() == 0; }

    T& tail() const
    {
        assert(!empty());
        return m_tail->data;
    }

    template <typename... Args> T& emplace_back(TrivialBlockAllocator& allocator, Args... args)
    {
        Node* node = allocator.make<Node>(std::forward<Args>(args)...);
        assert(static_cast<bool>(m_head) == static_cast<bool>(m_tail));
        if (m_head == nullptr)
        {
            m_head = node;
        }
        else
        {
            m_tail->next = node;
        }
        m_tail = node;
        ++m_count;
        return m_tail->data;
    }

    void reset()
    {
        m_tail = nullptr;
        m_head = nullptr;
        m_count = 0;
    }

    struct Node
    {
        template <typename... Args> Node(Args... args) : data(std::forward<Args>(args)...) {}
        T data;
        Node* next = nullptr;
    };

    template <typename U> class Iter
    {
    public:
        Iter(Node* current) : m_current(current) {}
        bool operator!=(const Iter& other) const { return m_current != other.m_current; }
        void operator++() { m_current = m_current->next; }
        U& operator*() { return m_current->data; }

    private:
        Node* m_current;
    };
    Iter<T> begin() { return {m_head}; }
    Iter<T> end() { return {nullptr}; }
    Iter<const T> begin() const { return {m_head}; }
    Iter<const T> end() const { return {nullptr}; }

private:
    Node* m_head = nullptr;
    Node* m_tail = nullptr;
    size_t m_count = 0;
};

} // namespace rive
