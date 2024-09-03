/*
 * Copyright 2023 Rive
 */

#include <catch.hpp>

#include "rive/enum_bitset.hpp"

namespace rive
{
enum class Flags : uint8_t
{
    zero = 0,
    one = 1 << 0,
    two = 1 << 1,
    four = 1 << 2,
    eight = 1 << 3,
};
RIVE_MAKE_ENUM_BITSET(Flags);

// Check rive::EnumBitset<> operators
TEST_CASE("enum-operators", "[enum_bitset]")
{
    Flags flags;

    flags = Flags::one | Flags::two;
    CHECK(flags == (Flags)3);

    flags &= ~Flags::two;
    CHECK(flags == Flags::one);

    flags = Flags::two | (Flags::four | Flags::eight);
    CHECK(flags == (Flags)14);

    flags = (Flags::two | Flags::four) & ~(Flags::one | Flags::two);
    CHECK(flags == Flags::four);

    flags = (Flags::two | Flags::four) & (Flags::one | Flags::two);
    CHECK(flags == Flags::two);
    CHECK(flags & Flags::two);
    CHECK(!(flags & Flags::one));
    CHECK(flags & (Flags::two | Flags::eight));
    CHECK(!(flags & (Flags::four | Flags::eight)));

    // All & overloads.
    CHECK(!(Flags::one & Flags::two));
    CHECK(Flags::four & Flags::four);
    CHECK((Flags::four & Flags::four) == Flags::four);
    CHECK((Flags::one & (Flags::one | Flags::two)) == Flags::one);
    CHECK(!(~Flags::one & Flags::one));

    // All | overloads.
    CHECK(!(Flags::zero | Flags::zero));
    CHECK(Flags::zero | Flags::one);
    CHECK((Flags::one | Flags::two) == (Flags)3);
    CHECK((Flags::one | (Flags::two | Flags::four)) == (Flags)7);
    CHECK(((Flags::one | Flags::two) | Flags::four) == (Flags)7);
    CHECK(((Flags::one | Flags::two) | (Flags::four | Flags::eight)) == (Flags)15);

    // All ~ overloads.
    CHECK(~Flags::two == (Flags)(255 ^ 2));                      // Flags is a uint8_t
    CHECK(~(Flags::two | Flags::eight) == (Flags)(255 ^ 2 ^ 8)); // Flags is a uint8_t

    // All &= overloads.
    flags = Flags::eight | Flags::four | Flags::two | Flags::one;
    CHECK(flags == (Flags)15);
    Flags inverseEight = ~Flags::eight;
    flags &= inverseEight;
    CHECK(flags == (Flags)7);
    flags &= ~(Flags::four | Flags::one);
    CHECK(flags == Flags::two);
    flags &= Flags::two;
    CHECK(flags == Flags::two);
    flags &= Flags::one;
    CHECK(flags == Flags::zero);

    // All |= overloads.
    flags = Flags::zero;
    CHECK(flags == Flags::zero);
    flags |= Flags::eight;
    CHECK(flags == Flags::eight);
    flags |= ~Flags::eight;
    CHECK(flags == (Flags)255); // Flags is a uint8_t
}
} // namespace rive
