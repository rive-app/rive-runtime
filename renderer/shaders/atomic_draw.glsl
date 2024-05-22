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
@OPTIONALLY_FLAT VARYING(1, ushort, v_pathID);
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
@OPTIONALLY_FLAT VARYING(1, ushort, v_pathID);
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
            v_edgeCoverage *= make_half(.5 / aaRadiusX);
        }
        else
        {
            vertexPosition.x += aaRadiusX * @a_imageRectVertex.z;
        }
        float aaRadiusY = AA_RADIUS * manhattan_width(MIT[0]) / dot(M[0], MIT[0]);
        if (aaRadiusY >= .5)
        {
            vertexPosition.y = .5;
            v_edgeCoverage *= make_half(.5 / aaRadiusY);
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
    v_clipRect =
        find_clip_rect_coverage_distances(make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
                                          imageDrawUniforms.clipRectInverseTranslate,
                                          vertexPosition);
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
    v_clipRect =
        find_clip_rect_coverage_distances(make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
                                          imageDrawUniforms.clipRectInverseTranslate,
                                          vertexPosition);
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
TEXTURE_RGBA8(GRAD_TEXTURE_IDX, @gradTexture);
#ifdef NEEDS_IMAGE_TEXTURE
TEXTURE_RGBA8(IMAGE_TEXTURE_IDX, @imageTexture);
#endif
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
#ifdef NEEDS_IMAGE_TEXTURE
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)
#endif

PLS_BLOCK_BEGIN
// We only write the framebuffer as a storage texture when there are blend modes. Otherwise, we
// render to it as a normal color attachment.
#ifdef @ENABLE_ADVANCED_BLEND
#ifdef @FRAMEBUFFER_PLANE_IDX_OVERRIDE
// D3D11 doesn't let us bind the framebuffer UAV to slot 0 when there is a color output.
PLS_DECL4F(@FRAMEBUFFER_PLANE_IDX_OVERRIDE, framebuffer);
#else
PLS_DECL4F(FRAMEBUFFER_PLANE_IDX, framebuffer);
#endif
#endif
PLS_DECLUI_ATOMIC(COVERAGE_PLANE_IDX, coverageCountBuffer);
#ifdef @ENABLE_CLIPPING
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#endif
PLS_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

uint to_fixed(float x) { return uint(x * FIXED_COVERAGE_FACTOR + FIXED_COVERAGE_ZERO); }

half from_fixed(uint x)
{
    return make_half(float(x) * FIXED_COVERAGE_INVERSE_FACTOR +
           (-FIXED_COVERAGE_ZERO * FIXED_COVERAGE_INVERSE_FACTOR));
}

// Return the color of the path at index 'pathID' at location '_fragCoord'.
// Also update the PLS clip value if called for.
half4 resolve_path_color(half coverageCount,
                         uint2 paintData,
                         uint pathID FRAGMENT_CONTEXT_DECL PLS_CONTEXT_DECL)
{
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if ((paintData.x & PAINT_FLAG_EVEN_ODD) != 0u)
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
#endif                                      // ENABLE_EVEN_ODD
    coverage = min(coverage, make_half(1)); // This also caps stroke coverage, which can be >1.
#ifdef @ENABLE_CLIPPING
    uint clipID = paintData.x >> 16u;
    if (clipID != 0u)
    {
        uint clipData = PLS_LOADUI(clipBuffer);
        half clipCoverage = clipID == (clipData >> 16u) ? unpackHalf2x16(clipData).r : .0;
        coverage = min(coverage, clipCoverage);
    }
#endif // ENABLE_CLIPPING
    half4 color = make_half4(0, 0, 0, 0);
    uint paintType = paintData.x & 0xfu;
    switch (paintType)
    {
        case SOLID_COLOR_PAINT_TYPE:
            color = unpackUnorm4x8(paintData.y);
#ifdef @ENABLE_CLIPPING
            PLS_PRESERVE_VALUE(clipBuffer);
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
                color = make_half4(TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, float2(x, y), .0));
            }
#ifdef @ENABLE_CLIPPING
            PLS_PRESERVE_VALUE(clipBuffer);
#endif
            break;
        }
#ifdef @ENABLE_CLIPPING
        case CLIP_UPDATE_PAINT_TYPE:
            PLS_STOREUI(clipBuffer, paintData.y | packHalf2x16(make_half2(coverage, 0)));
            break;
#endif // ENABLE_CLIPPING
    }
#ifdef @ENABLE_CLIP_RECT
    if ((paintData.x & PAINT_FLAG_HAS_CLIP_RECT) != 0u)
    {
        float2x2 M = make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 2u));
        float4 translate = STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 3u);
        float2 clipCoord = MUL(M, _fragCoord) + translate.xy;
        // translate.zw contains -1 / fwidth(clipCoord), which we use to calculate antialiasing.
        half2 distXY = make_half2(abs(clipCoord) * translate.zw - translate.zw);
        half clipRectCoverage = clamp(min(distXY.x, distXY.y) + .5, .0, 1.);
        coverage = min(coverage, clipRectCoverage);
    }
#endif // ENABLE_CLIP_RECT
    color.a *= coverage;
    return color;
}

half4 do_src_over_blend(half4 srcColorPremul, half4 dstColorPremul)
{
    return srcColorPremul + dstColorPremul * (1. - srcColorPremul.a);
}

#ifdef @ENABLE_ADVANCED_BLEND
half4 do_advanced_blend(half4 srcColorUnmul, half4 dstColorPremul, ushort blendMode)
{
    if (blendMode != BLEND_SRC_OVER)
    {
#ifdef @ENABLE_HSL_BLEND_MODES
        return advanced_hsl_blend(
#else
        return advanced_blend(
#endif
            srcColorUnmul,
            unmultiply(dstColorPremul),
            blendMode);
    }
    else
    {
        return do_src_over_blend(premultiply(srcColorUnmul), dstColorPremul);
    }
}

half4 do_pls_blend(half4 color, uint2 paintData PLS_CONTEXT_DECL)
{
    half4 dstColorPremul = PLS_LOAD4F(framebuffer);
    ushort blendMode = make_ushort((paintData.x >> 4) & 0xfu);
    return do_advanced_blend(color, dstColorPremul, blendMode);
}

void write_pls_blend(half4 color, uint2 paintData PLS_CONTEXT_DECL)
{
    if (color.a != .0)
    {
        half4 blendedColor = do_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
        PLS_STORE4F(framebuffer, blendedColor);
    }
    else
    {
        PLS_PRESERVE_VALUE(framebuffer);
    }
}
#endif // ENABLE_ADVANCED_BLEND

#ifdef @ENABLE_ADVANCED_BLEND
#define ATOMIC_PLS_MAIN PLS_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS PLS_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS EMIT_PLS
#else // !ENABLE_ADVANCED_BLEND
#define ATOMIC_PLS_MAIN PLS_FRAG_COLOR_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS EMIT_PLS_AND_FRAG_COLOR
#endif

#ifdef @DRAW_PATH
ATOMIC_PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_edgeDistance, half2);
    VARYING_UNPACK(v_pathID, ushort);

#ifndef @ENABLE_ADVANCED_BLEND
    _fragColor = make_half4(0, 0, 0, 0);
#endif

    half coverage = min(min(v_edgeDistance.x, abs(v_edgeDistance.y)), make_half(1));

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
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    if (lastPathID != v_pathID)
    {
        // We crossed into a new path! Resolve the previous path now that we know its exact
        // coverage.
        half coverageCount = from_fixed(lastCoverageData & 0xffffu);
        uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
        half4 color = resolve_path_color(coverageCount,
                                         paintData,
                                         lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
#ifdef @ENABLE_ADVANCED_BLEND
        write_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
#else
        _fragColor = premultiply(color);
#endif
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

#ifndef @ENABLE_ADVANCED_BLEND
    _fragColor = make_half4(0, 0, 0, 0);
#endif

    half coverage = v_windingWeight;

    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageCountBuffer);
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    half lastCoverageCount = from_fixed(lastCoverageData & 0xffffu);
    if (lastPathID != v_pathID)
    {
        // We crossed into a new path! Resolve the previous path now that we know its exact
        // coverage.
        uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
        half4 color = resolve_path_color(lastCoverageCount,
                                         paintData,
                                         lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
#ifdef @ENABLE_ADVANCED_BLEND
        write_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
#else
        _fragColor = premultiply(color);
#endif
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
    half4 imageColor = make_half4(TEXTURE_SAMPLE(@imageTexture, imageSampler, v_texCoord));
    half meshCoverage = 1.;
#ifdef @DRAW_IMAGE_RECT
    meshCoverage = min(v_edgeCoverage, meshCoverage);
#endif
#ifdef @ENABLE_CLIP_RECT
    half clipRectCoverage = min_value(make_half4(v_clipRect));
    meshCoverage = clamp(clipRectCoverage, make_half(0), meshCoverage);
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
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    uint2 lastPaintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
    half4 lastColor = resolve_path_color(coverageCount,
                                         lastPaintData,
                                         lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);

    // Clip the image after resolving the previous path, since that can affect the clip buffer.
#ifdef @ENABLE_CLIPPING // TODO! ENABLE_IMAGE_CLIPPING in addition to ENABLE_CLIPPING?
    if (imageDrawUniforms.clipID != 0u)
    {
        PLS_MEMORY_BARRIER(clipBuffer); // (Because resolve_path_color() may have modified it.)
        uint clipData = PLS_LOADUI(clipBuffer);
        uint clipID = clipData >> 16;
        half clipCoverage = clipID == imageDrawUniforms.clipID ? unpackHalf2x16(clipData).r : .0;
        meshCoverage = min(meshCoverage, clipCoverage);
    }
#endif // ENABLE_CLIPPING
    imageColor.a *= meshCoverage * make_half(imageDrawUniforms.opacity);

#ifdef @ENABLE_ADVANCED_BLEND
    if (lastColor.a != .0 || imageColor.a != .0)
    {
        // Blend the previous path and image both in a single operation.
        // TODO: Are advanced blend modes associative? srcOver is, so at least there we can blend
        // lastColor and imageColor first, and potentially avoid a framebuffer load if it ends up
        // opaque.
        half4 dstColorPremul = PLS_LOAD4F(framebuffer);
        ushort lastBlendMode = make_ushort((lastPaintData.x >> 4) & 0xfu);
        ushort imageBlendMode = make_ushort(imageDrawUniforms.blendMode);
        dstColorPremul = do_advanced_blend(lastColor, dstColorPremul, lastBlendMode);
        imageColor = do_advanced_blend(imageColor, dstColorPremul, imageBlendMode);
        PLS_STORE4F(framebuffer, imageColor);
    }
    else
    {
        PLS_PRESERVE_VALUE(framebuffer);
    }
#else
    // Leverage the property that premultiplied src-over blending is associative and blend the
    // imageColor and lastColor before passing them on to the blending pipeline.
    _fragColor = do_src_over_blend(premultiply(imageColor), premultiply(lastColor));
#endif

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
#ifndef @ENABLE_ADVANCED_BLEND
    _fragColor = make_half4(0, 0, 0, 0);
#endif
#ifdef @STORE_COLOR_CLEAR
    PLS_STORE4F(framebuffer, unpackUnorm4x8(uniforms.colorClearValue));
#endif
#ifdef @SWIZZLE_COLOR_BGRA_TO_RGBA
    half4 color = PLS_LOAD4F(framebuffer);
    PLS_STORE4F(framebuffer, color.bgra);
#endif
    PLS_STOREUI_ATOMIC(coverageCountBuffer, uniforms.coverageClearValue);
#ifdef @ENABLE_CLIPPING
    PLS_STOREUI(clipBuffer, 0u);
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
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, lastPathID);
    half4 color = resolve_path_color(coverageCount,
                                     paintData,
                                     lastPathID FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
#ifdef @COALESCED_PLS_RESOLVE_AND_TRANSFER
    _fragColor = do_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
    EMIT_PLS_AND_FRAG_COLOR
#else
#ifdef @ENABLE_ADVANCED_BLEND
    write_pls_blend(color, paintData PLS_CONTEXT_UNPACK);
#else
    _fragColor = premultiply(color);
#endif
    EMIT_ATOMIC_PLS
#endif
}
#endif // RESOLVE_PLS
#endif // FRAGMENT
