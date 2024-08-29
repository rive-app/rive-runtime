#ifndef _RIVE_MAC_UTILS_HPP_
#define _RIVE_MAC_UTILS_HPP_

#include "rive/rive_types.hpp"
#include "rive/span.hpp"
#include "utils/auto_cf.hpp"
#include <string>

#ifdef RIVE_BUILD_FOR_APPLE

template <size_t N, typename T> class AutoSTArray
{
    T m_storage[N];
    T* m_ptr;
    const size_t m_count;

public:
    AutoSTArray(size_t n) : m_count(n)
    {
        m_ptr = m_storage;
        if (n > N)
        {
            m_ptr = new T[n];
        }
    }
    ~AutoSTArray()
    {
        if (m_ptr != m_storage)
        {
            delete[] m_ptr;
        }
    }

    T* data() const { return m_ptr; }

    T& operator[](size_t index)
    {
        assert(index < m_count);
        return m_ptr[index];
    }
};

constexpr inline uint32_t make_tag(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (a << 24) | (b << 16) | (c << 8) | d;
}

static inline std::string tag2str(uint32_t tag)
{
    std::string str = "abcd";
    str[0] = (tag >> 24) & 0xFF;
    str[1] = (tag >> 16) & 0xFF;
    str[2] = (tag >> 8) & 0xFF;
    str[3] = (tag >> 0) & 0xFF;
    return str;
}

static inline float find_float(CFDictionaryRef dict, const void* key)
{
    auto num = (CFNumberRef)CFDictionaryGetValue(dict, key);
    assert(num);
    float value = 0;
    CFNumberGetValue(num, kCFNumberFloat32Type, &value);
    return value;
}

static inline uint32_t find_u32(CFDictionaryRef dict, const void* key)
{
    auto num = (CFNumberRef)CFDictionaryGetValue(dict, key);
    assert(num);
    assert(!CFNumberIsFloatType(num));
    uint32_t value = 0;
    CFNumberGetValue(num, kCFNumberSInt32Type, &value);
    return value;
}

static inline uint32_t number_as_u32(CFNumberRef num)
{
    uint32_t value;
    CFNumberGetValue(num, kCFNumberSInt32Type, &value);
    return value;
}

static inline float number_as_float(CFNumberRef num)
{
    float value;
    CFNumberGetValue(num, kCFNumberFloat32Type, &value);
    return value;
}

#endif
#endif
