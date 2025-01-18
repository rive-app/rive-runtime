/*
 * Copyright 2024 Rive
 */

#include "rive/math/math_types.hpp"
#include "rive/renderer/gpu.hpp"
#include "shaders/constants.glsl"
#include <catch.hpp>

namespace rive
{
TEST_CASE("find_transformed_area", "[gpu]")
{
    AABB unitSquare{0, 0, 1, 1};
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D()) == 1);
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D::fromScale(2, 2)) == 4);
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D::fromScale(2, 1)) == 2);
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D::fromScale(0, 1)) == 0);
    CHECK(gpu::find_transformed_area(unitSquare,
                                     Mat2D::fromRotation(math::PI / 4)) ==
          Approx(1.f));
    CHECK(gpu::find_transformed_area(
              unitSquare,
              Mat2D::fromRotation(math::PI / 8).scale({2, 2})) == Approx(4.f));
    CHECK(gpu::find_transformed_area(
              unitSquare,
              Mat2D::fromRotation(math::PI / 16).scale({2, 1})) == Approx(2.f));
    CHECK(
        gpu::find_transformed_area(unitSquare, {1, .87f, 8, 8 * .87f, 0, 0}) ==
        Approx(0.f).margin(math::EPSILON));
}

// Borrowed from:
// https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
float half_to_float(uint16_t x)
{
    // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15,
    // +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint32_t e = (x & 0x7C00) >> 10; // exponent
    const uint32_t m = (x & 0x03FF) << 13; // mantissa
    // evil log2 bit hack to count leading zeros in denormalized format
    const uint32_t v = math::bit_cast<uint32_t>(static_cast<float>(m)) >> 23;
    return math::bit_cast<float>(
        (x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) |
        ((e == 0) & (m != 0)) *
            ((v - 37) << 23 |
             ((m << (150 - v)) &
              0x007FE000))); // sign : normalized : denormalized
}

TEST_CASE("gaussian_integral_table", "[gpu]")
{
    float gaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        gaussianTable[i] = half_to_float(gpu::g_gaussianIntegralTableF16[i]);
    }

    CHECK(gaussianTable[0] >= 0);
    CHECK(gaussianTable[0] <= expf(-.5f * FEATHER_TEXTURE_STDDEVS));
    CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] <= 1);
    CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] >=
          1 - expf(-.5f * FEATHER_TEXTURE_STDDEVS));
    if (gpu::GAUSSIAN_TABLE_SIZE & 1)
    {
        CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2] == .5f);
    }
    else
    {
        CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] <= .5f);
        CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2] >= .5f);
        CHECK((gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] +
               gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2]) /
                  2 ==
              Approx(.5f).margin(1e-3f));
    }
    for (int i = 1; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        CHECK(gaussianTable[i - 1] <= gaussianTable[i]);
    }
    for (int i = 0; i < (gpu::GAUSSIAN_TABLE_SIZE + 1) / 2; ++i)
    {
        CHECK(gaussianTable[i] +
                  gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1 - i] ==
              Approx(1).margin(1e-3f));
    }
}

TEST_CASE("inverse_gaussian_integral_table", "[gpu]")
{
    CHECK(gpu::g_inverseGaussianIntegralTableF32[0] == 0);
    CHECK(
        gpu::g_inverseGaussianIntegralTableF32[gpu::GAUSSIAN_TABLE_SIZE - 1] ==
        1);
    if (gpu::GAUSSIAN_TABLE_SIZE & 1)
    {
        CHECK(gpu::g_inverseGaussianIntegralTableF32[gpu::GAUSSIAN_TABLE_SIZE /
                                                     2] == .5f);
    }
    else
    {
        CHECK((gpu::g_inverseGaussianIntegralTableF32
                   [gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] +
               gpu::g_inverseGaussianIntegralTableF32[gpu::GAUSSIAN_TABLE_SIZE /
                                                      2]) /
                  2 ==
              Approx(.5f).margin(1e-4f));
    }
    for (int i = 1; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        CHECK(gpu::g_inverseGaussianIntegralTableF32[i - 1] <=
              gpu::g_inverseGaussianIntegralTableF32[i]);
    }
    for (int i = 0; i < (gpu::GAUSSIAN_TABLE_SIZE + 1) / 2 - 4; ++i)
    {
        CHECK(gpu::g_inverseGaussianIntegralTableF32[i] +
                  gpu::g_inverseGaussianIntegralTableF32
                      [gpu::GAUSSIAN_TABLE_SIZE - 1 - i] ==
              Approx(1).margin(i > 100 ? 1e-4f
                               : i > 4 ? 1e-3f
                                       : 1e-2f));
    }

    // Check that the inverse table is actually an inverse of the gaussian
    // integral.
    float gaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        gaussianTable[i] = half_to_float(gpu::g_gaussianIntegralTableF16[i]);
    }
    float M = 21;
    for (float x = 0; x <= 1; x += 1.f / (gpu::GAUSSIAN_TABLE_SIZE * M))
    {
        float y = gpu::gaussian_table_lookup(gaussianTable, x);
        float inverseY = gpu::inverse_gaussian_integral(y);
        // The inverse table loses precision at the inner and outer cells.
        float margin = x > .125f && x < .875f ? 1.f / 512
                       : x > .04f && x < .96f ? 1.f / 256
                       : x > .02f && x < .98f ? 1.f / 128
                                              : 1.f / 95;
        CHECK(inverseY == Approx(x).margin(margin));
    }

    // Check inverse_gaussian_integral edge cases.
    CHECK(gpu::inverse_gaussian_integral(-1) == 0);
    CHECK(gpu::inverse_gaussian_integral(2) == 1);
    CHECK(gpu::inverse_gaussian_integral(
              -std::numeric_limits<float>::infinity()) == 0);
    CHECK(gpu::inverse_gaussian_integral(
              std::numeric_limits<float>::infinity()) == 1);
    CHECK(gpu::inverse_gaussian_integral(
              std::numeric_limits<float>::quiet_NaN()) == 0);
}
} // namespace rive
