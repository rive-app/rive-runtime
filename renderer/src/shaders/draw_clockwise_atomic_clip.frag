/*
 * Copyright 2026 Rive
 */

#ifdef @FRAGMENT

PLS_BLOCK_BEGIN
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
PLS_DECL4F_WRITEONLY(CLIP_PLANE_IDX, clipBuffer);
PLS_BLOCK_END

#ifdef @NESTED_CLIP_UPDATE_ONLY
FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32_ATOMIC(COVERAGE_BUFFER_IDX, CoverageBuffer, coverageBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END
#endif

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
#define CLOCKWISE_ATOMIC_PLS_MAIN PLS_FRAG_COLOR_MAIN
#define EMIT_CLOCKWISE_ATOMIC_PLS(FRAG_COLOR)                                  \
    _fragColor = FRAG_COLOR;                                                   \
    EMIT_PLS_AND_FRAG_COLOR
#else
#define CLOCKWISE_ATOMIC_PLS_MAIN PLS_MAIN
#define EMIT_CLOCKWISE_ATOMIC_PLS(FRAG_COLOR)                                  \
    PLS_STORE4F(colorBuffer, FRAG_COLOR);                                      \
    EMIT_PLS;
#endif

CLOCKWISE_ATOMIC_PLS_MAIN(@drawFragmentMain)
{
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_windingWeight, half);
    half fragCoverage = v_windingWeight;
#else
    VARYING_UNPACK(v_coverages, COVERAGE_TYPE);
    half fragCoverage = v_coverages.x;
#endif //@DRAW_INTERIOR_TRIANGLES

#ifdef @NESTED_CLIP_UPDATE_ONLY
    if (@NESTED_CLIP_UPDATE_ONLY)
    {
        VARYING_UNPACK(v_coveragePlacement, uint2);
        VARYING_UNPACK(v_coverageCoord, float2);

        uint coveragePitch = v_coveragePlacement.y;
        uint coverageIndex =
            v_coveragePlacement.x +
            swizzle_buffer_idx_32x32(uint2(floor(v_coverageCoord)),
                                     coveragePitch);

        uint preexistingCoverageValue =
            STORAGE_BUFFER_LOAD(coverageBuffer, coverageIndex);
        half pathCoverage;
        if (fragCoverage >= 1. &&
            (preexistingCoverageValue < uniforms.coverageBufferPrefix ||
             preexistingCoverageValue >=
                 (uniforms.coverageBufferPrefix | CLOCKWISE_FILL_ZERO_VALUE)))
        {
            // The inverse path has reached a coverage of 1, meaning, the area
            // we are erasing from the clip has reached 0.
            pathCoverage = .0;
            // No need to update the coverage buffer because the blend op is
            // min() and we have bottomed out at 0 -- it doesn't matter what any
            // future fragments do anymore.
        }
        else
        {
            // clockwiseAtomic nested clip updates take the inverse path as
            // input.
            half inversePathCoverage = fragCoverage;
            half unappliedFragCoverage = fragCoverage;
            if (preexistingCoverageValue < uniforms.coverageBufferPrefix)
            {
                // There was no borrowed coverage and we *might* be the first
                // fragment of the path to touch this pixel. Attempt to write
                // out our coverage with an atomicMax.
                uint targetCoverageValue =
                    uniforms.coverageBufferPrefix |
                    (CLOCKWISE_FILL_ZERO_VALUE +
                     clockwise_atomic_coverage_delta_to_fixed(
                         abs(fragCoverage)));
                uint coverageBeforeMax =
                    STORAGE_BUFFER_ATOMIC_MAX(coverageBuffer,
                                              coverageIndex,
                                              targetCoverageValue);
                if (coverageBeforeMax <= uniforms.coverageBufferPrefix)
                {
                    // Success! We were the first fragment of the path at this
                    // pixel.
                    unappliedFragCoverage = .0; // We're done.
                }
                else if (coverageBeforeMax < targetCoverageValue)
                {
                    // We were not first fragment of the path at this pixel, AND
                    // our atomicMax had some effect, but did not fully apply
                    // our coverage.
                    unappliedFragCoverage =
                        clockwise_atomic_fixed_to_coverage(coverageBeforeMax);
                }
            }
            if (unappliedFragCoverage > .0)
            {
                // Coverage wasn't fully applied during the implicit clear
                // operations above. Apply it now.
                uint coverageBeforeAdd = STORAGE_BUFFER_ATOMIC_ADD(
                    coverageBuffer,
                    coverageIndex,
                    clockwise_atomic_coverage_delta_to_fixed(
                        abs(unappliedFragCoverage)));
                inversePathCoverage =
                    clockwise_atomic_fixed_to_coverage(coverageBeforeAdd) +
                    fragCoverage;
            }

            // clockwiseAtomic nested clip updates take the inverse path as
            // input, so take 1 - inversePathCoverageto get back to the original
            // nested path.
            pathCoverage = 1. - inversePathCoverage;
        }

        PLS_STORE4F(clipBuffer, make_half4(pathCoverage));

        // Since the blend op is min(), emitting a color of 1 is effectively a
        // no-op as long as we're using unorm targets.
        EMIT_CLOCKWISE_ATOMIC_PLS(make_half4(1.))
    }
    else
#endif
    {
        PLS_STORE4F(clipBuffer, make_half4(fragCoverage));
        EMIT_CLOCKWISE_ATOMIC_PLS(make_half4(.0))
    }
}

#endif // FRAGMENT
