#include <catch.hpp>
#include <rive/math/bitwise.hpp>
#include <rive/math/math_types.hpp>
#include <rive/enums.hpp>
#include <rive/span.hpp>

using namespace rive;

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Check math::ieee_float_divide.
TEST_CASE("ieee_float_divide", "[math]")
{
    // Make sure we're doing division.
    CHECK(math::ieee_float_divide(100, 10) == 10);

    // Returns +/-Inf if b == 0.
    CHECK(math::ieee_float_divide(5, 0) == kInf);
    CHECK(math::ieee_float_divide(5, -0) == kInf);
    CHECK(math::ieee_float_divide(-3, 0) == -kInf);
    CHECK(math::ieee_float_divide(-3, -0) == -kInf);
    CHECK(math::ieee_float_divide(kInf, 0) == kInf);
    CHECK(math::ieee_float_divide(-kInf, 0) == -kInf);
    CHECK(math::ieee_float_divide(kInf, -0) == kInf);
    CHECK(math::ieee_float_divide(-kInf, -0) == -kInf);

    // Returns 0 if b == +/-Inf.
    CHECK(math::ieee_float_divide(1, kInf) == 0);
    CHECK(math::ieee_float_divide(-100, kInf) == 0);
    CHECK(math::ieee_float_divide(std::numeric_limits<float>::max(), kInf) ==
          0);
    CHECK(math::ieee_float_divide(std::numeric_limits<float>::max(), -kInf) ==
          0);
    CHECK(math::ieee_float_divide(-std::numeric_limits<float>::max(), -kInf) ==
          0);
    CHECK(math::ieee_float_divide(-std::numeric_limits<float>::max(), kInf) ==
          0);
    CHECK(math::ieee_float_divide(0, kInf) == 0);
    CHECK(math::ieee_float_divide(0, -kInf) == 0);
    CHECK(math::ieee_float_divide(-0, -kInf) == 0);
    CHECK(math::ieee_float_divide(-0, kInf) == 0);

    // Returns NaN if a and b are both zero.
    CHECK(std::isnan(math::ieee_float_divide(0, 0)));
    CHECK(std::isnan(math::ieee_float_divide(0, -0)));
    CHECK(std::isnan(math::ieee_float_divide(-0, 0)));
    CHECK(std::isnan(math::ieee_float_divide(-0, -0)));

    // Returns NaN if b and are both infinite.
    CHECK(std::isnan(math::ieee_float_divide(kInf, kInf)));
    CHECK(std::isnan(math::ieee_float_divide(kInf, -kInf)));
    CHECK(std::isnan(math::ieee_float_divide(-kInf, kInf)));
    CHECK(std::isnan(math::ieee_float_divide(kInf, -kInf)));

    // Returns NaN a or b is NaN.
    CHECK(std::isnan(math::ieee_float_divide(kNaN, 1)));
    CHECK(std::isnan(math::ieee_float_divide(kInf, kNaN)));
}

// Check math::bit_cast.
TEST_CASE("bit_cast", "[math]")
{
    CHECK(math::bit_cast<float>(0x3f800000) == 1);
    CHECK(math::bit_cast<float>((1u << 31) | 0x3f800000) == -1);
    CHECK(math::bit_cast<float>(0x7f800000) == kInf);
    CHECK(math::bit_cast<float>((1u << 31) | 0x7f800000) == -kInf);
    CHECK(std::isnan(math::bit_cast<float>(0x7fc00000)));
}

// Check math::clz*
TEST_CASE("clz", "[math]")
{
    CHECK(math::clz32(1) == 31);
    CHECK(math::clz32(-1) == 0);
    for (int i = 0; i < 32; ++i)
    {
        CHECK(math::clz32(1 << i) == 31 - i);
        CHECK(math::clz32((1 << i) | (rand() & ((1 << i) - 1))) == 31 - i);
    }

    CHECK(math::clz64(1) == 63);
    CHECK(math::clz64(-1) == 0);
    for (int i = 0; i < 64; ++i)
    {
        CHECK(math::clz64(1ll << i) == 63 - i);
        CHECK(math::clz64((1ll << i) | (rand() & ((1ll << i) - 1))) == 63 - i);
    }
}

// Check math::rotateleft32
TEST_CASE("rotateleft32", "[math]")
{
    CHECK(math::rotateleft32(0xabcdef01, 24) == 0x01abcdef);
    CHECK(math::rotateleft32(0xffff0000, 16) == 0x0000ffff);
}

// Check math::msb.
TEST_CASE("msb", "[math]")
{
    CHECK(math::msb(0) == 0);
    CHECK(math::msb(1) == 1);
    CHECK(math::msb(2) == 2);
    CHECK(math::msb(3) == 2);
    CHECK(math::msb(4) == 3);
    CHECK(math::msb(5) == 3);
    CHECK(math::msb(6) == 3);
    CHECK(math::msb(7) == 3);
    CHECK(math::msb(8) == 4);
    CHECK(math::msb(9) == 4);
    for (int i = 0; i < 29; ++i)
    {
        CHECK(math::msb(10 << i) == 4 + i);
    }
    CHECK(math::msb(0xffffffff) == 32);
}

// Check math::round_up_to_multiple_of.
TEST_CASE("round_up_to_multiple_of", "[math]")
{
    CHECK(math::round_up_to_multiple_of<4>(0) == 0);
    CHECK(math::round_up_to_multiple_of<4>(3) == 4);
    CHECK(math::round_up_to_multiple_of<8>(16) == 16);
    CHECK(math::round_up_to_multiple_of<8>(24) == 24);
    CHECK(math::round_up_to_multiple_of<8>(25) == 32);
    CHECK(math::round_up_to_multiple_of<8>(31) == 32);
    CHECK(math::round_up_to_multiple_of<8>(32) == 32);
    for (size_t i = 0; i < 10; ++i)
    {
        CHECK(math::round_up_to_multiple_of<1>(i) == i);
    }
    CHECK(math::round_up_to_multiple_of<2>(~size_t(0)) == 0);
}

// Check math::positive_mod.
TEST_CASE("positive_mod", "[math]")
{
    CHECK(math::positive_mod(1, 1) == 0);
    CHECK(math::positive_mod(10, 7) == 3);
    CHECK(math::positive_mod(-4, 3) == 2);
    CHECK(math::positive_mod(-5.5f, 7.0f) == 1.5f);
    CHECK(math::positive_mod(-5.5f, -70.0f) == 64.5f);
    CHECK(math::positive_mod(45.5f, -12.0f) == 9.5f);
}

TEST_CASE("count_set_bits", "[math]")
{
    CHECK(math::count_set_bits(0u) == 0);
    CHECK(math::count_set_bits(1u) == 1);
    CHECK(math::count_set_bits(0x80000000u) == 1);
    CHECK(math::count_set_bits(0b10010101100110u) == 7);
    CHECK(math::count_set_bits(~0b10010101100110u) == 32 - 7);

    CHECK(math::count_set_bits(0ull) == 0);
    CHECK(math::count_set_bits(1ull) == 1);
    CHECK(math::count_set_bits(0x8000000000000000ull) == 1);
    CHECK(math::count_set_bits(0b10010101100110ull) == 7);
    CHECK(math::count_set_bits(~0b10010101100110ull) == 64 - 7);

    CHECK(math::internal::count_set_bits_fallback(0u) == 0);
    CHECK(math::internal::count_set_bits_fallback(1u) == 1);
    CHECK(math::internal::count_set_bits_fallback(0x80000000u) == 1);
    CHECK(math::internal::count_set_bits_fallback(0b10010101100110u) == 7);
    CHECK(math::internal::count_set_bits_fallback(~0b10010101100110u) ==
          32 - 7);

    CHECK(math::internal::count_set_bits_fallback(0ull) == 0);
    CHECK(math::internal::count_set_bits_fallback(1ull) == 1);
    CHECK(math::internal::count_set_bits_fallback(0x8000000000000000ull) == 1);
    CHECK(math::internal::count_set_bits_fallback(0b10010101100110ull) == 7);
    CHECK(math::internal::count_set_bits_fallback(~0b10010101100110ull) ==
          64 - 7);
}

TEST_CASE("compact_bitmask_value", "[math]")
{
    CHECK(math::compact_bitmask_value(0x00000000u, 0x00000000u) == 0);
    CHECK(math::compact_bitmask_value(0xffffffffu, 0x00000000u) == 0);
    CHECK(math::compact_bitmask_value(0x00000000u, 0x10001000u) == 0);
    CHECK(math::compact_bitmask_value(0x00001000u, 0x10001000u) == 1);
    CHECK(math::compact_bitmask_value(0x10000000u, 0x10001000u) == 2);
    CHECK(math::compact_bitmask_value(0x10001000u, 0x10001000u) == 3);
    CHECK(math::compact_bitmask_value(0x10101000u, 0x10001000u) == 3);
    CHECK(math::compact_bitmask_value(0xffffffffu, 0x10001000u) == 3);
    CHECK(math::compact_bitmask_value(0xffffffffu, 0x10001010u) == 7);
    CHECK(math::compact_bitmask_value(0x10000000u, 0x10001010u) == 4);
}

TEST_CASE("expand_compacted_bitmask_value", "[math]")
{
    CHECK(math::expand_compacted_bitmask_value(0x00000000u, 0x00000000) == 0);
    CHECK(math::expand_compacted_bitmask_value(0xffffffffu, 0x00000000) == 0);
    CHECK(math::expand_compacted_bitmask_value(0x00000000u, 0x10001000) == 0);
    CHECK(math::expand_compacted_bitmask_value(1, 0x10001000) == 0x00001000);
    CHECK(math::expand_compacted_bitmask_value(2, 0x10001000) == 0x10000000);
    CHECK(math::expand_compacted_bitmask_value(3, 0x10001000) == 0x10001000);
    CHECK(math::expand_compacted_bitmask_value(0xffffffff, 0x10001000) ==
          0x10001000);
    CHECK(math::expand_compacted_bitmask_value(7, 0x10001010) == 0x10001010);
    CHECK(math::expand_compacted_bitmask_value(4, 0x10001010) == 0x10000000);
}

template <typename T>
static void iterate_bit_combinations_case(
    T mask,
    const std::initializer_list<T>& expectedValues)
{
    auto it = std::begin(expectedValues);
    for (auto combo : math::iterate_bit_combinations_in_mask(mask))
    {
        CHECK(combo == *it);
        ++it;
    }

    CHECK(it == std::end(expectedValues));
}

enum class TestEnum
{
    none = 0x00,
    a = 0x01,
    b = 0x02,
    c = 0x10,
};

TEST_CASE("iterate_bit_combinations_in_mask", "[math]")
{
    // There is a single possible value when there are no bits in the mask.
    iterate_bit_combinations_case(0x00000000u, {0x00000000u});

    // Iteration is actually from most flags -> no flags
    iterate_bit_combinations_case(0x00000001u, {0x00000001u, 0x00000000u});
    iterate_bit_combinations_case(0x00001001u,
                                  {
                                      0x00001001u,
                                      0x00001000u,
                                      0x00000001u,
                                      0x00000000u,
                                  });

    iterate_bit_combinations_case(TestEnum(TestEnum::a | TestEnum::c),
                                  {
                                      TestEnum(TestEnum::a | TestEnum::c),
                                      TestEnum::c,
                                      TestEnum::a,
                                      TestEnum::none,
                                  });

    iterate_bit_combinations_case(
        TestEnum(TestEnum::a | TestEnum::b | TestEnum::c),
        {
            TestEnum(TestEnum::a | TestEnum::b | TestEnum::c),
            TestEnum(TestEnum::b | TestEnum::c),
            TestEnum(TestEnum::a | TestEnum::c),
            TestEnum::c,
            TestEnum(TestEnum::a | TestEnum::b),
            TestEnum::b,
            TestEnum::a,
            TestEnum::none,
        });
}