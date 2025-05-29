/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

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
class PODStream
{
public:
    bool empty() const { return m_byteStream.empty(); }

    template <typename T> PODStream& operator<<(T obj)
    {
        static_assert(std::is_pod<T>(),
                      "PODStream only accepts plain-old-data types");
        const char* data = reinterpret_cast<const char*>(&obj);
        m_byteStream.insert(m_byteStream.end(), data, data + sizeof(T));
        return *this;
    }

    template <typename T> PODStream& operator>>(T& dst)
    {
        static_assert(std::is_pod<T>(),
                      "PODStream only accepts plain-old-data types");
        assert(m_byteStream.size() >= sizeof(T));
        char* data = reinterpret_cast<char*>(&dst);
        std::copy(m_byteStream.begin(), m_byteStream.begin() + sizeof(T), data);
        m_byteStream.erase(m_byteStream.begin(),
                           m_byteStream.begin() + sizeof(T));
        return *this;
    }

    template <typename T> PODStream& operator<<(rcp<T> obj)
    {
        return *this << obj.release();
    }

    template <typename T> PODStream& operator>>(rcp<T>& obj)
    {
        T* raw;
        *this >> raw;
        obj = rcp<T>(raw);
        return *this;
    }

private:
    std::deque<char> m_byteStream;
};
}; // namespace rive
