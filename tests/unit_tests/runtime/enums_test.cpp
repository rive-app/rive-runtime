/*
 * Copyright 2023 Rive
 */

#include <catch.hpp>
#include <limits.h>

#include "rive/math/bitwise.hpp"
#include "rive/enums.hpp"

namespace rive
{
using namespace enums;

enum NonScopedEnum
{
};
enum class NonFlagEnumA
{
    a = 1,
    b = 2,
};

enum class NonFlagEnumB
{
    none = 1, // none must be 0
    a = 1,
    b = 2,
};

enum class NonFlagEnumC
{
    None = 1, // None must be 0
    a = 1,
    b = 2,
};

enum class NonFlagEnumD
{
    NONE = 1, // NONE must be 0
    a = 1,
    b = 2,
};

enum class FlagEnumA
{
    none = 0,
    a = 1,
    b = 2,
};

enum class FlagEnumB
{
    None = 0,
    a = 1,
    b = 2,
};

enum class FlagEnumC
{
    NONE = 0,
    a = 1,
    b = 2,
};

static_assert(!internal::is_scoped_enum<NonScopedEnum>);
static_assert(internal::is_scoped_enum<NonFlagEnumA>);
static_assert(!internal::is_flag_enum<NonFlagEnumA>);
static_assert(!internal::is_flag_enum<NonFlagEnumB>);
static_assert(!internal::is_flag_enum<NonFlagEnumC>);
static_assert(!internal::is_flag_enum<NonFlagEnumD>);
static_assert(internal::is_flag_enum<FlagEnumA>);
static_assert(internal::is_flag_enum<FlagEnumB>);
static_assert(internal::is_flag_enum<FlagEnumC>);

enum class Flags
{
    none = 0,
    one = 1 << 0,
    two = 1 << 1,
    four = 1 << 2,
    eight = 1 << 3,
};

enum class Flags64 : uint64_t
{
    none = 0,
    one = 1 << 0,
    two = 1 << 1,
    four = 1 << 2,
    eight = 1 << 3,
};

static_assert(std::is_same_v<decltype(underlying_value(Flags{})),
                             std::underlying_type_t<Flags>>);
static_assert(std::is_same_v<decltype(underlying_value(Flags64{})),
                             std::underlying_type_t<Flags64>>);

template <typename Enum, typename OpFunc> void TestUnaryEnumOp(OpFunc&& func)
{
    using U = std::underlying_type_t<Enum>;

    auto seed = 0xf934929u; // arbitrary, but consistent, seed
    std::mt19937_64 random{seed};

    auto doCheck = [&](U a) {
        if constexpr (std::is_same_v<Enum, decltype(func(Enum(a)))>)
        {
            CHECK(func(Enum(a)) == Enum(func(a)));
        }
        else
        {
            CHECK(func(Enum(a)) == func(a));
        }
    };

    // Do some basic checks first
    doCheck(U(0));
    doCheck(U(1));
    doCheck(U(2));
    doCheck(U(3));
    doCheck(U(-1));

    // Now do a pile of random checks
    constexpr auto TEST_COUNT = 1000u;
    for (auto testIndex = 0u; testIndex < TEST_COUNT; testIndex++)
    {
        doCheck(U(random()));
    }
}

template <typename Enum, typename OpFunc> void TestBinaryEnumOp(OpFunc&& func)
{
    using U = std::underlying_type_t<Enum>;
    auto seed = 0xf934929u; // arbitrary, but consistent, seed
    std::mt19937_64 random{seed};

    auto doCheck = [&](U a, U b) {
        if constexpr (std::is_same_v<Enum, decltype(func(Enum(a), Enum(b)))>)
        {
            CHECK(func(Enum(a), Enum(b)) == Enum(func(a, b)));
        }
        else
        {
            CHECK(func(Enum(a), Enum(b)) == func(a, b));
        }
    };

    // Do some basic checks first
    doCheck(U(0), U(0));
    doCheck(U(1), U(0));
    doCheck(U(2), U(0));
    doCheck(U(0), U(1));
    doCheck(U(0), U(2));
    doCheck(U(1), U(1));
    doCheck(U(2), U(1));
    doCheck(U(3), U(1));
    doCheck(U(0), U(-1));
    doCheck(U(1), U(-1));
    doCheck(U(2), U(-1));

    // Now do a pile of random checks
    constexpr auto TEST_COUNT = 1000u;
    for (auto testIndex = 0u; testIndex < TEST_COUNT; testIndex++)
    {
        doCheck(U(random()), U(random()));
    }
}

TEST_CASE("flag operator|", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) { return a | b; });
}

TEST_CASE("flag operator&", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) { return a & b; });
}

TEST_CASE("flag operator^", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) { return a ^ b; });
}

TEST_CASE("flag operator~", "[enums]")
{
    TestUnaryEnumOp<Flags>([](auto a) { return ~a; });
}

TEST_CASE("flag operator |=", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) {
        auto r = a;
        r |= b;
        return r;
    });
}

TEST_CASE("flag operator &=", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) {
        auto r = a;
        r &= b;
        return r;
    });
}

TEST_CASE("flag operator ^=", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) {
        auto r = a;
        r ^= b;
        return r;
    });
}

// Make integral versions of these functions to make testing easier

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
I incr(I v)
{
    return ++v;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
I decr(I v)
{
    return --v;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
I underlying_value(I v)
{
    return v;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
bool any_flag_set(I flags)
{
    return flags != 0;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
bool any_flag_set(I flags, I mask)
{
    return (flags & mask) != 0;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
bool all_flags_set(I flags, I mask)
{
    return (flags & mask) == mask;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
bool no_flags_set(I flags)
{
    return flags == 0;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
bool no_flags_set(I flags, I mask)
{
    return (flags & mask) == 0;
}

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
bool is_single_flag(I flags)
{
    return math::count_set_bits(flags) == 1;
}

TEST_CASE("is_single_flag", "[enums]")
{
    CHECK(!is_single_flag(Flags::none));
    CHECK(!is_single_flag(Flags64::none));
    for (auto i = 0u; i < 32; i++)
    {
        CHECK(is_single_flag(Flags(1u << i)));
    }

    for (auto i = 0u; i < 64; i++)
    {
        CHECK(is_single_flag(Flags64(uint64_t(1u) << i)));
    }

    TestUnaryEnumOp<Flags>([](auto a) { return is_single_flag(a); });
    TestUnaryEnumOp<Flags64>([](auto a) { return is_single_flag(a); });
}

TEST_CASE("is_flag_set", "[enums]")
{
    // Because the second parameter needs to be a single flag, we'll do this as
    // a bespoke test instead of using TestBinaryEnumOp
    auto doTest = [](auto unusedEnumValue) {
        // the parameter is unused, except to give us the enum type to test.
        std::ignore = unusedEnumValue;

        using Enum = decltype(unusedEnumValue);
        using U = std::underlying_type_t<Enum>;

        CHECK(!is_flag_set(Enum::none, Enum::one));
        CHECK(is_flag_set(Enum::one, Enum::one));
        CHECK(!is_flag_set(Enum::one, Enum::two));
        CHECK(is_flag_set(Enum::one | Enum::two, Enum::one));

        auto seed = 0xf934929u; // arbitrary, but consistent, seed
        std::mt19937_64 random{seed};

        constexpr auto FLAG_BIT_COUNT = sizeof(U) * CHAR_BIT;
        constexpr auto TEST_COUNT_PER_FLAG_BIT = 100u;
        for (auto bitIndex = 0u; bitIndex < FLAG_BIT_COUNT; bitIndex++)
        {
            auto iTest = U(1) << bitIndex;
            auto eTest = Enum(iTest);
            CHECK(!is_flag_set(Enum::none, eTest));

            for (auto testIndex = 0u; testIndex < TEST_COUNT_PER_FLAG_BIT;
                 testIndex++)
            {
                auto iValue = U(random());
                auto eValue = Enum(iValue);
                CHECK(is_flag_set(eValue, eTest) == ((iValue & iTest) != 0));
            }
        }
    };

    doTest(Flags{});
    doTest(Flags64{});
}

TEST_CASE("underlying_value", "[enums]")
{
    TestUnaryEnumOp<Flags>([](auto a) { return underlying_value(a); });
    TestUnaryEnumOp<Flags64>([](auto a) { return underlying_value(a); });
}

TEST_CASE("incr", "[enums]")
{
    TestUnaryEnumOp<Flags>([](auto a) { return incr(a); });
    TestUnaryEnumOp<Flags64>([](auto a) { return incr(a); });
}

TEST_CASE("decr", "[enums]")
{
    TestUnaryEnumOp<Flags>([](auto a) { return decr(a); });
    TestUnaryEnumOp<Flags64>([](auto a) { return decr(a); });
}

TEST_CASE("any_flag_set (unmasked)", "[enums]")
{
    TestUnaryEnumOp<Flags>([](auto a) { return any_flag_set(a); });
    TestUnaryEnumOp<Flags64>([](auto a) { return decr(a); });
}

TEST_CASE("any_flag_set (masked)", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) { return any_flag_set(a, b); });
    TestBinaryEnumOp<Flags64>(
        [](auto a, auto b) { return any_flag_set(a, b); });
}

TEST_CASE("all_flags_set", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) { return all_flags_set(a, b); });
    TestBinaryEnumOp<Flags64>(
        [](auto a, auto b) { return all_flags_set(a, b); });
}

TEST_CASE("no_flag_set (unmasked)", "[enums]")
{
    TestUnaryEnumOp<Flags>([](auto a) { return no_flags_set(a); });
    TestUnaryEnumOp<Flags64>([](auto a) { return no_flags_set(a); });
}

TEST_CASE("no_flags_set (masked)", "[enums]")
{
    TestBinaryEnumOp<Flags>([](auto a, auto b) { return no_flags_set(a, b); });
    TestBinaryEnumOp<Flags64>(
        [](auto a, auto b) { return no_flags_set(a, b); });
}

} // namespace rive
