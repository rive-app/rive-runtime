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

TEST_CASE("gaussian_integral_table", "[gpu]")
{
    float gaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; i += 4)
    {
        float4 f32s = gpu::cast_f16_to_f32(
            simd::load<uint16_t, 4>(gpu::g_gaussianIntegralTableF16 + i));
        simd::store(gaussianTable + i, f32s);
    };

    CHECK(gaussianTable[0] == 0.0f);
    CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] == 1.0f);
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

// Looks up the value of "x" in the given function table, with linear filtering.
static float function_table_lookup(float x, const float* table, float tableSize)
{
    x = fminf(fmaxf(0, x), 1);
    float sampleRegionLeft = x * tableSize - .5f;
    int rightIdx = static_cast<int>(fminf(sampleRegionLeft + 1, tableSize - 1));
    int leftIdx = std::max(rightIdx - 1, 0);
    float t = fminf(fmaxf(0, sampleRegionLeft - leftIdx), 1);
    return lerp(table[leftIdx], table[rightIdx], t);
}

TEST_CASE("inverse_gaussian_integral_table", "[gpu]")
{
    float gaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; i += 4)
    {
        float4 f32s = gpu::cast_f16_to_f32(
            simd::load<uint16_t, 4>(gpu::g_gaussianIntegralTableF16 + i));
        simd::store(gaussianTable + i, f32s);
    };
    auto gaussianIntegral = [&gaussianTable](float x) {
        return function_table_lookup(x,
                                     gaussianTable,
                                     std::size(gaussianTable));
    };

    float inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; i += 4)
    {
        float4 f32s = gpu::cast_f16_to_f32(simd::load<uint16_t, 4>(
            gpu::g_inverseGaussianIntegralTableF16 + i));
        simd::store(inverseGaussianTable + i, f32s);
    };
    auto inverseGaussianIntegral = [&inverseGaussianTable](float y) {
        return function_table_lookup(y,
                                     inverseGaussianTable,
                                     std::size(inverseGaussianTable));
    };

    CHECK(inverseGaussianTable[0] == 0);
    CHECK(inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] == 1);
    if (gpu::GAUSSIAN_TABLE_SIZE & 1)
    {
        CHECK(inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2] == .5f);
    }
    else
    {
        CHECK((inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] +
               inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2]) /
                  2 ==
              Approx(.5f).margin(1e-4f));
    }
    for (int i = 1; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        CHECK(inverseGaussianTable[i - 1] <= inverseGaussianTable[i]);
    }
    for (int i = 0; i < (gpu::GAUSSIAN_TABLE_SIZE + 1) / 2 - 4; ++i)
    {
        CHECK(inverseGaussianTable[i] +
                  inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1 - i] ==
              Approx(1).margin(i > 100 ? 5e-4f
                               : i > 4 ? 1e-3f
                                       : 1e-2f));
    }

    // Check that the inverse table is actually an inverse of the gaussian
    // integral.
    float M = 21;
    for (float x = 0; x <= 1; x += 1.f / (gpu::GAUSSIAN_TABLE_SIZE * M))
    {
        float y = gaussianIntegral(x);
        // The inverse table loses precision at the inner and outer cells.
        float margin = x > .125f && x < .875f ? 1.f / 512
                       : x > .04f && x < .96f ? 1.f / 256
                       : x > .02f && x < .98f ? 1.f / 128
                                              : 1.f / 90;
        CHECK(inverseGaussianIntegral(y) == Approx(x).margin(margin));
    }

    // Check inverse_gaussian_integral edge cases.
    CHECK(inverseGaussianIntegral(-1) == 0);
    CHECK(inverseGaussianIntegral(2) == 1);
    CHECK(inverseGaussianIntegral(-std::numeric_limits<float>::infinity()) ==
          0);
    CHECK(inverseGaussianIntegral(std::numeric_limits<float>::infinity()) == 1);
    CHECK(inverseGaussianIntegral(std::numeric_limits<float>::quiet_NaN()) ==
          0);
}
} // namespace rive
