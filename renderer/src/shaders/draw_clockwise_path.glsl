/*
 * Copyright 2023 Rive
 */

#ifdef @DRAW_PATH
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
// [localVertexID, outset, fillCoverage, vertexType]
ATTR(0, float4, @a_patchVertexData);
ATTR(1, float4, @a_mirroredVertexData);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_coverages);
FLAT VARYING(1, ushort, v_pathID);
FLAT VARYING(2, uint2, v_coveragePlacement);
VARYING(3, float2, v_coverageCoord);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);

    VARYING_INIT(v_coverages, float4);
    VARYING_INIT(v_pathID, ushort);
    VARYING_INIT(v_coveragePlacement, uint2);
    VARYING_INIT(v_coverageCoord, float2);

    float4 pos;
    uint pathID;
    float2 vertexPosition;
    if (unpack_tessellated_path_vertex(@a_patchVertexData,
                                       @a_mirroredVertexData,
                                       _instanceID,
                                       pathID,
                                       vertexPosition,
                                       v_coverages VERTEX_CONTEXT_UNPACK))
    {
        uint4 coverageData =
            STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * 4u + 3u);
        v_pathID = pathID;
        v_coveragePlacement = coverageData.xy;
        v_coverageCoord = vertexPosition + uintBitsToFloat(coverageData.zw);
        pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
    }
    else
    {
        pos = float4(uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue);
    }

    VARYING_PACK(v_coverages);
    VARYING_PACK(v_pathID);
    VARYING_PACK(v_coveragePlacement);
    VARYING_PACK(v_coverageCoord);
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, packed_float3, @a_triangleVertex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
@OPTIONALLY_FLAT VARYING(0, half, v_windingWeight);
FLAT VARYING(1, ushort, v_pathID);
FLAT VARYING(2, uint2, v_coveragePlacement);
VARYING(3, float2, v_coverageCoord);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);

    VARYING_INIT(v_windingWeight, half);
    VARYING_INIT(v_pathID, ushort);
    VARYING_INIT(v_coveragePlacement, uint2);
    VARYING_INIT(v_coverageCoord, float2);

    uint pathID;
    float2 vertexPosition =
        unpack_interior_triangle_vertex(@a_triangleVertex,
                                        pathID,
                                        v_windingWeight VERTEX_CONTEXT_UNPACK);
    uint4 coverageData = STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * 4u + 3u);
    v_pathID = pathID;
    v_coveragePlacement = coverageData.xy;
    v_coverageCoord = vertexPosition + uintBitsToFloat(coverageData.zw);
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_windingWeight);
    VARYING_PACK(v_pathID);
    VARYING_PACK(v_coveragePlacement);
    VARYING_PACK(v_coverageCoord);
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef @FRAGMENT
FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
STORAGE_BUFFER_U32_ATOMIC(COVERAGE_BUFFER_IDX, CoverageBuffer, coverageBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

#ifdef @BORROWED_COVERAGE_PREPASS
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
    VARYING_UNPACK(v_coverages, float4);
    VARYING_UNPACK(v_pathID, ushort);
    VARYING_UNPACK(v_coveragePlacement, uint2);
    VARYING_UNPACK(v_coverageCoord, float2);

    half4 paintColor;
    uint pathID = v_pathID;
    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, pathID);
    uint paintType = paintData.x & 0xfu;
    if (paintType <= SOLID_COLOR_PAINT_TYPE) // CLIP_UPDATE_PAINT_TYPE or
                                             // SOLID_COLOR_PAINT_TYPE
    {
        paintColor = unpackUnorm4x8(paintData.y);
    }
    else // LINEAR_GRADIENT_PAINT_TYPE,
         // RADIAL_GRADIENT_PAINT_TYPE, or
         // IMAGE_PAINT_TYPE
    {
        float2x2 M =
            make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u));
        float4 translate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 1u);
        float2 paintCoord = MUL(M, _fragCoord) + translate.xy;
        if (paintType != IMAGE_PAINT_TYPE)
        {
            float t = paintType == LINEAR_GRADIENT_PAINT_TYPE
                          ? /*linear*/ paintCoord.x
                          : /*radial*/ length(paintCoord);
            t = clamp(t, .0, 1.);
            float x = t * translate.z + translate.w;
            float y = uintBitsToFloat(paintData.y);
            paintColor =
                TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, float2(x, y), .0);
        }
        else
        {
            float opacity = uintBitsToFloat(paintData.y);
            float lod = translate.z;
            paintColor = TEXTURE_SAMPLE_LOD(@imageTexture,
                                            imageSampler,
                                            paintCoord,
                                            lod);
            paintColor.a *= opacity;
        }
    }

    if (paintColor.a == .0)
    {
        discard;
    }

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

#ifdef @BORROWED_COVERAGE_PREPASS
    if (@BORROWED_COVERAGE_PREPASS)
    {
#ifdef @DRAW_INTERIOR_TRIANGLES
        half borrowedCoverage = -v_windingWeight;
#else
        half fragCoverage;
#ifdef @ENABLE_FEATHER
        if (@ENABLE_FEATHER && is_feathered_fill(v_coverages))
        {
            fragCoverage = eval_feathered_fill(
                v_coverages,
                SAMPLED_R16F(@featherTexture, featherSampler));
        }
        else
#endif
        {
            fragCoverage = v_coverages.x;
        }
        half borrowedCoverage = max(-fragCoverage, .0);
#endif
        apply_borrowed_coverage(borrowedCoverage, coverageIndex);
        discard;
    }
#endif // BORROWED_COVERAGE_PREPASS

#ifndef @DRAW_INTERIOR_TRIANGLES
    if (is_stroke(v_coverages))
    {
        half fragCoverage;
#ifdef @ENABLE_FEATHER
        if (@ENABLE_FEATHER && is_feathered_stroke(v_coverages))
        {
            fragCoverage = eval_feathered_stroke(
                v_coverages,
                SAMPLED_R16F(@featherTexture, featherSampler));
        }
        else
#endif
        {
            fragCoverage = min(v_coverages.x, v_coverages.y);
        }
        fragCoverage = clamp(fragCoverage, .0, 1.);
        apply_stroke_coverage(paintColor.a, fragCoverage, coverageIndex);
    }
    else // It's a fill.
#endif   // !DRAW_INTERIOR_TRIANGLES
    {
#ifdef @DRAW_INTERIOR_TRIANGLES
        half fragCoverage = v_windingWeight;
#else
        half fragCoverage;
#ifdef @ENABLE_FEATHER
        if (@ENABLE_FEATHER && is_feathered_fill(v_coverages))
        {
            fragCoverage = eval_feathered_fill(
                v_coverages,
                SAMPLED_R16F(@featherTexture, featherSampler));
        }
        else
#endif
        {
            fragCoverage = v_coverages.x;
        }
        fragCoverage = clamp(fragCoverage, .0, 1.);
#endif
        apply_fill_coverage(paintColor.a, fragCoverage, coverageIndex);
    }

    EMIT_FRAG_DATA(paintColor);
}
#endif // FRAGMENT
