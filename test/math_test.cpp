#include <catch.hpp>
#include <rive/math/math_types.hpp>

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
    CHECK(math::ieee_float_divide(std::numeric_limits<float>::max(), kInf) == 0);
    CHECK(math::ieee_float_divide(std::numeric_limits<float>::max(), -kInf) == 0);
    CHECK(math::ieee_float_divide(-std::numeric_limits<float>::max(), -kInf) == 0);
    CHECK(math::ieee_float_divide(-std::numeric_limits<float>::max(), kInf) == 0);
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
