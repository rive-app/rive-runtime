/*
 * Copyright 2023 Rive
 */

#ifdef @FRAGMENT

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
STORAGE_BUFFER_U32_ATOMIC(COVERAGE_BUFFER_IDX, CoverageBuffer, coverageBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

#ifdef @BORROWED_COVERAGE_PASS
INLINE void apply_borrowed_coverage(half borrowedCoverage, uint coverageIndex)
{
    // Try to apply borrowedCoverage, assuming the existing coverage value
    // is zero.
    uint borrowedCoverageFixed =
        uint(abs(borrowedCoverage) * CLOCKWISE_COVERAGE_PRECISION + .5);
    uint targetCoverageValue =
        uniforms.coverageBufferPrefix |
        (CLOCKWISE_FILL_ZERO_VALUE - borrowedCoverageFixed);
    uint coverageBeforeMax = STORAGE_BUFFER_ATOMIC_MAX(coverageBuffer,
                                                       coverageIndex,
                                                       targetCoverageValue);
    if (coverageBeforeMax >= uniforms.coverageBufferPrefix)
    {
        // Coverage was not zero. Undo the atomicMax and then subtract
        // borrowedCoverageFixed this time.
        uint undoAtomicMax =
            coverageBeforeMax - max(coverageBeforeMax, targetCoverageValue);
        STORAGE_BUFFER_ATOMIC_ADD(coverageBuffer,
                                  coverageIndex,
                                  undoAtomicMax - borrowedCoverageFixed);
    }
}
#endif

INLINE void apply_stroke_coverage(INOUT(float) paintAlpha,
                                  half fragCoverage,
                                  uint coverageIndex)
{
    if (min(paintAlpha, fragCoverage) >= 1.)
    {
        // Solid stroke pixels don't need to work out coverage at all. We can
        // just blast them out without ever touching the coverage buffer.
        return;
    }

    half X;
    uint fragCoverageFixed =
        uint(abs(fragCoverage) * CLOCKWISE_COVERAGE_PRECISION + .5);
    uint coverageBeforeMax = STORAGE_BUFFER_ATOMIC_MAX(
        coverageBuffer,
        coverageIndex,
        uniforms.coverageBufferPrefix | fragCoverageFixed);
    if (coverageBeforeMax < uniforms.coverageBufferPrefix)
    {
        // This is the first fragment of the stroke to touch this pixel. Just
        // multiply in our coverage and write it out.
        X = fragCoverage;
    }
    else
    {
        // This pixel has been touched previously by a fragment in the stroke.
        // Multiply in an incremental coverage value that mixes with what's
        // already in the framebuffer.
        half c1 =
            cast_uint_to_half(coverageBeforeMax & CLOCKWISE_COVERAGE_MASK) *
            CLOCKWISE_COVERAGE_INVERSE_PRECISION;
        half c2 = max(c1, fragCoverage);
        X = (c2 - c1) / (1. - c1 * paintAlpha);
    }

    paintAlpha *= X;
}

INLINE void apply_fill_coverage(INOUT(float) paintAlpha,
                                half fragCoverageRemaining,
                                uint coverageIndex)
{
    uint coverageInitialValue =
        STORAGE_BUFFER_LOAD(coverageBuffer, coverageIndex);
    if (min(paintAlpha, fragCoverageRemaining) >= 1. &&
        (coverageInitialValue < uniforms.coverageBufferPrefix ||
         coverageInitialValue >=
             (uniforms.coverageBufferPrefix | CLOCKWISE_FILL_ZERO_VALUE)))
    {
        // If we're solid, AND the current coverage at this pixel is >= 0, then
        // we can just write out or color without working out coverage any
        // further.
        return;
    }

    half X = .0; // Amount by which to multiply paintAlpha.
    uint fragCoverageRemainingFixed =
        uint(abs(fragCoverageRemaining) * CLOCKWISE_COVERAGE_PRECISION + .5);
    if (coverageInitialValue < uniforms.coverageBufferPrefix)
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
        if (coverageBeforeMax <= uniforms.coverageBufferPrefix)
        {
            // Success! We were the first fragment of the path at this pixel.
            X = fragCoverageRemaining; // Just multiply paintAlpha by coverage.
#ifdef @DRAW_INTERIOR_TRIANGLES
            X = min(X, 1.);
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
            uint c1Fixed = (coverageBeforeMax & CLOCKWISE_COVERAGE_MASK) -
                           CLOCKWISE_FILL_ZERO_VALUE;
            half c1 = cast_uint_to_half(c1Fixed) *
                      CLOCKWISE_COVERAGE_INVERSE_PRECISION;
            half c2 = fragCoverageRemaining;
#ifdef @DRAW_INTERIOR_TRIANGLES
            c2 = min(c2, 1.);
#endif
            // Apply the coverage increase from the atomicMax here. The next
            // step will apply the remaining increase.
            X = (c2 - c1) / (1. - c1 * paintAlpha);

            // We increased coverage by an amount of "fragCoverageRemaining" -
            // "coverageBeforeMax". However, we wanted to increase coverage by
            // "fragCoverageRemaining". So the remaining amount we still need to
            // increase by is "coverageBeforeMax".
            fragCoverageRemainingFixed = c1Fixed;
            fragCoverageRemaining = c1;
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
        half c1 =
            cast_int_to_half(int((coverageBeforeAdd & CLOCKWISE_COVERAGE_MASK) -
                                 CLOCKWISE_FILL_ZERO_VALUE)) *
            CLOCKWISE_COVERAGE_INVERSE_PRECISION;
        half c2 = c1 + fragCoverageRemaining;
        c1 = clamp(c1, .0, 1.);
        c2 = clamp(c2, .0, 1.);
        // Apply the coverage increase from c1 -> c2 that we just did, in
        // addition to any coverage that had been applied previously.
        half one_minus_c1a = 1. - c1 * paintAlpha;
        if (one_minus_c1a <= .0)
            discard;
        X += (1. - X * paintAlpha) * (c2 - c1) / one_minus_c1a;
    }

    paintAlpha *= X;
}

FRAG_DATA_MAIN(half4, @drawFragmentMain)
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

    half4 paintColor;
#if defined(@DRAW_INTERIOR_TRIANGLES) && defined(@BORROWED_COVERAGE_PASS)
    if (!@BORROWED_COVERAGE_PASS)
#endif
    {
        paintColor = find_paint_color(v_paint, 1. FRAGMENT_CONTEXT_UNPACK);
    }

    if (paintColor.a == .0)
    {
        discard;
    }

    half fragCoverage =
#ifdef @DRAW_INTERIOR_TRIANGLES
        v_windingWeight;
#else
        find_frag_coverage(v_coverages);
#endif

    // Swizzle the coverage buffer in a tiled format, starting with 32x32
    // row-major tiles.
    uint coverageIndex = v_coveragePlacement.x;
    uint coveragePitch = v_coveragePlacement.y;
    uint2 coverageCoord = uint2(floor(v_coverageCoord));
    coverageIndex += (coverageCoord.y >> 5) * (coveragePitch << 5) +
                     (coverageCoord.x >> 5) * (32 << 5);
    // Subdivide each main tile into 4x4 column-major tiles.
    coverageIndex += ((coverageCoord.x & 0x1f) >> 2) * (32 << 2) +
                     ((coverageCoord.y & 0x1f) >> 2) * (4 << 2);
    // Let the 4x4 tiles be row-major.
    coverageIndex += (coverageCoord.y & 0x3) * 4 + (coverageCoord.x & 0x3);

#ifdef @BORROWED_COVERAGE_PASS
    if (@BORROWED_COVERAGE_PASS)
    {
        apply_borrowed_coverage(-fragCoverage, coverageIndex);
        discard;
    }
#endif // BORROWED_COVERAGE_PASS

#ifndef @DRAW_INTERIOR_TRIANGLES
    if (is_stroke(v_coverages))
    {
        fragCoverage = clamp(fragCoverage, .0, 1.);
        apply_stroke_coverage(paintColor.a, fragCoverage, coverageIndex);
    }
    else // It's a fill.
#endif   // !DRAW_INTERIOR_TRIANGLES
    {
        apply_fill_coverage(paintColor.a, fragCoverage, coverageIndex);
    }
    paintColor.rgb *= paintColor.a;

    EMIT_FRAG_DATA(paintColor);
}

#endif // FRAGMENT
