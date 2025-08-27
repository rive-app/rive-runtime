/*
 * Copyright 2023 Rive
 */

#ifdef @DRAW_PATH
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0,
     float4,
     @a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, @a_mirroredVertexData);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
#ifdef @ENABLE_FEATHER
NO_PERSPECTIVE VARYING(0, float4, v_coverages);
#else
NO_PERSPECTIVE VARYING(0, half2, v_coverages);
#endif
FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);

#ifdef @ENABLE_FEATHER
    VARYING_INIT(v_coverages, float4);
#else
    VARYING_INIT(v_coverages, half2);
#endif
    VARYING_INIT(v_pathID, ushort);

    float4 pos;
    uint pathID;
    float2 vertexPosition;
    float4 coverages;
    if (unpack_tessellated_path_vertex(@a_patchVertexData,
                                       @a_mirroredVertexData,
                                       _instanceID,
                                       pathID,
                                       vertexPosition,
                                       coverages VERTEX_CONTEXT_UNPACK))
    {
#ifdef @ENABLE_FEATHER
        v_coverages = coverages;
#else
        v_coverages.xy = cast_float2_to_half2(coverages.xy);
#endif
        v_pathID = cast_uint_to_ushort(pathID);
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
#ifdef @ATLAS_BLIT
NO_PERSPECTIVE VARYING(0, float2, v_atlasCoord);
#else
@OPTIONALLY_FLAT VARYING(0, half, v_windingWeight);
#endif
FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);

#ifdef @ATLAS_BLIT
    VARYING_INIT(v_atlasCoord, float2);
#else
    VARYING_INIT(v_windingWeight, half);
#endif
    VARYING_INIT(v_pathID, ushort);

    uint pathID;
    float2 vertexPosition;
#ifdef @ATLAS_BLIT
    vertexPosition =
        unpack_atlas_coverage_vertex(@a_triangleVertex,
                                     pathID,
                                     v_atlasCoord VERTEX_CONTEXT_UNPACK);
#else
    vertexPosition =
        unpack_interior_triangle_vertex(@a_triangleVertex,
                                        pathID,
                                        v_windingWeight VERTEX_CONTEXT_UNPACK);
#endif
    v_pathID = cast_uint_to_ushort(pathID);
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

#ifdef @ATLAS_BLIT
    VARYING_PACK(v_atlasCoord);
#else
    VARYING_PACK(v_windingWeight);
#endif
    VARYING_PACK(v_pathID);
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef @DRAW_IMAGE_RECT
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, float4, @a_imageRectVertex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
NO_PERSPECTIVE VARYING(1, half, v_edgeCoverage);
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX
IMAGE_RECT_VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_imageRectVertex, float4);

    VARYING_INIT(v_texCoord, float2);
    VARYING_INIT(v_edgeCoverage, half);
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif

    bool isOuterVertex =
        @a_imageRectVertex.z == .0 || @a_imageRectVertex.w == .0;
    v_edgeCoverage = isOuterVertex ? .0 : 1.;

    float2 vertexPosition = @a_imageRectVertex.xy;
    float2x2 M = make_float2x2(imageDrawUniforms.viewMatrix);
    float2x2 MIT = transpose(inverse(M));
    if (!isOuterVertex)
    {
        // Inset the inner vertices to the point where coverage == 1.
        // NOTE: if width/height ever change from 1, these equations need to be
        // updated.
        float aaRadiusX =
            AA_RADIUS * manhattan_width(MIT[1]) / dot(M[1], MIT[1]);
        if (aaRadiusX >= .5)
        {
            vertexPosition.x = .5;
            v_edgeCoverage *= cast_float_to_half(.5 / aaRadiusX);
        }
        else
        {
            vertexPosition.x += aaRadiusX * @a_imageRectVertex.z;
        }
        float aaRadiusY =
            AA_RADIUS * manhattan_width(MIT[0]) / dot(M[0], MIT[0]);
        if (aaRadiusY >= .5)
        {
            vertexPosition.y = .5;
            v_edgeCoverage *= cast_float_to_half(.5 / aaRadiusY);
        }
        else
        {
            vertexPosition.y += aaRadiusY * @a_imageRectVertex.w;
        }
    }

    v_texCoord = vertexPosition;
    vertexPosition = MUL(M, vertexPosition) + imageDrawUniforms.translate;

    if (isOuterVertex)
    {
        // Outset the outer vertices to the point where coverage == 0.
        float2 n = MUL(MIT, @a_imageRectVertex.zw);
        n *= manhattan_width(n) / dot(n, n);
        vertexPosition += AA_RADIUS * n;
    }

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        v_clipRect = find_clip_rect_coverage_distances(
            make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
            imageDrawUniforms.clipRectInverseTranslate,
            vertexPosition);
    }
#endif

    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_texCoord);
    VARYING_PACK(v_edgeCoverage);
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
    EMIT_VERTEX(pos);
}
#endif // VERTEX

#elif defined(@DRAW_IMAGE_MESH)
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(PositionAttr)
ATTR(0, float2, @a_position);
ATTR_BLOCK_END

ATTR_BLOCK_BEGIN(UVAttr)
ATTR(1, float2, @a_texCoord);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(1, float4, v_clipRect);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX
IMAGE_MESH_VERTEX_MAIN(@drawVertexMain,
                       PositionAttr,
                       position,
                       UVAttr,
                       uv,
                       _vertexID)
{
    ATTR_UNPACK(_vertexID, position, @a_position, float2);
    ATTR_UNPACK(_vertexID, uv, @a_texCoord, float2);

    VARYING_INIT(v_texCoord, float2);
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif

    float2x2 M = make_float2x2(imageDrawUniforms.viewMatrix);
    float2 vertexPosition = MUL(M, @a_position) + imageDrawUniforms.translate;
    v_texCoord = @a_texCoord;

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        v_clipRect = find_clip_rect_coverage_distances(
            make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
            imageDrawUniforms.clipRectInverseTranslate,
            vertexPosition);
    }
#endif

    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_texCoord);
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_IMAGE_MESH

#ifdef @DRAW_RENDER_TARGET_UPDATE_BOUNDS
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR_BLOCK_END
#endif // VERTEX

VARYING_BLOCK_BEGIN
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    int2 coord;
    coord.x = (_vertexID & 1) == 0 ? uniforms.renderTargetUpdateBounds.x
                                   : uniforms.renderTargetUpdateBounds.z;
    coord.y = (_vertexID & 2) == 0 ? uniforms.renderTargetUpdateBounds.y
                                   : uniforms.renderTargetUpdateBounds.w;
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(float2(coord));
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_RENDER_TARGET_UPDATE_BOUNDS

#ifdef @DRAW_IMAGE
#define NEEDS_IMAGE_TEXTURE
#endif

#ifdef @FRAGMENT
PLS_BLOCK_BEGIN
// We only bind the framebuffer as PLS when there are blend modes. Otherwise, we
// render to it as a normal color attachment.
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
#ifdef @COLOR_PLANE_IDX_OVERRIDE
// D3D11 doesn't let us bind the framebuffer UAV to slot 0 when there is a color
// output.
#define LOCAL_COLOR_PLANE_IDX @COLOR_PLANE_IDX_OVERRIDE
#else
#define LOCAL_COLOR_PLANE_IDX COLOR_PLANE_IDX
#endif
#ifdef @COALESCED_PLS_RESOLVE_AND_TRANSFER
PLS_DECL4F_READONLY(LOCAL_COLOR_PLANE_IDX, colorBuffer);
#else
PLS_DECL4F(LOCAL_COLOR_PLANE_IDX, colorBuffer);
#endif
#endif // !FIXED_FUNCTION_COLOR_OUTPUT
#ifdef @PLS_BLEND_SRC_OVER
// When PLS has src-over blending enabled, the clip buffer is RGBA8 so we can
// preserve clip contents by emitting a=0 instead of loading the current value.
// This is also is a hint to the hardware that it doesn't need to write anything
// to the clip attachment.
#define CLIP_VALUE_TYPE half4
#define PLS_LOAD_CLIP_TYPE PLS_LOAD4F
#define MAKE_NON_UPDATING_CLIP_VALUE make_half4(.0)
#define HAS_UPDATED_CLIP_VALUE(X) ((X).a != .0)
#ifdef @ENABLE_CLIPPING
#ifndef @RESOLVE_PLS
PLS_DECL4F(CLIP_PLANE_IDX, clipBuffer);
#else
PLS_DECL4F_READONLY(CLIP_PLANE_IDX, clipBuffer);
#endif
#endif // ENABLE_CLIPPING
#else
// When PLS does not have src-over blending, the clip buffer the usual packed
// R32UI.
#define CLIP_VALUE_TYPE uint
#define MAKE_NON_UPDATING_CLIP_VALUE 0u
#define PLS_LOAD_CLIP_TYPE PLS_LOADUI
#define HAS_UPDATED_CLIP_VALUE(X) ((X) != 0u)
#ifdef @ENABLE_CLIPPING
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#endif // ENABLE_CLIPPING
#endif // !PLS_BLEND_SRC_OVER
PLS_DECLUI_ATOMIC(COVERAGE_PLANE_IDX, coverageAtomicBuffer);
PLS_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

INLINE uint to_fixed(float x)
{
    return uint(round(x * FIXED_COVERAGE_PRECISION + FIXED_COVERAGE_ZERO));
}

INLINE half from_fixed(uint x)
{
    return cast_float_to_half(
        float(x) * FIXED_COVERAGE_INVERSE_PRECISION +
        (-FIXED_COVERAGE_ZERO * FIXED_COVERAGE_INVERSE_PRECISION));
}

#ifdef @ENABLE_CLIPPING
INLINE void apply_clip(uint clipID,
                       CLIP_VALUE_TYPE clipData,
                       INOUT(half) coverage)
{
#ifdef @PLS_BLEND_SRC_OVER
    // The clipID is packed into r & g of clipData. Use a fuzzy equality test
    // since we lose precision when the clip value gets stored to and from the
    // attachment.
    if (all(lessThan(abs(clipData.rg - unpackUnorm4x8(clipID).rg),
                     make_half2(.25 / 255.))))
        coverage = min(coverage, clipData.b);
    else
        coverage = .0;
#else
    // The clipID is the top 16 bits of clipData.
    if (clipID == clipData >> 16)
        coverage = min(coverage, unpackHalf2x16(clipData).r);
    else
        coverage = .0;
#endif
}
#endif

INLINE void resolve_paint(uint pathID,
                          half coverageCount,
                          OUT(half4) fragColorOut
#if defined(@ENABLE_CLIPPING) && !defined(@RESOLVE_PLS)
                          ,
                          INOUT(CLIP_VALUE_TYPE) fragClipOut
#endif
                              FRAGMENT_CONTEXT_DECL PLS_CONTEXT_DECL)
{
    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, pathID);
    half coverage = coverageCount;
    if ((paintData.x & (PAINT_FLAG_NON_ZERO_FILL | PAINT_FLAG_EVEN_ODD_FILL)) !=
        0u)
    {
        // This path has a legacy (non-clockwise) fill.
        coverage = abs(coverage);
#ifdef @ENABLE_EVEN_ODD
        if (@ENABLE_EVEN_ODD && (paintData.x & PAINT_FLAG_EVEN_ODD_FILL) != 0u)
        {
            coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
        }
#endif
    }
    // This also caps stroke coverage, which can be >1.
    coverage = clamp(coverage, make_half(.0), make_half(1.));
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        uint clipID = paintData.x >> 16u;
        if (clipID != 0u)
        {
            apply_clip(clipID, PLS_LOAD_CLIP_TYPE(clipBuffer), coverage);
        }
    }
#endif // ENABLE_CLIPPING
#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT && (paintData.x & PAINT_FLAG_HAS_CLIP_RECT) != 0u)
    {
        float2x2 M = make_float2x2(
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 2u));
        float4 translate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 3u);
        float2 clipCoord = MUL(M, _fragCoord) + translate.xy;
        // translate.zw contains -1 / fwidth(clipCoord), which we use to
        // calculate antialiasing.
        half2 distXY =
            cast_float2_to_half2(abs(clipCoord) * translate.zw - translate.zw);
        half clipRectCoverage = clamp(min(distXY.x, distXY.y) + .5, .0, 1.);
        coverage = min(coverage, clipRectCoverage);
    }
#endif // ENABLE_CLIP_RECT
    uint paintType = paintData.x & 0xfu;
    if (paintType <= SOLID_COLOR_PAINT_TYPE) // CLIP_UPDATE_PAINT_TYPE or
                                             // SOLID_COLOR_PAINT_TYPE
    {
        fragColorOut = unpackUnorm4x8(paintData.y);
#ifdef @ENABLE_CLIPPING
        if (@ENABLE_CLIPPING && paintType == CLIP_UPDATE_PAINT_TYPE)
        {
#ifndef @RESOLVE_PLS
#ifdef @PLS_BLEND_SRC_OVER
            // This was actually a clip update. fragColorOut contains the packed
            // clipID.
            fragClipOut.rg = fragColorOut.ba; // Pack the clipID into r & g.
            fragClipOut.b = coverage;         // Put the clipCoverage in b.
            fragClipOut.a =
                1.; // a=1 so we replace the value in the clipBuffer.
#else
            fragClipOut = paintData.y | packHalf2x16(make_half2(coverage, .0));
#endif
#endif
            // Don't update the colorBuffer when this is a clip update.
            fragColorOut = make_half4(.0);
        }
#endif
    }
    else // LINEAR_GRADIENT_PAINT_TYPE or RADIAL_GRADIENT_PAINT_TYPE
    {
        float2x2 M =
            make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u));
        float4 translate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 1u);
        float2 paintCoord = MUL(M, _fragCoord) + translate.xy;
        float t = paintType == LINEAR_GRADIENT_PAINT_TYPE
                      ? /*linear*/ paintCoord.x
                      : /*radial*/ length(paintCoord);
        t = clamp(t, .0, 1.);
        float x = t * translate.z + translate.w;
        float y = uintBitsToFloat(paintData.y);
        fragColorOut =
            TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, float2(x, y), .0);
    }
    fragColorOut.a *= coverage;

#if !defined(@FIXED_FUNCTION_COLOR_OUTPUT) && defined(@ENABLE_ADVANCED_BLEND)
    // Apply the advanced blend mode, if applicable.
    ushort blendMode;
    if (@ENABLE_ADVANCED_BLEND && fragColorOut.a != .0 &&
        (blendMode = cast_uint_to_ushort((paintData.x >> 4) & 0xfu)) != 0u)
    {
        half4 dstColorPremul = PLS_LOAD4F(colorBuffer);
        fragColorOut.rgb =
            advanced_color_blend(fragColorOut.rgb, dstColorPremul, blendMode);
    }
#endif // !FIXED_FUNCTION_COLOR_OUTPUT && ENABLE_ADVANCED_BLEND

    // When PLS_BLEND_SRC_OVER is defined, the caller and/or blend state
    // multiply alpha into fragColorOut for us. Otherwise, we have to
    // premultiply it.
#ifndef @PLS_BLEND_SRC_OVER
    fragColorOut.rgb *= fragColorOut.a;
#endif

    // Certain platforms give us less control of the format of what we are
    // rendering too. Specifically, we are auto converted from linear -> sRGB on
    // render target writes in unreal. In those cases we made need to end up in
    // linear color space
#ifdef @NEEDS_GAMMA_CORRECTION
    fragColorOut = gamma_to_linear(fragColorOut);
#endif
}

#if !defined(@FIXED_FUNCTION_COLOR_OUTPUT) &&                                  \
    !defined(@COALESCED_PLS_RESOLVE_AND_TRANSFER)
INLINE void blend_pls_color_src_over(half4 fragColorOut PLS_CONTEXT_DECL)
{
#ifndef @PLS_BLEND_SRC_OVER
    if (fragColorOut.a == .0)
        return;
    float oneMinusSrcAlpha = 1. - fragColorOut.a;
    if (oneMinusSrcAlpha != .0)
        fragColorOut += PLS_LOAD4F(colorBuffer) * oneMinusSrcAlpha;
#endif
    PLS_STORE4F(colorBuffer, fragColorOut);
}
#endif // !@FIXED_FUNCTION_COLOR_OUTPUT && !@COALESCED_PLS_RESOLVE_AND_TRANSFER

#if defined(@ENABLE_CLIPPING) && !defined(@RESOLVE_PLS)
INLINE void emit_pls_clip(CLIP_VALUE_TYPE fragClipOut PLS_CONTEXT_DECL)
{
#ifdef @PLS_BLEND_SRC_OVER
    PLS_STORE4F(clipBuffer, fragClipOut);
#else
    if (fragClipOut != 0u)
        PLS_STOREUI(clipBuffer, fragClipOut);
#endif
}
#endif // ENABLE_CLIPPING && !RESOLVE_PLS

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
#define ATOMIC_PLS_MAIN PLS_FRAG_COLOR_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS                                    \
    PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS EMIT_PLS_AND_FRAG_COLOR
#else // !FIXED_FUNCTION_COLOR_OUTPUT
#define ATOMIC_PLS_MAIN PLS_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS PLS_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS EMIT_PLS
#endif

#ifdef @DRAW_PATH
ATOMIC_PLS_MAIN(@drawFragmentMain)
{
#ifdef @ENABLE_FEATHER
    VARYING_UNPACK(v_coverages, float4);
#else
    VARYING_UNPACK(v_coverages, half2);
#endif
    VARYING_UNPACK(v_pathID, ushort);

    half fragmentCoverage;
#ifdef @ENABLE_FEATHER
    if (@ENABLE_FEATHER && is_feathered_stroke(v_coverages))
    {
        fragmentCoverage =
            eval_feathered_stroke(v_coverages TEXTURE_CONTEXT_FORWARD);
    }
    else if (@ENABLE_FEATHER && is_feathered_fill(v_coverages))
    {
        fragmentCoverage =
            eval_feathered_fill(v_coverages TEXTURE_CONTEXT_FORWARD);
    }
    else
#endif
    {
        // Cover stroke and fill both in a branchless expression.
        fragmentCoverage =
            min(min(make_half(v_coverages.x), abs(make_half(v_coverages.y))),
                make_half(1.));
    }

    half4 fragColorOut = make_half4(.0);
#ifdef @ENABLE_CLIPPING
    CLIP_VALUE_TYPE fragClipOut = MAKE_NON_UPDATING_CLIP_VALUE;
#endif

    // Since v_pathID increases monotonically with every draw, and since it
    // lives in the most significant bits of the coverage data, an atomic max()
    // function will serve 3 purposes:
    //
    //    * The invocation that changes the pathID is the single first fragment
    //      invocation to hit the new path, and the one that should resolve the
    //      previous path in the framebuffer.
    //    * Properly resets coverage to zero when we do cross over into
    //      processing a new path.
    //    * Accumulates coverage for strokes.
    //
    uint fixedCoverage = to_fixed(fragmentCoverage);
    uint minCoverageData =
        (make_uint(v_pathID) << FIXED_COVERAGE_BIT_COUNT) | fixedCoverage;
    uint lastCoverageData =
        PLS_ATOMIC_MAX(coverageAtomicBuffer, minCoverageData);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);
    if (lastPathID == v_pathID)
    {
        // This is not the first fragment of the current path to touch this
        // pixel. We already resolved the previous path, so just update coverage
        // (if we're a fill) and move on.
        if (!is_stroke(v_coverages))
        {
            // Only apply the effect of the min() the first time we cross into a
            // path.
            fixedCoverage +=
                lastCoverageData - max(minCoverageData, lastCoverageData);
            fixedCoverage -=
                FIXED_COVERAGE_ZERO_UINT; // Only apply the zero bias once.
            PLS_ATOMIC_ADD(coverageAtomicBuffer,
                           fixedCoverage); // Count coverage.
        }
    }
    else
    {
        // We crossed into a new path! Resolve the previous path now that we
        // know its exact coverage.
        half coverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
        resolve_paint(lastPathID,
                      coverageCount,
                      fragColorOut
#ifdef @ENABLE_CLIPPING
                      ,
                      fragClipOut
#endif
                          FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
    }

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    blend_pls_color_src_over(fragColorOut PLS_CONTEXT_UNPACK);
#endif
#ifdef @ENABLE_CLIPPING
    emit_pls_clip(fragClipOut PLS_CONTEXT_UNPACK);
#endif

    EMIT_ATOMIC_PLS
}
#endif // DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
ATOMIC_PLS_MAIN(@drawFragmentMain)
{
#ifdef @ATLAS_BLIT
    VARYING_UNPACK(v_atlasCoord, float2);
#else
    VARYING_UNPACK(v_windingWeight, half);
#endif
    VARYING_UNPACK(v_pathID, ushort);

    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageAtomicBuffer);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);

    // Update coverageAtomicBuffer with the coverage weight of the current
    // triangle. This does not need to be atomic since interior triangles don't
    // overlap.
    uint currPathCoverageData;
#ifndef @ATLAS_BLIT
    if (lastPathID == v_pathID)
    {
        currPathCoverageData = lastCoverageData;
    }
    else
#endif
    {
        currPathCoverageData =
            (make_uint(v_pathID) << FIXED_COVERAGE_BIT_COUNT) +
            FIXED_COVERAGE_ZERO_UINT;
    }

    half coverage;
#ifdef @ATLAS_BLIT
    coverage = filter_feather_atlas(
        v_atlasCoord,
        uniforms.atlasTextureInverseSize TEXTURE_CONTEXT_FORWARD);
#else
    coverage = v_windingWeight;
#endif

    int coverageDeltaFixed = int(round(coverage * FIXED_COVERAGE_PRECISION));
    PLS_STOREUI_ATOMIC(coverageAtomicBuffer,
                       currPathCoverageData + uint(coverageDeltaFixed));

    half4 fragColorOut = make_half4(.0);
#ifdef @ENABLE_CLIPPING
    CLIP_VALUE_TYPE fragClipOut = MAKE_NON_UPDATING_CLIP_VALUE;
#endif

#ifndef @ATLAS_BLIT
    // If this is not the first fragment of the current path to touch this
    // pixel, then we've already resolved the previous path and can move on.
    if (lastPathID != v_pathID)
#endif
    {
        // We crossed into a new path! Resolve the previous path now that we
        // know its exact coverage.
        half lastCoverageCount =
            from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
        resolve_paint(lastPathID,
                      lastCoverageCount,
                      fragColorOut
#ifdef @ENABLE_CLIPPING
                      ,
                      fragClipOut
#endif
                          FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
    }

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    blend_pls_color_src_over(fragColorOut PLS_CONTEXT_UNPACK);
#endif
#ifdef @ENABLE_CLIPPING
    emit_pls_clip(fragClipOut PLS_CONTEXT_UNPACK);
#endif

    EMIT_ATOMIC_PLS
}
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef @DRAW_IMAGE
ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS(@drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);
#ifdef @DRAW_IMAGE_RECT
    VARYING_UNPACK(v_edgeCoverage, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif

    // Start by finding the image color. We have to do this immediately instead
    // of allowing it to get resolved later like other draws because the
    // @imageTexture binding is liable to change, and furthermore in the case of
    // imageMeshes, we can't calculate UV coordinates based on fragment
    // position.
    half4 imageColor =
        TEXTURE_SAMPLE_DYNAMIC(@imageTexture, imageSampler, v_texCoord);
    half imageCoverage = 1.;
#ifdef @DRAW_IMAGE_RECT
    imageCoverage = min(v_edgeCoverage, imageCoverage);
#endif
#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        half clipRectCoverage = min_value(cast_float4_to_half4(v_clipRect));
        imageCoverage = clamp(clipRectCoverage, make_half(.0), imageCoverage);
    }
#endif

    // Resolve the previous path.
    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageAtomicBuffer);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);
    half lastCoverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
    half4 fragColorOut;
#ifdef @ENABLE_CLIPPING
    CLIP_VALUE_TYPE fragClipOut = MAKE_NON_UPDATING_CLIP_VALUE;
#endif
    // TODO: consider not resolving the prior paint if we're solid and the prior
    // paint is not a clip update: (imageColor.a == 1. &&
    //                              imageDrawUniforms.blendMode ==
    //                              BLEND_SRC_OVER && priorPaintType !=
    //                              CLIP_UPDATE_PAINT_TYPE)
    resolve_paint(lastPathID,
                  lastCoverageCount,
                  fragColorOut
#ifdef @ENABLE_CLIPPING
                  ,
                  fragClipOut
#endif
                      FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);

#ifdef @PLS_BLEND_SRC_OVER
    // Image draws use a premultiplied blend state, but resolve_paint() did not
    // premultiply fragColorOut. Multiply fragColorOut by alpha now.
    fragColorOut.rgb *= fragColorOut.a;
#endif

    // Clip the image after resolving the previous path, since that can affect
    // the clip buffer.
#ifdef @ENABLE_CLIPPING // TODO! ENABLE_IMAGE_CLIPPING in addition to
                        // ENABLE_CLIPPING?
    if (@ENABLE_CLIPPING && imageDrawUniforms.clipID != 0u)
    {
        CLIP_VALUE_TYPE clipData = HAS_UPDATED_CLIP_VALUE(fragClipOut)
                                       ? fragClipOut
                                       : PLS_LOAD_CLIP_TYPE(clipBuffer);
        apply_clip(imageDrawUniforms.clipID, clipData, imageCoverage);
    }
#endif // ENABLE_CLIPPING

    // Prepare imageColor for premultiplied src-over blending.
#if !defined(@FIXED_FUNCTION_COLOR_OUTPUT) && defined(@ENABLE_ADVANCED_BLEND)
    if (@ENABLE_ADVANCED_BLEND && imageDrawUniforms.blendMode != BLEND_SRC_OVER)
    {
        // Calculate what dstColorPremul will be after applying fragColorOut.
        half4 dstColorPremul =
            PLS_LOAD4F(colorBuffer) * (1. - fragColorOut.a) + fragColorOut;
        // Calculate the imageColor to emit *BEFORE* src-over blending, such
        // that the post-src-over-blend result is equivalent to the blendMode.
        imageColor.rgb = advanced_color_blend(
                             unmultiply_rgb(imageColor),
                             dstColorPremul,
                             cast_uint_to_ushort(imageDrawUniforms.blendMode)) *
                         imageColor.a;
    }
#endif // !FIXED_FUNCTION_COLOR_OUTPUT && ENABLE_ADVANCED_BLEND
    imageColor *= imageCoverage * cast_float_to_half(imageDrawUniforms.opacity);

    // Leverage the property that premultiplied src-over blending is associative
    // and blend the imageColor and fragColorOut before passing them on to the
    // blending pipeline.
    fragColorOut = fragColorOut * (1. - imageColor.a) + imageColor;

#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    blend_pls_color_src_over(fragColorOut PLS_CONTEXT_UNPACK);
#endif
#ifdef @ENABLE_CLIPPING
    emit_pls_clip(fragClipOut PLS_CONTEXT_UNPACK);
#endif

    // Write out a coverage value of "zero at pathID=0" so a future resolve
    // attempt doesn't affect this pixel.
    PLS_STOREUI_ATOMIC(coverageAtomicBuffer, FIXED_COVERAGE_ZERO_UINT);

    EMIT_ATOMIC_PLS
}
#endif // DRAW_IMAGE

#ifdef @INITIALIZE_PLS

ATOMIC_PLS_MAIN(@drawFragmentMain)
{
#ifdef @STORE_COLOR_CLEAR
    PLS_STORE4F(colorBuffer, unpackUnorm4x8(uniforms.colorClearValue));
#endif
#ifdef @SWIZZLE_COLOR_BGRA_TO_RGBA
    half4 color = PLS_LOAD4F(colorBuffer);
    PLS_STORE4F(colorBuffer, color.bgra);
#endif
    PLS_STOREUI_ATOMIC(coverageAtomicBuffer, uniforms.coverageClearValue);
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        PLS_STOREUI(clipBuffer, 0u);
    }
#endif
#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    discard;
#endif
    EMIT_ATOMIC_PLS
}

#endif // INITIALIZE_PLS

#ifdef @RESOLVE_PLS

#ifdef @COALESCED_PLS_RESOLVE_AND_TRANSFER
PLS_FRAG_COLOR_MAIN(@drawFragmentMain)
#else
ATOMIC_PLS_MAIN(@drawFragmentMain)
#endif
{
    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageAtomicBuffer);
    half coverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);
    half4 fragColorOut;
    resolve_paint(lastPathID,
                  coverageCount,
                  fragColorOut FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
#ifdef @COALESCED_PLS_RESOLVE_AND_TRANSFER
#ifdef @PLS_BLEND_SRC_OVER
    // When PLS_BLEND_SRC_OVER is defined, the blend state usually multiplies
    // alpha into fragColorOut for us. But since the coalesced resolve does not
    // use blend, premultiply it now.
    fragColorOut.rgb *= fragColorOut.a;
#endif
    float oneMinusSrcAlpha = 1. - fragColorOut.a;
    if (oneMinusSrcAlpha != .0)
        fragColorOut += PLS_LOAD4F(colorBuffer) * oneMinusSrcAlpha;
    _fragColor = fragColorOut;
    EMIT_PLS_AND_FRAG_COLOR
#else
#ifdef @FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    blend_pls_color_src_over(fragColorOut PLS_CONTEXT_UNPACK);
#endif
    EMIT_ATOMIC_PLS
#endif // COALESCED_PLS_RESOLVE_AND_TRANSFER
}
#endif // RESOLVE_PLS
#endif // FRAGMENT
