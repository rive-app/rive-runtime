/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/math/math_types.hpp"
#include <cassert>
#include <deque>

namespace rive
{
// Stream for recording objects of a specific type, using C++-style "<<" ">>"
// operators.
template <typename T> class ObjectStream
{
public:
    bool empty() const { return m_stream.empty(); }

    ObjectStream& operator<<(T obj)
    {
        m_stream.push_back(std::move(obj));
        return *this;
    }

    ObjectStream& operator>>(T& dst)
    {
        assert(!empty());
        dst = std::move(m_stream.front());
        m_stream.pop_front();
        return *this;
    }

private:
    std::deque<T> m_stream;
};

// Stream for recording objects of any trivially-copyable type, using C++-style
// "<<" ">>" operators. Object types must be read back in the same order they
// were writen.
class TrivialObjectStream
{
public:
    bool empty() const { return m_byteStream.empty(); }

    template <typename T> TrivialObjectStream& operator<<(T obj)
    {
        static_assert(
            std::is_trivially_copyable<T>(),
            "TrivialObjectStream only accepts trivially copyable types");
        char* ptr = &(*m_byteStream.insert(m_byteStream.end(), sizeof(T), 0));
        if (uintptr_t pad = math::padding_to_align_up<T>(ptr))
        {
            // Recalculate 'ptr' based on the iterator. If the deque grew with
            // this operation, the old ptr will be invalid.
            ptr = &(*m_byteStream.insert(m_byteStream.end(), pad, 0)) + pad -
                  sizeof(T);
        }
        assert(reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0);
        new (ptr) T(std::move(obj));
        return *this;
    }

    template <typename T> TrivialObjectStream& operator>>(T& dst)
    {
        static_assert(
            std::is_pod<T>(),
            "TrivialObjectStream only accepts trivially copyable types");
        assert(!empty());
        char* ptr = &m_byteStream.front();
        if (uintptr_t pad = math::padding_to_align_up<T>(&m_byteStream.front()))
        {
            assert(m_byteStream.size() >= pad);
            m_byteStream.erase(m_byteStream.begin(),
                               m_byteStream.begin() + pad);
            ptr = &m_byteStream.front();
        }
        assert(reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0);
        auto obj = reinterpret_cast<T*>(ptr);
        dst = std::move(*obj);
        obj->~T();
        m_byteStream.erase(m_byteStream.begin(),
                           m_byteStream.begin() + sizeof(T));
        return *this;
    }

private:
    std::deque<char> m_byteStream;
};
}; // namespace rive
