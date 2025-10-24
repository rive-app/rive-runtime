/*
 * Copyright 2025 Rive
 */

#ifdef @FRAGMENT

PLS_BLOCK_BEGIN
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
#endif
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageBuffer);
PLS_BLOCK_END

#if defined(@DRAW_INTERIOR_TRIANGLES) && defined(@BORROWED_COVERAGE_PASS)

// Interior triangles with borrowed coverage never write color. They're also
// always the first fragment of the path at their pixel, so just blindly write
// coverage and move on.
PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_windingWeight, half);
    VARYING_UNPACK(v_pathID, half);

    PLS_INTERLOCK_BEGIN;
    PLS_STOREUI(coverageBuffer,
                packHalf2x16(make_half2(v_windingWeight, v_pathID)));
    PLS_PRESERVE_UI(clipBuffer);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    PLS_PRESERVE_4F(colorBuffer);
#endif
    PLS_INTERLOCK_END;

    EMIT_PLS;
}

#else

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_FRAG_COLOR_MAIN(@drawFragmentMain)
#else
PLS_MAIN(@drawFragmentMain)
#endif
{
    VARYING_UNPACK(v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_windingWeight, half);
#else
    VARYING_INIT(v_coverages, COVERAGE_TYPE);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(v_clipIDs, half2);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(v_blendMode, half);
#endif

    // Calculate the paint color before entering the interlock.
    half4 paintColor = find_paint_color(v_paint, 1. FRAGMENT_CONTEXT_UNPACK);

    // Calculate fragment coverage before entering the interlock.
    half fragCoverage =
#ifdef @DRAW_INTERIOR_TRIANGLES
        v_windingWeight;
#else
        find_frag_coverage(v_coverages);
#endif

    half maxCoverage = 1.;
#ifdef @ENABLE_CLIP_RECT
    // Calculate the clip rect before entering the interlock.
    if (@ENABLE_CLIP_RECT)
    {
        half clipRectMin = min_value(cast_float4_to_half4(v_clipRect));
        maxCoverage = min(clipRectMin, maxCoverage);
    }
#endif

    PLS_INTERLOCK_BEGIN;

    half2 coverageData = unpackHalf2x16(PLS_LOADUI(coverageBuffer));
    half coverageBufferID = coverageData.g;
    half initialCoverage =
        coverageBufferID == v_pathID ? coverageData.r : make_half(.0);
    half finalCoverage =
#ifndef @DRAW_INTERIOR_TRIANGLES
        is_stroke(v_coverages) ? max(initialCoverage, fragCoverage) :
#endif
                               initialCoverage + fragCoverage;

#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING && v_clipIDs.x != .0)
    {
        half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
        half clipBufferID = clipData.g;
        half clip = clipBufferID == v_clipIDs.x ? clipData.r : make_half(.0);
        maxCoverage = min(clip, maxCoverage);
    }
#endif

    // Find the coverage delta (c0 -> c1) that this fragment will apply, where
    // c0 is the coverage with which "paintColor" is already blended into the
    // framebuffer, and c1 is the total coverage with which we *want* it to be
    // blended after this fragment.
    // The geometry is ordered such that if c1 > 0, c1 >= c0 as well.
    maxCoverage = max(maxCoverage, .0);
    half c0 = clamp(initialCoverage, .0, maxCoverage);
    half c1 = clamp(finalCoverage, .0, maxCoverage);

#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    half4 dstColorPremul = PLS_LOAD4F(colorBuffer);
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
        // Don't bother with advanced blend until coverage becomes > 0. This
        // way, cutout regions don't pay the cost of advanced blend.
        if (v_blendMode != cast_uint_to_half(BLEND_SRC_OVER) && c1 != .0)
        {
            if (c0 == .0)
            {
                // This is the first fragment of the path to apply the blend
                // mode, meaning, the current dstColor is the correct value we
                // need to pass to advanced_color_blend().
                // Calculate the color-blended paint color before coverage.
                // Coverage can be applied later as a simple src-over operation.
                paintColor.rgb =
                    advanced_color_blend(paintColor.rgb,
                                         dstColorPremul,
                                         cast_half_to_ushort(v_blendMode));
                // Normally we need to save the color-blended paint color for
                // any future fragments at this same pixel because once we blend
                // this fragment, the original dstColor will be destroyed.
                // However, there are 2 exceptions:
                //
                // * No need to save the color-blended paint color if we're a
                //   (clockwise) interior triangle, because those are always
                //   guaranteed to be the final fragment of the path at a given
                //   pixel.
                //
                // * No need to save the color-blended paint color once coverage
                //   is maxed out, out because once it's maxed, any future
                //   fragments will effectively be no-ops (since c1 - c0 == 0).
#ifndef @DRAW_INTERIOR_TRIANGLES
                if (c1 < maxCoverage)
                {
                    PLS_STORE4F(scratchColorBuffer, paintColor);
                }
#endif
            }
            else
            {
                // This is not the first fragment of the path to apply the blend
                // mode, meaning, the current dstColor is no longer the correct
                // value we need to pass to advanced_color_blend().
                // Instead, the first fragment saved its result of
                // advanced_color_blend() to the scratch buffer, which we can
                // pull back up and use to apply our fragment's coverage
                // contribution.
                paintColor = PLS_LOAD4F(scratchColorBuffer);
                PLS_PRESERVE_4F(scratchColorBuffer);
            }
        }
        // GENERATE_PREMULTIPLIED_PAINT_COLORS is false when
        // @ENABLE_ADVANCED_BLEND is defined because advanced blend needs
        // unmultiplied colors. Premultiply alpha now.
        paintColor.rgb *= paintColor.a;
    }
#endif // @ENABLE_ADVANCED_BLEND
#endif // @FIXED_FUNCTION_COLOR_OUTPUT

    // Emit a paint color whose post-src-over-blend result is algebraically
    // equivalent to applying the c0 -> c1 coverage delta.
    //
    // NOTE: "max(, 1e-9)" is just to avoid a divide by zero. When the
    // denominator would be 0, c0 == 1, which also means c1 == 1, and there is
    // no coverage to apply. Since c0 == c1 == 1, (c1 - c0) / 1e-9 == 0, which
    // is the result we want in this case.
    paintColor *= (c1 - c0) / max(1. - c0 * paintColor.a, 1e-9);
#ifndef @DRAW_INTERIOR_TRIANGLES
    // Update the coverage buffer with our final value if we aren't an interior
    // triangle, because another fragment from this same path might come along
    // at this pixel.
    // The only exception is if we're src-over and fully opaque, because at that
    // point next fragment will effectively be a no-op (since any color blended
    // with itself is a no-op).
    // We can't skip the write for advanced blends because they also use the ID
    // in the coverage buffer to detect the first fragment of the path for dst
    // reads.
    if (
#ifdef @ENABLE_ADVANCED_BLEND
        (@ENABLE_ADVANCED_BLEND &&
         v_blendMode != cast_uint_to_half(BLEND_SRC_OVER)) ||
#endif
        paintColor.a < 1.)
    {
        PLS_STOREUI(coverageBuffer,
                    packHalf2x16(make_half2(finalCoverage, v_pathID)));
    }
    else
#endif // !@DRAW_INTERIOR_TRIANGLES
    {
        PLS_PRESERVE_UI(coverageBuffer);
    }

#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    PLS_STORE4F(colorBuffer, dstColorPremul * (1. - paintColor.a) + paintColor);
#endif

    PLS_PRESERVE_UI(clipBuffer);
    PLS_INTERLOCK_END;

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = paintColor;
#endif
    EMIT_PLS;
}

#endif

#endif // @FRAGMENT
