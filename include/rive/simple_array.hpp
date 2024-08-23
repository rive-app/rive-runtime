/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SIMPLE_ARRAY_HPP_
#define _RIVE_SIMPLE_ARRAY_HPP_

#include "rive/rive_types.hpp"

#include <initializer_list>
#include <type_traits>
#include <cstring>
#include <iterator>

namespace rive
{

template <typename T> class SimpleArrayBuilder;

#ifdef TESTING
namespace SimpleArrayTesting
{
extern int mallocCount;
extern int reallocCount;
extern int freeCount;
void resetCounters();
} // namespace SimpleArrayTesting
#endif

// Helper for constructing and destructing arrays of objects.
template <typename T, bool IsPOD = std::is_pod<T>()> class SimpleArrayHelper
{
public:
    static_assert(!std::is_pod<T>(), "This helper is for non-POD types.");
    static void DefaultConstructArray(T* ptr, T* end)
    {
        for (; ptr < end; ++ptr)
            new (ptr) T();
    }
    static void CopyConstructArray(const T* first, const T* end, T* ptr)
    {
        for (; first < end; ++first, ++ptr)
            new (ptr) T(*first);
    }
    static void DestructArray(T* ptr, T* end)
    {
        for (; ptr < end; ++ptr)
            ptr->~T();
    }
};

// Specialized helper for constructing and destructing arrays of POD objects.
template <typename T> class SimpleArrayHelper<T, true>
{
public:
    static_assert(std::is_pod<T>(), "This helper is only for POD types.");
    static void DefaultConstructArray(T* ptr, T* end) {}
    static void CopyConstructArray(const T* first, const T* end, T* ptr)
    {
        memcpy(ptr, first, reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(first));
    }
    static void DestructArray(T* ptr, T* end) {}
};

/// Lightweight heap array meant to be used when knowing the exact memory layout
/// of the simple array is necessary, like marshaling the data to Dart/C#/Wasm.
/// Note that it intentionally doesn't have push/add/resize functionality as
/// that's reserved for a special case builder that should be to build up the
/// array. This saves the structure from needing to store extra ptrs and keeps
/// it optimally sized for marshaling. See SimpleArrayBuilder<T> below for push
/// functionality.

template <typename T> class SimpleArray
{
public:
    SimpleArray() : m_ptr(nullptr), m_size(0) {}
    SimpleArray(size_t size) : m_ptr(static_cast<T*>(malloc(size * sizeof(T)))), m_size(size)
    {
        SimpleArrayHelper<T>::DefaultConstructArray(m_ptr, m_ptr + m_size);
#ifdef TESTING
        SimpleArrayTesting::mallocCount++;
#endif
    }
    SimpleArray(const T* ptr, size_t size) : SimpleArray(size)
    {
        assert(ptr <= ptr + size);
        SimpleArrayHelper<T>::CopyConstructArray(ptr, ptr + size, m_ptr);
    }

    constexpr SimpleArray(const SimpleArray<T>& other) : SimpleArray(other.m_ptr, other.m_size) {}

    SimpleArray(SimpleArray<T>&& other) : m_ptr(other.m_ptr), m_size(other.m_size)
    {
        other.m_ptr = nullptr;
        other.m_size = 0;
    }

    SimpleArray(SimpleArrayBuilder<T>&& other);

    SimpleArray<T>& operator=(const SimpleArray<T>& other) = delete;

    SimpleArray<T>& operator=(SimpleArray<T>&& other)
    {
        SimpleArrayHelper<T>::DestructArray(m_ptr, m_ptr + m_size);
        free(m_ptr);
        m_ptr = other.m_ptr;
        m_size = other.m_size;
        other.m_ptr = nullptr;
        other.m_size = 0;
        return *this;
    }

    SimpleArray<T>& operator=(SimpleArrayBuilder<T>&& other);

    template <typename Container>
    constexpr SimpleArray(Container& c) : SimpleArray(c.data(), c.size())
    {}
    constexpr SimpleArray(const std::initializer_list<T>& il) : SimpleArray(il.begin(), il.size())
    {}
    ~SimpleArray()
    {
        SimpleArrayHelper<T>::DestructArray(m_ptr, m_ptr + m_size);
        free(m_ptr);
#ifdef TESTING
        SimpleArrayTesting::freeCount++;
#endif
    }

    T& operator[](size_t index) const { return m_ptr[index]; }

    constexpr T* data() const { return m_ptr; }
    constexpr size_t size() const { return m_size; }
    constexpr bool empty() const { return m_size == 0; }

    constexpr T* begin() const { return m_ptr; }
    constexpr T* end() const { return m_ptr + m_size; }

    constexpr T& front() const { return (*this)[0]; }
    constexpr T& back() const { return (*this)[m_size - 1]; }

    // returns byte-size of the entire simple array
    constexpr size_t size_bytes() const { return m_size * sizeof(T); }

    // Makes rive::SimpleArray std::Container compatible
    // https://en.cppreference.com/w/cpp/named_req/Container
    typedef typename std::remove_cv<T>::type value_type;
    typedef T& reference;
    typedef T const& const_reference;
    typedef T* iterator;
    typedef T const* const_iterator;
    typedef std::ptrdiff_t difference_type;
    typedef size_t size_type;

protected:
    T* m_ptr;
    size_t m_size;
};

/// Extension of SimpleArray which can progressively expand as contents are
/// pushed/added/written to it. Can be released as a simple SimpleArray.
template <typename T> class SimpleArrayBuilder : public SimpleArray<T>
{
    friend class SimpleArray<T>;

public:
    SimpleArrayBuilder(size_t reserve) : SimpleArray<T>(reserve)
    {
        assert(this->m_ptr <= this->m_ptr + this->m_size);
        m_write = this->m_ptr;
    }

    SimpleArrayBuilder() : SimpleArrayBuilder(0) {}

    void add(const T& value)
    {
        growToFit();
        *m_write++ = value;
    }

    void add(T&& value)
    {
        growToFit();
        *m_write++ = std::move(value);
    }

    // Allows iterating just the written content.
    constexpr size_t capacity() const { return this->m_size; }
    constexpr size_t size() const { return m_write - this->m_ptr; }
    constexpr bool empty() const { return size() == 0; }
    constexpr T* begin() const { return this->m_ptr; }
    constexpr T* end() const { return m_write; }

    constexpr T& front() const { return (*this)[0]; }
    constexpr T& back() const { return *(m_write - 1); }

private:
    void growToFit()
    {
        if (m_write == this->m_ptr + this->m_size)
        {
            auto writeOffset = m_write - this->m_ptr;
            this->resize(std::max((size_t)1, this->m_size * 2));
            m_write = this->m_ptr + writeOffset;
        }
    }

    void resize(size_t size)
    {
        if (size == this->m_size)
        {
            return;
        }
#ifdef TESTING
        SimpleArrayTesting::reallocCount++;
#endif
        // Call destructor for elements when sizing down.
        SimpleArrayHelper<T>::DestructArray(this->m_ptr + size, this->m_ptr + this->m_size);
        this->m_ptr = static_cast<T*>(realloc(this->m_ptr, size * sizeof(T)));
        // Call constructor for elements when sizing up.
        SimpleArrayHelper<T>::DefaultConstructArray(this->m_ptr + this->m_size, this->m_ptr + size);
        this->m_size = size;
    }

    T* m_write;
};

template <typename T> SimpleArray<T>::SimpleArray(SimpleArrayBuilder<T>&& other)
{
    // Bring the capacity down to the actual size (this should keep the same
    // ptr, but that's not guaranteed, so we copy the ptr after the realloc).
    other.resize(other.size());
    m_ptr = other.m_ptr;
    m_size = other.m_size;
    other.m_ptr = nullptr;
    other.m_size = 0;
}

template <typename T> SimpleArray<T>& SimpleArray<T>::operator=(SimpleArrayBuilder<T>&& other)
{
    other.resize(other.size());
    this->m_ptr = other.m_ptr;
    this->m_size = other.m_size;
    other.m_ptr = nullptr;
    other.m_size = 0;
    return *this;
}

} // namespace rive

#endif
