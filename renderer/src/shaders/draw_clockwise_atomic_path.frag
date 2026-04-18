/*
 * Copyright 2023 Rive
 */

#ifdef @FRAGMENT

PLS_BLOCK_BEGIN
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
PLS_DECL4F(CLIP_PLANE_IDX, clipBuffer);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F_RGB10_A2_UAV(SCRATCH_COLOR_PLANE_IDX, blendColorBuffer);
#endif
PLS_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32_ATOMIC(COVERAGE_BUFFER_IDX, CoverageBuffer, coverageBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

INLINE void apply_stroke_coverage(INOUT(float) paintAlpha,
                                  half fragCoverage,
                                  uint coverageIndex,
                                  OUT(uint) preexistingCoverageValue,
                                  OUT(half) newCoverage)
{
#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    if (min(paintAlpha, fragCoverage) >= 1.)
    {
        // Solid stroke pixels don't need to work out coverage at all. We can
        // just blast them out without ever touching the coverage buffer, even
        // if another fragment from the path will get drawn on top. This is
        // because any fragment drawn on top will be the same color, and any
        // color blended onto a fully opaque version of itself is a no-op.
        return;
    }
#endif

    half X;
    uint fragCoverageFixed =
        clockwise_atomic_coverage_delta_to_fixed(abs(fragCoverage));
    preexistingCoverageValue = STORAGE_BUFFER_ATOMIC_MAX(
        coverageBuffer,
        coverageIndex,
        uniforms.coverageBufferPrefix | fragCoverageFixed);
    if (preexistingCoverageValue < uniforms.coverageBufferPrefix)
    {
        // This is the first fragment of the stroke to touch this pixel. Just
        // multiply in our coverage and write it out.
        X = fragCoverage;
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
        newCoverage = fragCoverage;
#endif
    }
    else
    {
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
        if ((preexistingCoverageValue & BLEND_COLOR_VALID_BIT) != 0u)
        {
            // The BLEND_COLOR_VALID_BIT had already been set at this fragment.
            // Redo the atomic max with that bit set.
            preexistingCoverageValue = STORAGE_BUFFER_ATOMIC_MAX(
                coverageBuffer,
                coverageIndex,
                uniforms.coverageBufferPrefix | BLEND_COLOR_VALID_BIT |
                    fragCoverageFixed);
        }
#endif
        // This pixel has been touched previously by a fragment in the stroke.
        // Multiply in an incremental coverage value that mixes with what's
        // already in the framebuffer.
        half c0 = cast_uint_to_half(preexistingCoverageValue &
                                    CLOCKWISE_COVERAGE_MASK) *
                  CLOCKWISE_COVERAGE_INVERSE_PRECISION;
        half c1 = max(c0, fragCoverage);
        X = incremental_clockwise_coverage(c0, c1, paintAlpha);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
        newCoverage = c1;
#endif
    }

    paintAlpha *= X;
}

INLINE void apply_fill_coverage(INOUT(float) paintAlpha,
                                half fragCoverageRemaining,
                                uint coverageIndex,
                                OUT(uint) preexistingCoverageValue,
                                OUT(half) newCoverage)
{
    half X = .0; // Amount by which to multiply paintAlpha.
    uint fragCoverageRemainingFixed =
        clockwise_atomic_coverage_delta_to_fixed(abs(fragCoverageRemaining));

    preexistingCoverageValue =
        STORAGE_BUFFER_LOAD(coverageBuffer, coverageIndex);

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    if (min(paintAlpha, fragCoverageRemaining) >= 1. &&
        (preexistingCoverageValue < uniforms.coverageBufferPrefix ||
         preexistingCoverageValue >=
             (uniforms.coverageBufferPrefix | CLOCKWISE_FILL_ZERO_VALUE)))
    {
        // If we're solid, AND the current coverage at this pixel is >= 0, then
        // we can just write out our color without working out coverage any
        // further, even if another fragment from the path will get drawn on
        // top. This is because any fragment drawn on top will be the same
        // color, and any color blended onto a fully opaque version of itself is
        // a no-op.
        return;
    }
#endif

    if (preexistingCoverageValue < uniforms.coverageBufferPrefix)
    {
        // The initial coverage value does not belong to this path. We *might*
        // be the first fragment of the path to touch this pixel. Attempt to
        // write out our coverage with an atomicMax.
        uint targetCoverage =
            uniforms.coverageBufferPrefix |
            (CLOCKWISE_FILL_ZERO_VALUE + fragCoverageRemainingFixed);
        uint coverageBeforeMax = STORAGE_BUFFER_ATOMIC_MAX(coverageBuffer,
                                                           coverageIndex,
                                                           targetCoverage);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
        preexistingCoverageValue = coverageBeforeMax;
#endif
        if (coverageBeforeMax <= uniforms.coverageBufferPrefix)
        {
            // Success! We were the first fragment of the path at this pixel.
            X = fragCoverageRemaining; // Just multiply paintAlpha by coverage.
#ifdef @DRAW_INTERIOR_TRIANGLES
            X = min(X, 1.);
#endif
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
            newCoverage = X;
#endif
            fragCoverageRemaining = .0; // We're done.
        }
        else if (coverageBeforeMax < targetCoverage)
        {
            // We were not first fragment of the path at this pixel, AND our
            // atomicMax had an effect that we now have to account for in
            // paintAlpha. Coverage increased from "coverageBeforeMax" to
            // "fragCoverageRemaining".
            //
            // NOTE: because we know coverage was initially zero, and because
            // coverage is always positive in this pass, we know
            // coverageBeforeMax >= 0.
            uint c0Fixed = (coverageBeforeMax & CLOCKWISE_COVERAGE_MASK) -
                           CLOCKWISE_FILL_ZERO_VALUE;
            half c0 = cast_uint_to_half(c0Fixed) *
                      CLOCKWISE_COVERAGE_INVERSE_PRECISION;
            half c1 = fragCoverageRemaining;
#ifdef @DRAW_INTERIOR_TRIANGLES
            c1 = min(c1, 1.);
#endif
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
            newCoverage = c1;
#endif
            // Apply the coverage increase from the atomicMax here. The next
            // step will apply the remaining increase.
            X = incremental_clockwise_coverage(c0, c1, paintAlpha);

            // We increased coverage by an amount of "fragCoverageRemaining" -
            // "coverageBeforeMax". However, we wanted to increase coverage by
            // "fragCoverageRemaining". So the remaining amount we still need to
            // increase by is "coverageBeforeMax".
            fragCoverageRemainingFixed = c0Fixed;
            fragCoverageRemaining = c0;
        }
    }

    if (fragCoverageRemaining > .0)
    {
        // At this point we know the value in the coverage buffer belongs to
        // this path, so we can do a simple atomicAdd.
        uint coverageBeforeAdd =
            STORAGE_BUFFER_ATOMIC_ADD(coverageBuffer,
                                      coverageIndex,
                                      fragCoverageRemainingFixed);
        half c0 = clockwise_atomic_fixed_to_coverage(coverageBeforeAdd);
        half c1 = c0 + fragCoverageRemaining;
        c0 = clamp(c0, .0, 1.);
        c1 = clamp(c1, .0, 1.);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
        newCoverage = c1;
#endif
        // Apply the coverage increase from c0 -> c1 that we just did, in
        // addition to any coverage that had been applied previously.
        X += (1. - X * paintAlpha) *
             incremental_clockwise_coverage(c0, c1, paintAlpha);
    }

    paintAlpha *= X;
}

CLOCKWISE_ATOMIC_PLS_MAIN(@drawFragmentMain)
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
    VARYING_UNPACK(v_coveragePlacement, uint2);
    VARYING_UNPACK(v_coverageCoord, float2);

    half4 paintColor = find_paint_color(v_paint, 1. FRAGMENT_CONTEXT_UNPACK);

#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    // Fetch the framebuffer BEFORE any atomic operations on the coverage
    // buffer. In order for advanced blend to work, we have to fetch the
    // framebuffer value before checking if it's still valid.
    half4 dstColor = PLS_LOAD4F(colorBuffer);
#endif

    half fragCoverage =
#ifdef @DRAW_INTERIOR_TRIANGLES
        v_windingWeight;
#else
        find_frag_coverage(v_coverages);
#endif

    float2 coverageCoord = v_coverageCoord;
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    // This little trick forces the shader to fetch the framebuffer BEFORE any
    // atomic operations on the coverage buffer. (i.e., not to reorder the above
    // fetch past this point). In order for advanced blend to work, we have to
    // fetch the framebuffer value before operating on coverage.
    //
    // NOTE: Since v_coverageCoord is pixel-grid aligned, it will always have a
    // fractional value of ~.5 (because varyings are sampled at at pixel
    // center). So as long as colorBuffer is a standard unorm in the range 0..1,
    // this will have literally no effect on the final outcome. If we ever
    // support rendering to full floating point targets outside the range 0..1,
    // we may need to put some more thought into this.
    coverageCoord +=
        (dstColor.rg + dstColor.ba) * uniforms.epsilonForPseudoMemoryBarrier;
#endif
    coverageCoord = floor(coverageCoord);
    uint coveragePitch = v_coveragePlacement.y;
    uint coverageIndex =
        v_coveragePlacement.x +
        swizzle_buffer_idx_32x32(uint2(coverageCoord), coveragePitch);

    half maxCoverage = 1.;

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        half clipRectMin = min_component(cast_float4_to_half4(v_clipRect));
        maxCoverage = min(clipRectMin, maxCoverage);
    }
#endif

#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING && v_clipIDs.x != .0)
    {
        half clip = PLS_LOAD4F(clipBuffer).r;
        maxCoverage = min(clip, maxCoverage);
    }
#endif

    maxCoverage = max(maxCoverage, .0);
    fragCoverage = clamp(fragCoverage, .0, maxCoverage);

    uint preexistingCoverageValue;
    float newCoverage;
#ifndef @DRAW_INTERIOR_TRIANGLES
    if (is_stroke(v_coverages))
    {
        apply_stroke_coverage(paintColor.a,
                              fragCoverage,
                              coverageIndex,
                              preexistingCoverageValue,
                              newCoverage);
    }
    else // It's a fill.
#endif   // !DRAW_INTERIOR_TRIANGLES
    {
        apply_fill_coverage(paintColor.a,
                            fragCoverage,
                            coverageIndex,
                            preexistingCoverageValue,
                            newCoverage);
    }

#ifdef @ENABLE_DITHER
    half dither;
    if (@ENABLE_DITHER)
    {
        dither = get_dither(_fragCoord.xy,
                            uniforms.ditherScale,
                            uniforms.ditherBias);
    }
#endif

#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    if (paintColor.a > .0)
    {
        bool wasBlendColorValid =
            preexistingCoverageValue >= uniforms.coverageBufferPrefix &&
            (preexistingCoverageValue & BLEND_COLOR_VALID_BIT) != 0u;
        if (!wasBlendColorValid)
        {
            // If the saved blend color was not yet valid after we fetched
            // dstColor, we are guaranteed that dstColor is valid because the
            // BLEND_COLOR_VALID_BIT gets set before any color outputs that
            // might overwrite the framebuffer.
            // Calculate a blendColor based on dstColor.
            paintColor.rgb =
                advanced_color_blend(paintColor.rgb,
                                     dstColor,
                                     cast_half_to_ushort(v_blendMode));

            // Anybody who updated, or will update, the coverage buffer before
            // we overwrite the framebuffer is guaranteed to have a dstColor
            // that is unaffected by our color output. They already have it.
            // But if 0 < coverage < 1 after our fragment, we have to save out
            // the blend color we just found for any future fragments that may
            // need to blend, before we overwrite the contents of the
            // framebuffer.
            if (newCoverage < 1.)
            {
                half3 blendRGBToSave = paintColor.rgb;
#ifdef @ENABLE_DITHER
                if (@ENABLE_DITHER)
                {
                    blendRGBToSave += dither * uniforms.ditherConversionToRGB10;
                }
#endif
                PLS_STORE4F_UAV(blendColorBuffer,
                                make_half4(blendRGBToSave, .0));

                // Mark this pixel as having a valid blendColor, AFTER writing
                // out the blendColor, but BEFORE updating the framebuffer.
                memoryBarrier();
                STORAGE_BUFFER_ATOMIC_OR(coverageBuffer,
                                         coverageIndex,
                                         BLEND_COLOR_VALID_BIT);
            }
        }
        else
        {
            // Use the saved blendColor whenever it's valid, because shortly
            // after that point the framebuffer can be overwritten, invalidating
            // the dstColor.
            paintColor.rgb = PLS_LOAD4F_UAV(blendColorBuffer).rgb;
        }
    }
#endif

    paintColor.rgb *= paintColor.a;

#ifdef @ENABLE_DITHER
    if (@ENABLE_DITHER)
    {
        paintColor.rgb += dither;
    }
#endif

    // Since blend is enabled, storing 0 to the clip will ensure it remains
    // unchanged.
    PLS_STORE4F(clipBuffer, make_half4(.0));
    EMIT_CLOCKWISE_ATOMIC_PLS(paintColor);
}

#endif // FRAGMENT
