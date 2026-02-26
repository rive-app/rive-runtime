/*
 * Copyright 2026 Rive
 */

#ifdef @FRAGMENT

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32_ATOMIC(COVERAGE_BUFFER_IDX, CoverageBuffer, coverageBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

void main()
{
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_windingWeight, half);
#else
    VARYING_INIT(v_coverages, COVERAGE_TYPE);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_coveragePlacement, uint2);
    VARYING_UNPACK(v_coverageCoord, float2);

    half fragCoverage =
#ifdef @DRAW_INTERIOR_TRIANGLES
        v_windingWeight;
#else
        find_frag_coverage(v_coverages);
#endif

    uint2 coverageCoord = uint2(floor(v_coverageCoord));
    uint coveragePitch = v_coveragePlacement.y;
    uint coverageIndex = v_coveragePlacement.x +
                         swizzle_buffer_idx_32x32(coverageCoord, coveragePitch);

    // Try to apply borrowedCoverage, assuming the existing coverage value
    // is zero.
    uint borrowedCoverageFixed =
        uint(abs(fragCoverage) * CLOCKWISE_COVERAGE_PRECISION + .5);
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

#endif // FRAGMENT
