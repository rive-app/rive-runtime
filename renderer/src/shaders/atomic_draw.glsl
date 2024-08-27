/*
 * Copyright 2023 Rive
 */

#ifdef @DRAW_PATH
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, float4, @a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, @a_mirroredVertexData);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, half2, v_edgeDistance);
FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);

    VARYING_INIT(v_edgeDistance, half2);
    VARYING_INIT(v_pathID, ushort);

    float4 pos;
    float2 vertexPosition;
    if (unpack_tessellated_path_vertex(@a_patchVertexData,
                                       @a_mirroredVertexData,
                                       _instanceID,
                                       v_pathID,
                                       vertexPosition,
                                       v_edgeDistance VERTEX_CONTEXT_UNPACK))
    {
        pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
    }
    else
    {
        pos = float4(uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue);
    }

    VARYING_PACK(v_edgeDistance);
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
@OPTIONALLY_FLAT VARYING(0, half, v_windingWeight);
FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);

    VARYING_INIT(v_windingWeight, half);
    VARYING_INIT(v_pathID, ushort);

    float2 vertexPosition = unpack_interior_triangle_vertex(@a_triangleVertex,
                                                            v_pathID,
                                                            v_windingWeight VERTEX_CONTEXT_UNPACK);
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_windingWeight);
    VARYING_PACK(v_pathID);
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef @DRAW_IMAGE
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
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

IMAGE_RECT_VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_imageRectVertex, float4);

    VARYING_INIT(v_texCoord, float2);
    VARYING_INIT(v_edgeCoverage, half);
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif

    bool isOuterVertex = @a_imageRectVertex.z == .0 || @a_imageRectVertex.w == .0;
    v_edgeCoverage = isOuterVertex ? .0 : 1.;

    float2 vertexPosition = @a_imageRectVertex.xy;
    float2x2 M = make_float2x2(imageDrawUniforms.viewMatrix);
    float2x2 MIT = transpose(inverse(M));
    if (!isOuterVertex)
    {
        // Inset the inner vertices to the point where coverage == 1.
        // NOTE: if width/height ever change from 1, these equations need to be updated.
        float aaRadiusX = AA_RADIUS * manhattan_width(MIT[1]) / dot(M[1], MIT[1]);
        if (aaRadiusX >= .5)
        {
            vertexPosition.x = .5;
            v_edgeCoverage *= cast_float_to_half(.5 / aaRadiusX);
        }
        else
        {
            vertexPosition.x += aaRadiusX * @a_imageRectVertex.z;
        }
        float aaRadiusY = AA_RADIUS * manhattan_width(MIT[0]) / dot(M[0], MIT[0]);
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

#else // DRAW_IMAGE_RECT -> DRAW_IMAGE_MESH
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
IMAGE_MESH_VERTEX_MAIN(@drawVertexMain, PositionAttr, position, UVAttr, uv, _vertexID)
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
#endif // DRAW_IMAGE

#ifdef @DRAW_RENDER_TARGET_UPDATE_BOUNDS
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR_BLOCK_END
#endif // VERTEX

VARYING_BLOCK_BEGIN
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

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

#ifdef @ENABLE_BINDLESS_TEXTURES
#define NEEDS_IMAGE_TEXTURE
#endif
#ifdef @DRAW_IMAGE
#define NEEDS_IMAGE_TEXTURE
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, GRAD_TEXTURE_IDX, @gradTexture);
#ifdef NEEDS_IMAGE_TEXTURE
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
#endif
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
#ifdef NEEDS_IMAGE_TEXTURE
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)
#endif

PLS_BLOCK_BEGIN
// We only write the framebuffer as a storage texture when there are blend modes. Otherwise, we
// render to it as a normal color attachment.
#ifndef @FIXED_FUNCTION_COLOR_BLEND
#ifdef @COLOR_PLANE_IDX_OVERRIDE
// D3D11 doesn't let us bind the framebuffer UAV to slot 0 when there is a color output.
PLS_DECL4F(@COLOR_PLANE_IDX_OVERRIDE, colorBuffer);
#else
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
#endif
#ifdef @ENABLE_CLIPPING
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#endif
PLS_DECLUI_ATOMIC(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

uint to_fixed(float x) { return uint(x * FIXED_COVERAGE_FACTOR + FIXED_COVERAGE_ZERO); }

half from_fixed(uint x)
{
    return cast_float_to_half(float(x) * FIXED_COVERAGE_INVERSE_FACTOR +
                              (-FIXED_COVERAGE_ZERO * FIXED_COVERAGE_INVERSE_FACTOR));
}

// Return the color of the path at index 'pathID' at location '_fragCoord'.
// Also update the PLS clip value if called for.
half4 resolve_path_color(half coverageCount,
                         uint2 paintData,
                         uint pathID FRAGMENT_CONTEXT_DECL PLS_CONTEXT_DECL,
                         OUT(uint) clipData,
                         bool needsClipData)
{
    clipData = 0u;
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if (@ENABLE_EVEN_ODD && (paintData.x & PAINT_FLAG_EVEN_ODD) != 0u)
    {
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
    }
#endif                                       // ENABLE_EVEN_ODD
    coverage = min(coverage, make_half(1.)); // This also caps stroke coverage, which can be >1.
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        uint clipID = paintData.x >> 16u;
        if (clipID != 0u || needsClipData)
        {
            clipData = PLS_LOADUI(clipBuffer);
        }
        if (clipID != 0u)
        {
            clipData = PLS_LOADUI(clipBuffer);
            half clipCoverage = clipID == (clipData >> 16u) ? unpackHalf2x16(clipData).r : .0;
            coverage = min(coverage, clipCoverage);
        }
    }
#endif // ENABLE_CLIPPING
    half4 color = make_half4(.0);
    uint paintType = paintData.x & 0xfu;
    switch (paintType)
    {
        case SOLID_COLOR_PAINT_TYPE:
            color = unpackUnorm4x8(paintData.y);
#ifdef @ENABLE_CLIPPING
            if (@ENABLE_CLIPPING)
            {
                PLS_PRESERVE_UI(clipBuffer);
            }
#endif
            break;
        case LINEAR_GRADIENT_PAINT_TYPE:
        case RADIAL_GRADIENT_PAINT_TYPE:
#ifdef @ENABLE_BINDLESS_TEXTURES
        case IMAGE_PAINT_TYPE:
#endif // ENABLE_BINDLESS_TEXTURES
        {
            float2x2 M = make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u));
            float4 translate = STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 1u);
            float2 paintCoord = MUL(M, _fragCoord) + translate.xy;
#ifdef @ENABLE_BINDLESS_TEXTURES
            if (paintType == IMAGE_PAINT_TYPE)
            {
                color = TEXTURE_SAMPLE_GRAD(sampler2D(floatBitsToUint(translate.zw)),
                                            imageSampler,
                                            paintCoord,
                                            M[0],
                                            M[1]);
                float opacity = uintBitsToFloat(paintData.y);
                color.a *= opacity;
            }
            else
#endif // ENABLE_BINDLESS_TEXTURES
            {
                float t = paintType == LINEAR_GRADIENT_PAINT_TYPE ? /*linear*/ paintCoord.x
                                                                  : /*radial*/ length(paintCoord);
                t = clamp(t, .0, 1.);
                float x = t * translate.z + translate.w;
                float y = uintBitsToFloat(paintData.y);
                color = TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, float2(x, y), .0);
            }
#ifdef @ENABLE_CLIPPING
            if (@ENABLE_CLIPPING)
            {
                PLS_PRESERVE_UI(clipBuffer);
            }
#endif
            break;
        }
#ifdef @ENABLE_CLIPPING
        case CLIP_UPDATE_PAINT_TYPE:
            if (@ENABLE_CLIPPING)
            {
                clipData = paintData.y | packHalf2x16(make_half2(coverage, .0));
                PLS_STOREUI(clipBuffer, clipData);
            }
            break;
#endif // ENABLE_CLIPPING
    }
#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT && (paintData.x & PAINT_FLAG_HAS_CLIP_RECT) != 0u)
    {
        float2x2 M = make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 2u));
        float4 translate = STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 3u);
        float2 clipCoord = MUL(M, _fragCoord) + translate.xy;
        // translate.zw contains -1 / fwidth(clipCoord), which we use to calculate antialiasing.
        half2 distXY = cast_float2_to_half2(abs(clipCoord) * translate.zw - translate.zw);
        half clipRectCoverage = clamp(min(distXY.x, distXY.y) + .5, .0, 1.);
        coverage = min(coverage, clipRectCoverage);
    }
#endif // ENABLE_CLIP_RECT
    color.a *= coverage;
    return color;
}

half4 blend_src_over(half4 srcColorPremul, half4 dstColorPremul)
{
    return srcColorPremul + dstColorPremul * (1. - srcColorPremul.a);
}

#ifndef @FIXED_FUNCTION_COLOR_BLEND
half4 blend(half4 srcColorUnmul, half4 dstColorPremul, ushort blendMode)
{
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND && blendMode != BLEND_SRC_OVER)
    {
        return advanced_blend(srcColorUnmul, unmultiply(dstColorPremul), blendMode);
    }
    else
#endif // ENABLE_ADVANCED_BLEND
    {
        return blend_src_over(premultiply(srcColorUnmul), dstColorPremul);
    }
}

half4 do_pls_blend(half4 color, uint2 paintData PLS_CONTEXT_DECL)
{
    half4 dstColorPremul = PLS_LOAD4F(colorBuffer);
    ushort blendMode = cast_uint_to_ushort((paintData.x >> 4) & 0xfu);
    return blend(color, dstColorPremul, blendMode);
}

void write_pls_blend(half4 color, uint2 paintData PLS_CONTEXT_DECL)
{
    if (color.a != .0)
    {
        half4 blendedColor = do_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
        PLS_STORE4F(colorBuffer, blendedColor);
    }
    else
    {
        PLS_PRESERVE_4F(colorBuffer);
    }
}
#endif // !FIXED_FUNCTION_COLOR_BLEND

#ifdef @FIXED_FUNCTION_COLOR_BLEND
#define ATOMIC_PLS_MAIN PLS_FRAG_COLOR_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS EMIT_PLS_AND_FRAG_COLOR
#else // !FIXED_FUNCTION_COLOR_BLEND
#define ATOMIC_PLS_MAIN PLS_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS PLS_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS EMIT_PLS
#endif

#ifdef @DRAW_PATH
ATOMIC_PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_edgeDistance, half2);
    VARYING_UNPACK(v_pathID, ushort);

    half coverage = min(min(v_edgeDistance.x, abs(v_edgeDistance.y)), make_half(1.));

    // Since v_pathID increases monotonically with every draw, and since it lives in the most
    // significant bits of the coverage data, an atomic max() function will serve 3 purposes:
    //
    //    * The invocation that changes the pathID is the single first fragment invocation to
    //      hit the new path, and the one that should resolve the previous path in the framebuffer.
    //    * Properly resets coverage to zero when we do cross over into processing a new path.
    //    * Accumulates coverage for strokes.
    //
    uint fixedCoverage = to_fixed(coverage);
    uint minCoverageData = (make_uint(v_pathID) << 16) | fixedCoverage;
    uint lastCoverageData = PLS_ATOMIC_MAX(coverageCountBuffer, minCoverageData);
    ushort lastPathID = cast_uint_to_ushort(lastCoverageData >> 16);
    if (lastPathID != v_pathID)
    {
        // We crossed into a new path! Resolve the previous path now that we know its exact
        // coverage.
        half coverageCount = from_fixed(lastCoverageData & 0xffffu);
        uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
        uint clipData;
        half4 color = resolve_path_color(coverageCount,
                                         paintData,
                                         lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK,
                                         clipData,
                                         /*needsClipData=*/false);
#ifdef @FIXED_FUNCTION_COLOR_BLEND
        _fragColor = premultiply(color);
#else
        write_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
#endif // FIXED_FUNCTION_COLOR_BLEND
    }
    else
    {
        if (v_edgeDistance.y < .0 /*fill?*/)
        {
            // We're a fill, and we did not cross into the new path this time. Count coverage.
            if (lastCoverageData < minCoverageData)
            {
                // We already crossed into this path. Oops. Undo the effect of the min().
                fixedCoverage += lastCoverageData - minCoverageData;
            }
            fixedCoverage -= uint(FIXED_COVERAGE_ZERO); // Only apply the zero bias once.
            PLS_ATOMIC_ADD(coverageCountBuffer, fixedCoverage);
        }
        // Discard because some PLS implementations require that we assign values to the color &
        // clip attachments, but since we aren't raster ordered, we don't have values to assign.
        discard;
    }

    EMIT_ATOMIC_PLS
}
#endif // DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
ATOMIC_PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_windingWeight, half);
    VARYING_UNPACK(v_pathID, ushort);

    half coverage = v_windingWeight;

    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageCountBuffer);
    ushort lastPathID = cast_uint_to_ushort(lastCoverageData >> 16);
    half lastCoverageCount = from_fixed(lastCoverageData & 0xffffu);
    if (lastPathID != v_pathID)
    {
        // We crossed into a new path! Resolve the previous path now that we know its exact
        // coverage.
        uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
        uint clipData;
        half4 color = resolve_path_color(lastCoverageCount,
                                         paintData,
                                         lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK,
                                         clipData,
                                         /*needsClipData=*/false);
#ifdef @FIXED_FUNCTION_COLOR_BLEND
        _fragColor = premultiply(color);
#else
        write_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
#endif // FIXED_FUNCTION_COLOR_BLEND
    }
    else
    {
        coverage += lastCoverageCount;
    }

    PLS_STOREUI_ATOMIC(coverageCountBuffer, (make_uint(v_pathID) << 16) | to_fixed(coverage));

    if (lastPathID == v_pathID)
    {
        // Discard because some PLS implementations require that we assign values to the color &
        // clip attachments, but since we aren't raster ordered, we don't have values to assign.
        discard;
    }

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

    // Start by finding the image color. We have to do this immediately instead of allowing it to
    // get resolved later like other draws because the @imageTexture binding is liable to change,
    // and furthermore in the case of imageMeshes, we can't calculate UV coordinates based on
    // fragment position.
    half4 imageColor = TEXTURE_SAMPLE(@imageTexture, imageSampler, v_texCoord);
    half meshCoverage = 1.;
#ifdef @DRAW_IMAGE_RECT
    meshCoverage = min(v_edgeCoverage, meshCoverage);
#endif
#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        half clipRectCoverage = min_value(cast_float4_to_half4(v_clipRect));
        meshCoverage = clamp(clipRectCoverage, make_half(.0), meshCoverage);
    }
#endif

#ifdef @DRAW_IMAGE_MESH
    // TODO: If we care: Use the interlock if we can, since individual meshes may shimmer if they
    // have overlapping triangles.
    PLS_INTERLOCK_BEGIN;
#endif

    // Find the previous path color. (This might also update the clip buffer.)
    // TODO: skip this step if no clipping AND srcOver AND imageColor is solid.
    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageCountBuffer);
    half coverageCount = from_fixed(lastCoverageData & 0xffffu);
    ushort lastPathID = cast_uint_to_ushort(lastCoverageData >> 16);
    uint2 lastPaintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
    uint clipData;
    half4 lastColor = resolve_path_color(coverageCount,
                                         lastPaintData,
                                         lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK,
                                         clipData,
                                         /*needsClipData=*/true);

    // Clip the image after resolving the previous path, since that can affect the clip buffer.
#ifdef @ENABLE_CLIPPING // TODO! ENABLE_IMAGE_CLIPPING in addition to ENABLE_CLIPPING?
    if (@ENABLE_CLIPPING && imageDrawUniforms.clipID != 0u)
    {
        uint clipID = clipData >> 16;
        half clipCoverage = clipID == imageDrawUniforms.clipID ? unpackHalf2x16(clipData).r : .0;
        meshCoverage = min(meshCoverage, clipCoverage);
    }
#endif // ENABLE_CLIPPING
    imageColor.a *= meshCoverage * cast_float_to_half(imageDrawUniforms.opacity);

#ifdef @FIXED_FUNCTION_COLOR_BLEND
    // Leverage the property that premultiplied src-over blending is associative and blend the
    // imageColor and lastColor before passing them on to the blending pipeline.
    _fragColor = blend_src_over(premultiply(imageColor), premultiply(lastColor));
#else
    if (lastColor.a != .0 || imageColor.a != .0)
    {
        // Blend the previous path and image both in a single operation.
        // TODO: Are advanced blend modes associative? srcOver is, so at least there we can blend
        // lastColor and imageColor first, and potentially avoid a framebuffer load if it ends up
        // opaque.
        half4 dstColorPremul = PLS_LOAD4F(colorBuffer);
        ushort lastBlendMode = cast_uint_to_ushort((lastPaintData.x >> 4) & 0xfu);
        ushort imageBlendMode = cast_uint_to_ushort(imageDrawUniforms.blendMode);
        dstColorPremul = blend(lastColor, dstColorPremul, lastBlendMode);
        imageColor = blend(imageColor, dstColorPremul, imageBlendMode);
        PLS_STORE4F(colorBuffer, imageColor);
    }
    else
    {
        PLS_PRESERVE_4F(colorBuffer);
    }
#endif // FIXED_FUNCTION_COLOR_BLEND

    // Write out a coverage value of "zero at pathID=0" so a future resolve attempt doesn't affect
    // this pixel.
    PLS_STOREUI_ATOMIC(coverageCountBuffer, uint(FIXED_COVERAGE_ZERO));

#ifdef @DRAW_IMAGE_MESH
    // TODO: If we care: Use the interlock if we can, since individual meshes may shimmer if they
    // have overlapping triangles.
    PLS_INTERLOCK_END;
#endif

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
    PLS_STOREUI_ATOMIC(coverageCountBuffer, uniforms.coverageClearValue);
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        PLS_STOREUI(clipBuffer, 0u);
    }
#endif
#ifdef @FIXED_FUNCTION_COLOR_BLEND
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
    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageCountBuffer);
    half coverageCount = from_fixed(lastCoverageData & 0xffffu);
    ushort lastPathID = cast_uint_to_ushort(lastCoverageData >> 16);
    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
    uint clipData;
    half4 color = resolve_path_color(coverageCount,
                                     paintData,
                                     lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK,
                                     clipData,
                                     false);
#ifdef @COALESCED_PLS_RESOLVE_AND_TRANSFER
    _fragColor = do_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
    EMIT_PLS_AND_FRAG_COLOR
#else
#ifdef @FIXED_FUNCTION_COLOR_BLEND
    _fragColor = premultiply(color);
#else
    write_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
#endif // FIXED_FUNCTION_COLOR_BLEND
    EMIT_ATOMIC_PLS
#endif // COALESCED_PLS_RESOLVE_AND_TRANSFER
}
#endif // RESOLVE_PLS
#endif // FRAGMENT
