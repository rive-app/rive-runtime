/*
 * Copyright 2025 Rive
 */

// glsl cross-compiling requires the clang/gcc vector extension.
#ifndef _MSC_VER

#include "common/rand.hpp"
#include <catch.hpp>

namespace glsl_cross
{
#include "cpp.glsl"
#include "generated/shaders/constants.minified.glsl"
#if 0
// "common.glsl" is currently too complicated to compile for C++. If we really
// need it we can make it work, but for now it works to just declare our own
// version of unmultiply_rgb.
#include "generated/shaders/common.minified.glsl"
#else
static half3 unmultiply_rgb(half4 premul)
{
    // We *could* return preciesly 1 when premul.rgb == premul.a, but we can
    // also be approximate here. The blend modes that depend on this exact level
    // of precision (colordodge and colorburn) account for it with dstPremul.
    return premul.rgb * (premul.a != .0 ? 1. / premul.a : .0);
}
#endif
#define FRAGMENT
#define ENABLE_ADVANCED_BLEND true
#define ENABLE_HSL_BLEND_MODES true
#include "generated/shaders/advanced_blend.minified.glsl"

constexpr static float INF = std::numeric_limits<float>::infinity();

TEST_CASE("glsl_mix", "[advanced_blend]")
{
    CHECK(simd::all(mix(make_half3(0, 0, 0),
                        make_half3(1, 1, 1),
                        equal(make_half3(1, 2, 3), make_half3(2))) ==
                    make_half3(0, 1, 0)));
}

TEST_CASE("glsl_sign", "[advanced_blend]")
{
    CHECK(simd::all(sign(make_half4(-1, 0, .001f, 1)) ==
                    make_half4(-1, 0, 1, 1)));
}

TEST_CASE("glsl_unmultiply_rgb", "[advanced_blend]")
{
    // 0 if a == 0
    CHECK(simd::all(unmultiply_rgb(make_half4(0, 0, 0, 0)) == make_half3(0)));
    CHECK(simd::all(unmultiply_rgb(make_half4(.25f, .5f, .75f, 0)) ==
                    make_half3(0)));
    CHECK(simd::all(unmultiply_rgb(make_half4(-1, 0, 1, 0)) == make_half3(0)));
    CHECK(simd::all(unmultiply_rgb(make_half4(-1.0001f, 1, 1.0001f, 0)) ==
                    make_half3(0)));

    // rgb / a otherwise
    CHECK(simd::all(unmultiply_rgb(make_half4(.1f, .2f, .4f, .5f)) ==
                    make_half3(.2f, .4f, .8f)));
}

static half3 advanced_blend_coeffs_with_dst_alpha(half3 src,
                                                  half3 dst,
                                                  float dstAlpha,
                                                  uint16_t mode)
{
    return advanced_blend_coeffs(src,
                                 make_half4(dst.x * dstAlpha,
                                            dst.y * dstAlpha,
                                            dst.z * dstAlpha,
                                            dstAlpha),
                                 mode);
}

TEST_CASE("glsl_colordodge", "[advanced_blend]")
{
    // If dstAlpha == 0, always 0, even if dst is invalid premul data.
    CHECK(simd::all(advanced_blend_coeffs(make_half3(0),
                                          make_half4(0, 0, 0, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(1, 1.001f, INF),
                                          make_half4(0, 0, 0, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(-.0001f, -1, -INF),
                                          make_half4(0, 0, 0, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(0),
                                          make_half4(.0001f, 1, INF, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(1, 1.001f, INF),
                                          make_half4(.0001f, 1, INF, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(-.0001f, -1, -INF),
                                          make_half4(.0001f, 1, INF, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(0),
                                          make_half4(-.0001f, -1, -INF, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(1, 1.001f, INF),
                                          make_half4(-.0001f, -1, -INF, 0),
                                          BLEND_MODE_COLORDODGE) == 0));
    CHECK(simd::all(advanced_blend_coeffs(make_half3(-.0001f, -1, -INF),
                                          make_half4(-.0001f, -1, -INF, 0),
                                          BLEND_MODE_COLORDODGE) == 0));

    for (float dstAlpha :
         {0.f, 1.f / 255, .25f, .5f, 254.f / 255, 1.f, 256.f / 255})
    {
        // 0, if Cd <= 0
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0),
                                                 make_half3(0, -.001f, -1),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORDODGE) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0),
                                                 make_half3(-10, -100, -INF),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORDODGE) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(1),
                                                 make_half3(0, -.001f, -1),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORDODGE) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(1),
                                                 make_half3(-10, -100, -INF),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORDODGE) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-INF, INF, NAN),
                                                 make_half3(0, -.001f, -1),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORDODGE) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-INF, INF, NAN),
                                                 make_half3(-10, -100, -INF),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORDODGE) == 0));

        // 1, if Cd > 0 and Cs >= 1
        if (dstAlpha != 0)
        {
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(1, 1.00001f, 2),
                                make_half3(1e-10f),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(10, 100, INF),
                                make_half3(1e-10f),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(1, 1.00001f, 2),
                                make_half3(1),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(10, 100, INF),
                                make_half3(1),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(1, 1.00001f, 2),
                                make_half3(1.0001f),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(10, 100, INF),
                                make_half3(1.0001f),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(1, 1.00001f, 2),
                                make_half3(INF),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(10, 100, INF),
                                make_half3(INF),
                                dstAlpha,
                                BLEND_MODE_COLORDODGE) == 1));
        }

        // min(1,Cd/(1-Cs)), if Cd > 0 and Cs < 1
        Rand rando;
        for (int j = 0; j < 100; ++j)
        {
            half3 src = make_half3(rando.f32(), rando.f32(), rando.f32());
            assert(simd::all(0 <= src && src <= 1));
            half3 dst = dstAlpha == 0
                            ? make_half3(0)
                            : make_half3(rando.f32(), rando.f32(), rando.f32());
            assert(simd::all(0 <= dst && dst <= 1));
            half3 coeffs =
                advanced_blend_coeffs_with_dst_alpha(src,
                                                     dst,
                                                     dstAlpha,
                                                     BLEND_MODE_COLORDODGE);
            for (int i = 0; i < 3; ++i)
            {
                if (dst[i] <= 0)
                    CHECK(coeffs[i] == 0);
                else if (src[i] >= 1)
                    CHECK(coeffs[i] == 1);
                else
                    CHECK(coeffs[i] ==
                          Approx(std::min(1.f, dst[i] / (1 - src[i]))));
            }
        }
    }
}

TEST_CASE("glsl_colorburn", "[advanced_blend]")
{
    for (float dstAlpha :
         {.0f, 1.f / 255, .25f, .5f, 254.f / 255, 1.f, 256.f / 255})
    {
        // 1, if Cd >= 1
        if (dstAlpha != 0)
        {
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(0),
                                make_half3(1, 1.001f, 2),
                                dstAlpha,
                                BLEND_MODE_COLORBURN) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(0),
                                make_half3(10, 100, INF),
                                dstAlpha,
                                BLEND_MODE_COLORBURN) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(1),
                                make_half3(1, 1.001f, 2),
                                dstAlpha,
                                BLEND_MODE_COLORBURN) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(1),
                                make_half3(10, 100, INF),
                                dstAlpha,
                                BLEND_MODE_COLORBURN) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(-INF, INF, NAN),
                                make_half3(1, 1.001f, 2),
                                dstAlpha,
                                BLEND_MODE_COLORBURN) == 1));
            CHECK(simd::all(advanced_blend_coeffs_with_dst_alpha(
                                make_half3(-INF, INF, NAN),
                                make_half3(10, 100, INF),
                                dstAlpha,
                                BLEND_MODE_COLORBURN) == 1));
        }

        // 0, if Cd < 1 and Cs <= 0
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1, -1),
                                                 make_half3(1 - 1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(1 - 1e-6),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1, -1),
                                                 make_half3(0),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(0),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1, -1),
                                                 make_half3(-1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(-1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1, -1),
                                                 make_half3(-INF),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(-INF),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));

        // 1 - min(1,(1-Cd)/Cs), if Cd < 1 and Cs > 0
        Rand rando;
        for (int j = 0; j < 100; ++j)
        {
            half3 src = make_half3(rando.f32(), rando.f32(), rando.f32());
            assert(simd::all(0 <= src && src <= 1));
            half3 dst = dstAlpha == 0
                            ? make_half3(0)
                            : make_half3(rando.f32(), rando.f32(), rando.f32());
            assert(simd::all(0 <= dst && dst <= 1));
            half3 coeffs =
                advanced_blend_coeffs_with_dst_alpha(src,
                                                     dst,
                                                     dstAlpha,
                                                     BLEND_MODE_COLORBURN);
            for (int i = 0; i < 3; ++i)
            {
                if (dst[i] >= 1 && dstAlpha != 0)
                    CHECK(coeffs[i] == 1);
                else if (src[i] <= 0)
                    CHECK(coeffs[i] == 0);
                else
                    CHECK(coeffs[i] ==
                          Approx(1.f - std::min(1.f, (1.f - dst[i]) / src[i])));
            }
        }
    }
}
} // namespace glsl_cross

#endif // _MSC_VER
