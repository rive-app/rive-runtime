/*
 * Copyright 2023 Rive
 */

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
TEXTURE_RGBA32UI(TESS_VERTEX_TEXTURE_IDX, @tessVertexTexture);
TEXTURE_RGBA32UI(PATH_TEXTURE_IDX, @pathTexture);
TEXTURE_RGBA32UI(CONTOUR_TEXTURE_IDX, @contourTexture);
VERTEX_TEXTURE_BLOCK_END
#endif

#ifdef @DRAW_PATH
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, float4, @a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, @a_mirroredVertexData);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(0, half2, v_edgeDistance);
@OPTIONALLY_FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain,
            @Uniforms,
            uniforms,
            Attrs,
            attrs,
            Varyings,
            varyings,
            VertexTextures,
            textures,
            _vertexID,
            _instanceID,
            _pos)
{
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);

    VARYING_INIT(varyings, v_edgeDistance, half2);
    VARYING_INIT(varyings, v_pathID, ushort);

    int2 pathTexelCoord;
    float2x2 M;
    float2 translate;
    uint pathParams;
    float2 vertexPosition;
    if (unpack_tessellated_path_vertex(@a_patchVertexData,
                                       @a_mirroredVertexData,
                                       _instanceID,
                                       TEXTURE_DEREF(textures, @tessVertexTexture),
                                       TEXTURE_DEREF(textures, @pathTexture),
                                       TEXTURE_DEREF(textures, @contourTexture),
                                       v_pathID,
                                       pathTexelCoord,
                                       M,
                                       translate,
                                       pathParams,
                                       v_edgeDistance,
                                       vertexPosition))
    {
        _pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
    }
    else
    {
        _pos = float4(uniforms.vertexDiscardValue,
                      uniforms.vertexDiscardValue,
                      uniforms.vertexDiscardValue,
                      uniforms.vertexDiscardValue);
    }

    VARYING_PACK(varyings, v_edgeDistance);
    VARYING_PACK(varyings, v_pathID);
    EMIT_VERTEX(varyings, _pos);
}
#endif // VERTEX
#endif // DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, packed_float3, @a_triangleVertex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
@OPTIONALLY_FLAT VARYING(0, half, v_windingWeight);
@OPTIONALLY_FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain,
            @Uniforms,
            uniforms,
            Attrs,
            attrs,
            Varyings,
            varyings,
            VertexTextures,
            textures,
            _vertexID,
            _instanceID,
            _pos)
{
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);

    VARYING_INIT(varyings, v_windingWeight, half);
    VARYING_INIT(varyings, v_pathID, ushort);

    int2 pathTexelCoord;
    float2x2 M;
    float2 translate;
    uint pathParams;
    float2 vertexPosition = unpack_interior_triangle_vertex(@a_triangleVertex,
                                                            TEXTURE_DEREF(textures, @pathTexture),
                                                            v_pathID,
                                                            pathTexelCoord,
                                                            M,
                                                            translate,
                                                            pathParams,
                                                            v_windingWeight);
    _pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(varyings, v_windingWeight);
    VARYING_PACK(varyings, v_pathID);
    EMIT_VERTEX(varyings, _pos);
}
#endif // VERTEX
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef @DRAW_IMAGE
UNIFORM_BLOCK_BEGIN(IMAGE_DRAW_UNIFORM_BUFFER_IDX, @ImageDrawUniforms)
float4 viewMatrix;
float2 translate;
float opacity;
float padding;
// clipRectInverseMatrix transforms from pixel coordinates to a space where the clipRect is the
// normalized rectangle: [-1, -1, 1, 1].
float4 clipRectInverseMatrix;
float2 clipRectInverseTranslate;
uint clipID;
uint blendMode;
UNIFORM_BLOCK_END(imageDrawUniforms)

#ifdef @DRAW_IMAGE_RECT
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, float4, @a_imageRectVertex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
NO_PERSPECTIVE VARYING(1, half, v_edgeCoverage);
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain,
            @Uniforms,
            uniforms,
            Attrs,
            attrs,
            Varyings,
            varyings,
            VertexTextures,
            textures,
            _vertexID,
            _instanceID,
            _pos)
{
    ATTR_UNPACK(_vertexID, attrs, @a_imageRectVertex, float4);

    VARYING_INIT(varyings, v_texCoord, float2);
    VARYING_INIT(varyings, v_edgeCoverage, half);
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(varyings, v_clipRect, float4);
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
            v_edgeCoverage *= .5 / aaRadiusX;
        }
        else
        {
            vertexPosition.x += aaRadiusX * @a_imageRectVertex.z;
        }
        float aaRadiusY = AA_RADIUS * manhattan_width(MIT[0]) / dot(M[0], MIT[0]);
        if (aaRadiusY >= .5)
        {
            vertexPosition.y = .5;
            v_edgeCoverage *= .5 / aaRadiusY;
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

    _pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(varyings, v_texCoord);
    VARYING_PACK(varyings, v_edgeCoverage);
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(varyings, v_clipRect);
#endif
    EMIT_VERTEX(varyings, _pos);
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

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(1, float4, v_clipRect);
#endif
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
IMAGE_MESH_VERTEX_MAIN(@drawVertexMain,
                       @Uniforms,
                       uniforms,
                       @ImageDrawUniforms,
                       imageDrawUniforms,
                       PositionAttr,
                       position,
                       UVAttr,
                       uv,
                       Varyings,
                       varyings,
                       _vertexID,
                       _pos)
{
    ATTR_UNPACK(_vertexID, position, @a_position, float2);
    ATTR_UNPACK(_vertexID, uv, @a_texCoord, float2);

    VARYING_INIT(varyings, v_texCoord, float2);
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(varyings, v_clipRect, float4);
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

    _pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(varyings, v_texCoord);
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(varyings, v_clipRect);
#endif
    EMIT_VERTEX(varyings, _pos);
}
#endif // VERTEX
#endif // DRAW_IMAGE_MESH
#endif // DRAW_IMAGE

#ifdef @RESOLVE_PLS
#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR_BLOCK_END
#endif // VERTEX

VARYING_BLOCK_BEGIN(Varyings)
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain,
            @Uniforms,
            uniforms,
            Attrs,
            attrs,
            Varyings,
            varyings,
            VertexTextures,
            textures,
            _vertexID,
            _instanceID,
            _pos)
{
    _pos.x = (_vertexID & 1) == 0 ? -1. : 1.;
    _pos.y = (_vertexID & 2) == 0 ? 1. : -1.;
    _pos.zw = float2(0, 1);
    EMIT_VERTEX(varyings, _pos);
}
#endif // VERTEX
#endif // RESOLVE_PLS

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN(FragmentTextures)
TEXTURE_RGBA8(GRAD_TEXTURE_IDX, @gradTexture);
TEXTURE_RGBA8(IMAGE_TEXTURE_IDX, @imageTexture);
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)

PLS_BLOCK_BEGIN
PLS_DECL4F(FRAMEBUFFER_PLANE_IDX, framebuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_DECL4F(ORIGINAL_DST_COLOR_PLANE_IDX, originalDstColorBuffer);
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
PLS_BLOCK_END

STORAGE_BUFFER(PAINT_STORAGE_BUFFER_IDX, PaintDataBuffer, uint2, paintDataBuffer);
STORAGE_BUFFER(PAINT_MATRIX_STORAGE_BUFFER_IDX, PaintMatrixBuffer, float4, paintMatrixBuffer);
STORAGE_BUFFER(PAINT_TRANSLATE_STORAGE_BUFFER_IDX,
               PaintTranslateBuffer,
               float4,
               paintTranslateBuffer);

#ifdef @ENABLE_CLIP_RECT
STORAGE_BUFFER(CLIPRECT_MATRIX_STORAGE_BUFFER_IDX,
               ClipRectMatrixBuffer,
               float4,
               clipRectMatrixBuffer);
STORAGE_BUFFER(CLIPRECT_TRANSLATE_STORAGE_BUFFER_IDX,
               ClipRectTranslateBuffer,
               float4,
               clipRectTranslateBuffer);
#endif // ENABLE_CLIP_RECT

uint to_fixed(float x) { return uint(x * FIXED_COVERAGE_FACTOR + FIXED_COVERAGE_ZERO); }

float from_fixed(uint x)
{
    return float(x) * FIXED_COVERAGE_INVERSE_FACTOR +
           (-FIXED_COVERAGE_ZERO * FIXED_COVERAGE_INVERSE_FACTOR);
}

// Return the color at location '_pos' of the path at index 'pathID' in the storage buffers. Also
// update the PLS clip value if called for.
half4 resolve_path_color(half coverageCount,
                         uint2 paintData,
                         ushort pathID,
                         float2 _pos,
                         int2 _plsCoord)
{
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if ((paintData.x & ATOMIC_MODE_FLAG_EVEN_ODD) != 0u)
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
#endif                                      // ENABLE_EVEN_ODD
    coverage = min(coverage, make_half(1)); // This also caps stroke coverage, which can be >1.
#ifdef @ENABLE_CLIPPING
    uint clipID = paintData.x >> 16u;
    if (clipID != 0u)
    {
        uint clipData = PLS_LOADUI(clipBuffer, _plsCoord);
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
            break;
        case LINEAR_GRADIENT_PAINT_TYPE:
        case RADIAL_GRADIENT_PAINT_TYPE:
#ifdef @ENABLE_BINDLESS_TEXTURES
        case IMAGE_PAINT_TYPE:
#endif // ENABLE_BINDLESS_TEXTURES
        {
            float2x2 M = make_float2x2(STORAGE_BUFFER_AT(paintMatrixBuffer, pathID));
            float4 translate = STORAGE_BUFFER_AT(paintTranslateBuffer, pathID);
            float2 paintCoord = MUL(M, _pos) + translate.xy;
#ifdef @ENABLE_BINDLESS_TEXTURES
            if (paintType == IMAGE_PAINT_TYPE)
            {
                color = TEXTURE_SAMPLE_GRAD(sampler2D(floatBitsToUint(translate.zw)),
                                            imageSampler,
                                            paintCoord,
                                            M[0],
                                            M[1]);
            }
            else
#endif // ENABLE_BINDLESS_TEXTURES
            {
                float t = paintType == 1 ? /*linear*/ paintCoord.x : /*radial*/ length(paintCoord);
                t = clamp(t, .0, 1.);
                float x = t * translate.z + translate.w;
                float y = uintBitsToFloat(paintData.y);
                color = make_half4(TEXTURE_SAMPLE_LOD(TEXTURE_DEREF(textures, @gradTexture),
                                                      gradSampler,
                                                      float2(x, y),
                                                      .0));
            }
            break;
        }
#ifdef @ENABLE_CLIPPING
        case CLIP_UPDATE_PAINT_TYPE:
            PLS_STOREUI(clipBuffer, paintData.y | packHalf2x16(make_half2(coverage, 0)), _plsCoord);
            break;
#endif // ENABLE_CLIPPING
    }
#ifdef @ENABLE_CLIP_RECT
    if ((paintData.x & ATOMIC_MODE_FLAG_HAS_CLIP_RECT) != 0u)
    {
        float2x2 M = make_float2x2(STORAGE_BUFFER_AT(clipRectMatrixBuffer, pathID));
        float4 translate = STORAGE_BUFFER_AT(clipRectTranslateBuffer, pathID);
        half2 clipCoord = MUL(M, _pos) + translate.xy;
        // translate.zw contains -1 / fwidth(clipCoord), which we use to calculate antialiasing.
        half2 distXY = abs(clipCoord) * translate.zw - translate.zw;
        half clipRectCoverage = clamp(min(distXY.x, distXY.y) + .5, .0, 1.);
        coverage = min(coverage, clipRectCoverage);
    }
#endif // ENABLE_CLIP_RECT
    color.a *= coverage;
    return color;
}

half4 do_src_over_blend(half4 srcColorUnmul, half4 dstColorPremul)
{
    half4 dstFactor = dstColorPremul * (1. - srcColorUnmul.a);
    return make_half4(srcColorUnmul.rgb * srcColorUnmul.a + dstFactor.rgb,
                      srcColorUnmul.a + dstFactor.a);
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
        return do_src_over_blend(srcColorUnmul, dstColorPremul);
    }
}
#endif // ENABLE_ADVANCED_BLEND

void do_pls_blend(half4 color, uint2 paintData, int2 _plsCoord)
{
    if (color.a != .0)
    {
        half4 dstColorPremul = PLS_LOAD4F(framebuffer, _plsCoord);
#ifdef @ENABLE_ADVANCED_BLEND
        ushort blendMode = make_ushort((paintData.x >> 4) & 0xfu);
        color = do_advanced_blend(color, dstColorPremul, blendMode);
#else
        color = do_src_over_blend(color, dstColorPremul);
#endif
        PLS_STORE4F(framebuffer, color, _plsCoord);
    }
}

#ifdef @DRAW_PATH
PLS_MAIN(@drawFragmentMain, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)
{
    VARYING_UNPACK(varyings, v_edgeDistance, half2);
    VARYING_UNPACK(varyings, v_pathID, ushort);

    half coverage = min(min(v_edgeDistance.x, abs(v_edgeDistance.y)), 1.);

    // Since v_pathID increases monotonically with every draw, and since it lives in the most
    // significant bits of the coverage data, an atomic max() function will serve 3 purposes:
    //
    //    * The invocation that changes the pathID is the single first fragment invocation to
    //      hit the new path, and the one that should resolve the previous path in the framebuffer.
    //    * Properly resets coverage to zero when we do cross over into processing a new path.
    //    * Accumulates coverage for strokes.
    //
    uint fixedCoverage = to_fixed(coverage);
    uint minCoverageData = (uint(v_pathID) << 16) | fixedCoverage;
    uint lastCoverageData = PLS_ATOMIC_MAX(coverageCountBuffer, minCoverageData, _plsCoord);
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    if (lastPathID != v_pathID)
    {
        // We crossed into a new path! Resolve the previous path now that we know its exact
        // coverage.
        half coverageCount = from_fixed(lastCoverageData & 0xffffu);
        uint2 paintData = STORAGE_BUFFER_AT(paintDataBuffer, lastPathID);
        half4 color = resolve_path_color(coverageCount, paintData, lastPathID, _pos, _plsCoord);
        do_pls_blend(color, paintData, _plsCoord);
    }
    else if (v_edgeDistance.y < .0 /*fill?*/)
    {
        // We're a fill, and we did not cross into the new path this time. Count coverage.
        if (lastCoverageData < minCoverageData)
        {
            // We already crossed into this path. Oops. Undo the effect of the min().
            fixedCoverage += lastCoverageData - minCoverageData;
        }
        fixedCoverage -= uint(FIXED_COVERAGE_ZERO); // Only apply the zero bias once.
        PLS_ATOMIC_ADD(coverageCountBuffer, fixedCoverage, _plsCoord);
    }

    EMIT_PLS;
}
#endif // DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
PLS_MAIN(@drawFragmentMain, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)
{
    VARYING_UNPACK(varyings, v_windingWeight, half);
    VARYING_UNPACK(varyings, v_pathID, ushort);

    half coverage = v_windingWeight;

    uint lastCoverageData = PLS_LOADUI(coverageCountBuffer, _plsCoord);
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    half lastCoverageCount = from_fixed(lastCoverageData & 0xffffu);
    if (lastPathID != v_pathID)
    {
        // We crossed into a new path! Resolve the previous path now that we know its exact
        // coverage.
        uint2 paintData = STORAGE_BUFFER_AT(paintDataBuffer, lastPathID);
        half4 color = resolve_path_color(lastCoverageCount, paintData, lastPathID, _pos, _plsCoord);
        do_pls_blend(color, paintData, _plsCoord);
    }
    else
    {
        coverage += lastCoverageCount;
    }

    PLS_STOREUI(coverageCountBuffer, (uint(v_pathID) << 16) | to_fixed(coverage), _plsCoord);

    EMIT_PLS;
}
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef @DRAW_IMAGE
IMAGE_DRAW_PLS_MAIN(@drawFragmentMain,
                    @ImageDrawUniforms,
                    imageDrawUniforms,
                    Varyings,
                    varyings,
                    FragmentTextures,
                    textures,
                    _pos,
                    _plsCoord)
{
    VARYING_UNPACK(varyings, v_texCoord, float2);
#ifdef @DRAW_IMAGE_RECT
    VARYING_UNPACK(varyings, v_edgeCoverage, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(varyings, v_clipRect, float4);
#endif

    // Start by finding the image color. We have to do this immediately instead of allowing it to
    // get resolved later like other draws because the @imageTexture binding is liable to change,
    // and furthermore in the case of imageMeshes, we can't calculate UV coordinates based on
    // fragment position.
    half4 meshColor = TEXTURE_DEREF_SAMPLE(textures, @imageTexture, imageSampler, v_texCoord);
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
    // TODO: skip this step if no clipping AND srcOver AND meshColor is solid.
    uint lastCoverageData = PLS_LOADUI(coverageCountBuffer, _plsCoord);
    half coverageCount = from_fixed(lastCoverageData & 0xffffu);
    ushort lastPathID = make_ushort(lastCoverageData >> 16);
    uint2 lastPaintData = STORAGE_BUFFER_AT(paintDataBuffer, lastPathID);
    half4 lastColor = resolve_path_color(coverageCount, lastPaintData, lastPathID, _pos, _plsCoord);

    // Clip the image after resolving the previous path, since that can affect the clip buffer.
#ifdef @ENABLE_CLIPPING // TODO! ENABLE_IMAGE_CLIPPING in addition to ENABLE_CLIPPING?
    if (imageDrawUniforms.clipID != 0u)
    {
        uint clipData = PLS_LOADUI(clipBuffer, _plsCoord);
        uint clipID = clipData >> 16;
        half clipCoverage = clipID == imageDrawUniforms.clipID ? unpackHalf2x16(clipData).r : 0;
        meshCoverage = min(meshCoverage, clipCoverage);
    }
#endif // ENABLE_CLIPPING
    meshColor.a *= meshCoverage * imageDrawUniforms.opacity;

    if (lastColor.a != .0 || meshColor.a != 0)
    {
        // Blend the previous path and image both in a single operation.
        // TODO: Are advanced blend modes associative? srcOver is, so at least there we can blend
        // lastColor and meshColor first, and potentially avoid a framebuffer load if it ends up
        // opaque.
        half4 dstColorPremul = PLS_LOAD4F(framebuffer, _plsCoord);
#ifdef @ENABLE_ADVANCED_BLEND
        ushort lastBlendMode = make_ushort((lastPaintData.x >> 4) & 0xfu);
        ushort imageBlendMode = make_ushort(imageDrawUniforms.blendMode);
        dstColorPremul = do_advanced_blend(lastColor, dstColorPremul, lastBlendMode);
        meshColor = do_advanced_blend(meshColor, dstColorPremul, imageBlendMode);
#else
        dstColorPremul = do_src_over_blend(lastColor, dstColorPremul);
        meshColor = do_src_over_blend(meshColor, dstColorPremul);
#endif
        PLS_STORE4F(framebuffer, meshColor, _plsCoord);
    }

    // Write out a coverage value of "zero at pathID=0" so a future resolve attempt doesn't affect
    // this pixel.
    PLS_STOREUI(coverageCountBuffer, uint(FIXED_COVERAGE_ZERO), _plsCoord);

#ifdef @DRAW_IMAGE_MESH
    // TODO: If we care: Use the interlock if we can, since individual meshes may shimmer if they
    // have overlapping triangles.
    PLS_INTERLOCK_END;
#endif

    EMIT_PLS;
}
#endif // DRAW_IMAGE

#ifdef @RESOLVE_PLS
PLS_MAIN(@drawFragmentMain, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)
{
    uint lastCoverageData = PLS_LOADUI(coverageCountBuffer, _plsCoord);
    half coverageCount = from_fixed(lastCoverageData & 0xffffu);
    ushort pathID = make_ushort(lastCoverageData >> 16);
    uint2 paintData = STORAGE_BUFFER_AT(paintDataBuffer, pathID);
    half4 color = resolve_path_color(coverageCount, paintData, pathID, _pos, _plsCoord);
    do_pls_blend(color, paintData, _plsCoord);
    EMIT_PLS;
}
#endif // RESOLVE_PLS
#endif // FRAGMENT
