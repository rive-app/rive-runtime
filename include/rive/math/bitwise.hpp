/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_BITWISE_HPP_
#define _RIVE_BITWISE_HPP_

#include <type_traits>

#include "rive/rive_types.hpp"
#include "rive/enums.hpp"
#include "rive/math/math_types.hpp"

namespace rive::math
{

// Attempt to generate a "clz" assembly instruction.
RIVE_ALWAYS_INLINE static int clz32(uint32_t x)
{
    assert(x != 0);
#if __has_builtin(__builtin_clz)
    return __builtin_clz(x);
#else
    uint64_t doubleBits = bit_cast<uint64_t>(static_cast<double>(x));
    return 1054 - (doubleBits >> 52);
#endif
}

RIVE_ALWAYS_INLINE static int clz64(uint64_t x)
{
    assert(x != 0);
#if __has_builtin(__builtin_clzll)
    return __builtin_clzll(x);
#else
    uint32_t hi32 = x >> 32;
    return hi32 != 0 ? clz32(hi32) : 32 + clz32(x & 0xffffffff);
#endif
}

// Returns the 1-based index of the most significat bit in x.
//
//   0    -> 0
//   1    -> 1
//   2..3 -> 2
//   4..7 -> 3
//   ...
//
RIVE_ALWAYS_INLINE static uint32_t msb(uint32_t x)
{
    return x != 0 ? 32 - clz32(x) : 0;
}

// Attempt to generate a "rotl" (rotate-left) assembly instruction.
RIVE_ALWAYS_INLINE static uint32_t rotateleft32(uint32_t x, int y)
{
#if __has_builtin(__builtin_rotateleft32)
    return __builtin_rotateleft32(x, y);
#else
    return (x << y) | (x >> (32 - y));
#endif
}

namespace internal
{
// For compilers where there isn't a built-in popcount function, here's the
// simplest efficient implementation. This is done here in a namespace so that
// it can be unit tested even on systems that have built-in popcount support.
template <typename I> constexpr uint32_t count_set_bits_fallback(I i) noexcept
{
    uint32_t count = 0;
    while (i != 0)
    {
        count++;

        // remove the lowest set bit from the given value
        i &= i - 1;
    }
    return count;
}

} // namespace internal

template <typename T> constexpr uint32_t count_set_bits(T i) noexcept
{
#if __has_builtin(__builtin_popcount)
    static_assert(std::is_integral_v<T>);
    if constexpr (sizeof(T) == sizeof(uint64_t))
    {
        return uint32_t(__builtin_popcountll(uint64_t(i)));
    }
    else
    {
        return uint32_t(__builtin_popcount(uint32_t(i)));
    }
#else
    return internal::count_set_bits_fallback(i);
#endif
}

// Give a value with bits set within a given mask, compact the bits down to the
// minimum number needed to represent the values that are set in the mask.
//  For example: a mask of 11010001 has 4 set bits, and so a value of 10x0xxx1
//   (where x is any bit value not within the mask) would be compacted to 1001
constexpr uint32_t compact_bitmask_value(uint32_t value, uint32_t mask) noexcept
{
    uint32_t compacted = 0;
    for (int32_t inBitIndex = 31; inBitIndex >= 0; inBitIndex--)
    {
        auto bit = 1u << inBitIndex;
        if ((mask & bit) != 0)
        {
            compacted <<= 1;
            compacted |= ((value & bit) != 0) ? 1 : 0;
        }
    }

    return compacted;
}

// Undo the operation that compactBitmaskValue does.
//  For example: a mask of 11010001, with a given compacted vaue of 1001 would
//  expand to 10000001 (where each 1 in "compacted" means setting the
//  corresponding bit from mask)
constexpr uint32_t expand_compacted_bitmask_value(uint32_t compacted,
                                                  uint32_t mask) noexcept
{
    uint32_t expanded = 0;
    for (int32_t maskBitIndex = 0; maskBitIndex < 32; maskBitIndex++)
    {
        auto maskBit = 1u << maskBitIndex;
        if ((mask & maskBit) != 0)
        {
            if (compacted & 1)
            {
                expanded |= maskBit;
            }
            compacted >>= 1;
        }
    }

    return expanded;
}

// Iteratable returned from iterateBitCombinationsInMask
template <typename T> class BitCombinationIterable
{
public:
    explicit constexpr BitCombinationIterable(T mask) : m_mask(mask) {}

    class Iterator
    {
    public:
        explicit constexpr Iterator(T mask) noexcept :
            m_currentValue(mask), m_mask(mask)
        {}

        constexpr static Iterator MakeEnd(T mask) noexcept
        {
            Iterator it{mask};
            it.m_wasAdvanced = true;
            return it;
        }

        constexpr Iterator& operator++() noexcept
        {
            m_wasAdvanced = true;

            // Do this operation as an unsigned integral version of the type.
            // For enums this allows the subtraction below to work, and for
            // signed integers this allows us to do wrapping subtraction
            // (subtracting from 0) without it being undefined behavior.
            // NOTE: The odd "enable_if" in there is a trick to delay evaluation
            // of underlying_type<T> until the conditional gets chosen, because
            // otherwise this won't compile for non-enum types
            using U = std::make_unsigned_t<
                typename std::conditional_t<std::is_enum_v<T>,
                                            std::underlying_type<T>,
                                            std::enable_if<true, T>>::type>;

            // We actually iterate through these combinations from most bits to
            // least bits because it's simpler. (Iterating forward can be
            // accomplished by starting at 0, and xoring with the mask before
            // and after this decrement, but it seemed unnecessary since it's
            // unlikely that the ordering matters anywhere)

            // This subtraction and mask effectively removes the lowest set bit
            // in the mask from the current value (and re-sets and mask bits
            // below it), that is with a mask of 01001010 and a current value of
            // 01001000, subtracting would give 01000111, then the mask leaves
            // it as 01000010, which is the next-lowest valid combination of
            // mask bits from where we were.
            m_currentValue = T(U(m_currentValue) - 1) & m_mask;

            return *this;
        }

        constexpr Iterator operator++(int) noexcept
        {
            auto prev = *this;
            ++(*this);
            return prev;
        }

        constexpr T operator*() const noexcept { return m_currentValue; }

        constexpr bool operator==(const Iterator& other) const noexcept
        {
            assert(m_mask == other.m_mask);
            return (m_currentValue == other.m_currentValue &&
                    m_wasAdvanced == other.m_wasAdvanced);
        }

        constexpr bool operator!=(const Iterator& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        T m_currentValue{};
        T m_mask{};
        bool m_wasAdvanced = false;
    };

    constexpr Iterator begin() const noexcept { return Iterator{m_mask}; }
    constexpr Iterator end() const noexcept
    {
        return Iterator::MakeEnd(m_mask);
    }

private:
    T m_mask;
};

// Iterate through all combinations of set/unset bits for a given mask.
// That is, for a mask that is 101011, it would iterate through:
// 101011, 101010, 101001, 101000, 100011, ...,  000001, 000000
template <typename T>
BitCombinationIterable<T> iterate_bit_combinations_in_mask(T mask)
{
    return BitCombinationIterable{mask};
}

// Shift bits into the bottom of a key value (ensuring that the key value does
// not overflow and the value fits within the specified bit count)
template <typename Key, typename Bits>
[[nodiscard]] Key add_bits_to_key(Key key,
                                  Bits bits,
                                  uint32_t bitCount) noexcept
{
    static_assert(sizeof(Bits) <= sizeof(Key));

    assert((key << bitCount) >> bitCount == key);
    assert((Key(bits) & ((1ull << bitCount) - 1)) == Key(bits));
    return (key << bitCount) | Key(bits);
}

} // namespace rive::math
#endif
