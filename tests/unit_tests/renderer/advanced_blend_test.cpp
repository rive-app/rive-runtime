/*
 * Copyright 2025 Rive
 */

#include "common/rand.hpp"
#include <catch.hpp>

// <array> is included in "cpp.glsl" so include it here first so that it
// doesn't try to put all of its functions into the glsl_cross namespace
#include <array>

namespace glsl_cross
{
#ifdef _MSC_VER
#pragma warning(push)
// The shader code writes float constants without the "f" suffix, so disable
// this warning since there ends up being a lot of it:
//  truncation from 'double' to 'glsl_cross::half'
#pragma warning(disable : 4305)
#endif

#include "cpp.glsl"
#include "generated/shaders/constants.minified.glsl"
#if 0
// "common.glsl" is currently too complicated to compile for C++. If we really
// need it we can make it work, but for now it works to just declare our own
// version of a couple required functions
#include "generated/shaders/common.minified.glsl"
#else
static half3 unmultiply_rgb(half4 premul)
{
    // We *could* return preciesly 1 when premul.rgb == premul.a, but we can
    // also be approximate here. The blend modes that depend on this exact level
    // of precision (colordodge and colorburn) account for it with dstPremul.
    return premul.rgb * (premul.a != .0 ? 1. / premul.a : .0);
}

static half min_component(half3 v) { return min(v.x, min(v.y, v.z)); }
static half max_component(half3 v) { return max(v.x, max(v.y, v.z)); }
#endif
#define FRAGMENT
#define ENABLE_ADVANCED_BLEND true
#define ENABLE_HSL_BLEND_MODES true

#include "generated/shaders/advanced_blend.minified.glsl"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

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
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1f, -1),
                                                 make_half3(1 - 1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(1 - 1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1f, -1),
                                                 make_half3(0),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(0),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1f, -1),
                                                 make_half3(-1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(-10, -100, -INF),
                                                 make_half3(-1e-6f),
                                                 dstAlpha,
                                                 BLEND_MODE_COLORBURN) == 0));
        CHECK(simd::all(
            advanced_blend_coeffs_with_dst_alpha(make_half3(0, -1e-1f, -1),
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

namespace blend_spec_functions
{
using vec3 = simd::gvec<float, 3>;

using simd::dot;

// Functions copied (nearly) unmodified (except for formatting) from
// https://registry.khronos.org/OpenGL/extensions/NV/NV_blend_equation_advanced.txt

float minv3(vec3 c) { return min(min(c.r, c.g), c.b); }
float maxv3(vec3 c) { return max(max(c.r, c.g), c.b); }
float lumv3(vec3 c) { return dot(c, vec3{0.30f, 0.59f, 0.11f}); }
float satv3(vec3 c) { return maxv3(c) - minv3(c); }

// If any color components are outside [0,1], adjust the color to
// get the components in range.
vec3 ClipColor(vec3 color)
{
    float lum = lumv3(color);
    float mincol = minv3(color);
    float maxcol = maxv3(color);

    if (mincol < 0.0)
    {
        color = lum + ((color - lum) * lum) / (lum - mincol);
    }
    if (maxcol > 1.0)
    {
        color = lum + ((color - lum) * (1 - lum)) / (maxcol - lum);
    }
    return color;
}

// Take the base RGB color <cbase> and override its luminosity
// with that of the RGB color <clum>.
vec3 SetLum(vec3 cbase, vec3 clum)
{
    float lbase = lumv3(cbase);
    float llum = lumv3(clum);
    float ldiff = llum - lbase;
    vec3 color = cbase + vec3(ldiff);
    return ClipColor(color);
}

// Take the base RGB color <cbase> and override its saturation with
// that of the RGB color <csat>.  The override the luminosity of the
// result with that of the RGB color <clum>.
vec3 SetLumSat(vec3 cbase, vec3 csat, vec3 clum)
{
    float minbase = minv3(cbase);
    float sbase = satv3(cbase);
    float ssat = satv3(csat);
    vec3 color;
    if (sbase > 0)
    {
        // Equivalent (modulo rounding errors) to setting the
        // smallest (R,G,B) component to 0, the largest to <ssat>,
        // and interpolating the "middle" component based on its
        // original value relative to the smallest/largest.
        color = (cbase - minbase) * ssat / sbase;
    }
    else
    {
        color = vec3(0.0);
    }
    return SetLum(color, clum);
}
} // namespace blend_spec_functions

// Helper to compare the outputs of 2 functions over a reasonable selection of
// color values.
template <typename Func1, typename Func2>
void test_color_pairs(Func1&& func1, Func2&& func2)
{
    constexpr auto COLOR_STEP_COUNT = 6;
    for (int riX = 0; riX < COLOR_STEP_COUNT; riX++)
    {
        for (int giX = 0; giX < COLOR_STEP_COUNT; giX++)
        {
            for (int biX = 0; biX < COLOR_STEP_COUNT; biX++)
            {
                for (int riY = 0; riY < COLOR_STEP_COUNT; riY++)
                {
                    for (int giY = 0; giY < COLOR_STEP_COUNT; giY++)
                    {
                        for (int biY = 0; biY < COLOR_STEP_COUNT; biY++)
                        {
                            simd::gvec<float, 3> x = {
                                float(riX) / float(COLOR_STEP_COUNT - 1),
                                float(giX) / float(COLOR_STEP_COUNT - 1),
                                float(biX) / float(COLOR_STEP_COUNT - 1),
                            };
                            simd::gvec<float, 3> y = {
                                float(riY) / float(COLOR_STEP_COUNT - 1),
                                float(giY) / float(COLOR_STEP_COUNT - 1),
                                float(biY) / float(COLOR_STEP_COUNT - 1),
                            };

                            // Snap the colors to 8-bit values
                            x = simd::floor(x * 255.0f) / 255.0f;
                            y = simd::floor(x * 255.0f) / 255.0f;

                            auto r1 = func1(x, y);
                            auto r2 = func2(x, y);

                            auto d = r1 - r2;
                            float maxDiff =
                                max(std::abs(blend_spec_functions::minv3(d)),
                                    blend_spec_functions::maxv3(d));

                            CHECK(maxDiff <= 1e-4);
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("glsl_color_blend", "[advanced_blend]")
{
    test_color_pairs(
        [](auto a, auto b) { return blend_spec_functions::SetLum(a, b); },
        [](auto a, auto b) {
            return glsl_cross::advanced_blend_coeffs_with_dst_alpha(
                a,
                b,
                1.0f,
                BLEND_MODE_COLOR);
        });
}

TEST_CASE("glsl_luminosity_blend", "[advanced_blend]")
{
    test_color_pairs(
        [](auto a, auto b) { return blend_spec_functions::SetLum(b, a); },
        [](auto a, auto b) {
            return glsl_cross::advanced_blend_coeffs_with_dst_alpha(
                a,
                b,
                1.0f,
                BLEND_MODE_LUMINOSITY);
        });
}

TEST_CASE("glsl_saturation_blend", "[advanced_blend]")
{
    test_color_pairs(
        [](auto a, auto b) { return blend_spec_functions::SetLumSat(b, a, b); },
        [](auto a, auto b) {
            return glsl_cross::advanced_blend_coeffs_with_dst_alpha(
                a,
                b,
                1.0f,
                BLEND_MODE_SATURATION);
        });
}

TEST_CASE("glsl_hue_blend", "[advanced_blend]")
{
    test_color_pairs(
        [](auto a, auto b) { return blend_spec_functions::SetLumSat(a, b, b); },
        [](auto a, auto b) {
            return glsl_cross::advanced_blend_coeffs_with_dst_alpha(
                a,
                b,
                1.0f,
                BLEND_MODE_HUE);
        });
}
} // namespace glsl_cross
