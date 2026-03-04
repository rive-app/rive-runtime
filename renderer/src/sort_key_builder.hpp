#pragma once

#include "rive/renderer.hpp"
#include "rive/renderer/gpu.hpp"

#include <array>

// Unit tests override RIVE_ENABLE_SORT_KEY_VALIDATION to enable it even in
// release builds so we can always test validation.
#if !defined(RIVE_ENABLE_SORT_KEY_VALIDATION)
#if defined(NDEBUG)
#define RIVE_ENABLE_SORT_KEY_VALIDATION 0
#else
#define RIVE_ENABLE_SORT_KEY_VALIDATION 1
#endif
#endif

namespace rive::gpu
{
// The order of the values in the enum does not matter - the prioritization
// order is set at SortKeyBuilder construction time.
enum class SortEntry
{
    blendMode,
    drawContents,
    drawIndex,
    drawGroup,
    drawType,
    subpassIndex,
    textureHash,
};
constexpr uint32_t SORT_ENTRY_COUNT = 7;

namespace internal
{
// Note: this function is not constexpr, specifically so that compilation will
// fail when called at compile time. If it is called at runtime, it will assert
// with the given message.
inline void FailConstexpr(const char* message)
{
// the unit tests declare this macro so we can catch test failures more cleanly.
#ifdef TESTING_ASSERT
    TESTING_ASSERT(false, message);
#else
    assert(false && message);
#endif
}
} // namespace internal

// Make an "assert" that will cause constexpr compilation to fail if the
// condition is false (regardless of whether asserts are enabled or not)
// This will work like an assert at runtime.
#define RIVE_CONSTEXPR_CHECK(v, message)                                       \
    do                                                                         \
    {                                                                          \
        if (!(v))                                                              \
        {                                                                      \
            internal::FailConstexpr(message);                                  \
        }                                                                      \
    } while (false)

enum class ValidateKeyEntry : bool
{
    no,
    yes,
};

// Helper class to build the draw sort key for the render context. It is
// intended to be initialized at compile time, which will pre-bake the masks and
// shifts for each element into compile-time constants (rather than buliding
// them up at runtime)
class SortKeyBuilder
{
public:
    struct Initializer
    {
        SortEntry entry;
        uint32_t bitCount;
    };

    struct Value
    {
        // Allow constructing a Value with any type (that can cast to uint64_t)
        template <typename T>
        constexpr Value(SortEntry e,
                        T v,
                        ValidateKeyEntry vk = ValidateKeyEntry::yes) :
            entry(e), value(uint64_t(v)), validate(vk)
        {}

        SortEntry entry;
        uint64_t value;
        ValidateKeyEntry validate;
    };

    constexpr SortKeyBuilder(const std::initializer_list<Initializer>& init)
    {
        std::array<bool, SORT_ENTRY_COUNT> isSpecified{};
        uint32_t currentBitCount = 0;

        // These are in priority order so iterate from last to first (the lowest
        // priority bits are shifted the least)
        for (auto iter = rbegin(init); iter != rend(init); ++iter)
        {
            auto& e = *iter;
            auto i = int(e.entry);
            RIVE_CONSTEXPR_CHECK(!isSpecified[i], "SortEntry specified twice");
            isSpecified[i] = true;

            if (e.bitCount != 0)
            {
                m_elements[i].mask = ((1ull << e.bitCount) - 1)
                                     << currentBitCount;
                m_elements[i].shift = currentBitCount;
            }

            currentBitCount += e.bitCount;
            RIVE_CONSTEXPR_CHECK(e.bitCount <= 63,
                                 "More than 63 bits were specified");
            RIVE_CONSTEXPR_CHECK(currentBitCount <= 63,
                                 "More than 63 bits were specified");
        }

        for (auto b : isSpecified)
        {
            RIVE_CONSTEXPR_CHECK(b, "Missing SortEntry");
        }

        RIVE_CONSTEXPR_CHECK(currentBitCount > 0, "No bits specified");
    }

    // Build a key, but don't require every element to be specified.
    constexpr uint64_t buildPartialKey(
        const std::initializer_list<Value>& values) const
    {
        RIVE_CONSTEXPR_CHECK(values.size() > 0,
                             "No values specified to add to key");
        uint64_t key = 0;
        for (const auto& value : values)
        {
            const auto m = mask(value.entry);
            const auto s = shift(value.entry);
            RIVE_CONSTEXPR_CHECK((key & m) == 0,
                                 "Same value added multiple times");
            key |= (value.value << s) & m;

#if RIVE_ENABLE_SORT_KEY_VALIDATION
            if (value.validate == ValidateKeyEntry::yes && m != 0)
            {
                auto extracted = (key & m) >> s;
                RIVE_CONSTEXPR_CHECK(extracted == value.value,
                                     "Not enough bits to store given value");
            }
#endif
        }

        return key;
    }

    // Build a key, requiring every member of SortEntry to be specified once.
    constexpr uint64_t buildKey(
        const std::initializer_list<Value>& values) const
    {
#if RIVE_ENABLE_SORT_KEY_VALIDATION
        std::array<bool, SORT_ENTRY_COUNT> isSpecified{};
        for (auto& v : values)
        {
            RIVE_CONSTEXPR_CHECK(!isSpecified[int(v.entry)],
                                 "SortEntry specified multiple times");
            isSpecified[int(v.entry)] = true;
        }

        for (auto b : isSpecified)
        {
            RIVE_CONSTEXPR_CHECK(b, "Missing SortEntry while building key");
        }
#endif
        return buildPartialKey(values);
    }

    // Pull the given value out of a key
    template <typename T = uint64_t>
    T extract(SortEntry entry, uint64_t key) const
    {
        if constexpr (std::is_enum_v<T>)
        {
            return T(extract<std::underlying_type_t<T>>(entry, key));
        }
        else
        {
            return math::lossless_numeric_cast<T>((key & mask(entry)) >>
                                                  shift(entry));
        }
    }

    constexpr uint64_t mask(SortEntry e) const
    {
        return m_elements[int(e)].mask;
    }

    constexpr uint64_t shift(SortEntry e) const
    {
        return m_elements[int(e)].shift;
    }

private:
    struct Element
    {
        uint64_t mask;
        uint32_t shift;
    };

    std::array<Element, SORT_ENTRY_COUNT> m_elements{};
};

#undef RIVE_KEY_ENTRIES
#undef RIVE_CONSTEXPR_ASSERT
#undef RIVE_ENABLE_SORT_KEY_VALIDATION

} // namespace rive::gpu