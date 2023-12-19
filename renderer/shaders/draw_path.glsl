/*
 * Copyright 2022 Rive
 */

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
#ifdef @DRAW_INTERIOR_TRIANGLES
ATTR(0, packed_float3, @a_triangleVertex);
#else
ATTR(0, float4, @a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, @a_mirroredVertexData);
#endif
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(0, float4, v_paint);
#ifdef @DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(1, half, v_windingWeight);
#else
NO_PERSPECTIVE VARYING(2, half2, v_edgeDistance);
#endif
@OPTIONALLY_FLAT VARYING(3, half, v_pathID);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(4, half, v_clipID);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(5, half, v_blendMode);
#endif
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(6, float4, v_clipRect);
#endif
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
TEXTURE_RGBA32UI(TESS_VERTEX_TEXTURE_IDX, @tessVertexTexture);
TEXTURE_RGBA32UI(PATH_TEXTURE_IDX, @pathTexture);
TEXTURE_RGBA32UI(CONTOUR_TEXTURE_IDX, @contourTexture);
VERTEX_TEXTURE_BLOCK_END

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
#ifdef @DRAW_INTERIOR_TRIANGLES
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);
#else
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);
#endif

    VARYING_INIT(varyings, v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(varyings, v_windingWeight, half);
#else
    VARYING_INIT(varyings, v_edgeDistance, half2);
#endif
    VARYING_INIT(varyings, v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_INIT(varyings, v_blendMode, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(varyings, v_clipRect, float4);
#endif

    bool shouldDiscardVertex = false;
    ushort pathIDBits;
    int2 pathTexelCoord;
    float2x2 M;
    float2 translate;
    uint pathParams;
    float2 vertexPosition;

#ifdef @DRAW_INTERIOR_TRIANGLES
    vertexPosition = unpack_interior_triangle_vertex(@a_triangleVertex,
                                                     TEXTURE_DEREF(textures, @pathTexture),
                                                     pathIDBits,
                                                     pathTexelCoord,
                                                     M,
                                                     translate,
                                                     pathParams,
                                                     v_windingWeight);
#else
    shouldDiscardVertex =
        !unpack_tessellated_path_vertex(@a_patchVertexData,
                                        @a_mirroredVertexData,
                                        _instanceID,
                                        TEXTURE_DEREF(textures, @tessVertexTexture),
                                        TEXTURE_DEREF(textures, @pathTexture),
                                        TEXTURE_DEREF(textures, @contourTexture),
                                        pathIDBits,
                                        pathTexelCoord,
                                        M,
                                        translate,
                                        pathParams,
                                        v_edgeDistance,
                                        vertexPosition);
#endif

    // Encode the integral pathID as a "half" that we know the hardware will see as a unique value
    // in the fragment shader.
    v_pathID = id_bits_to_f16(pathIDBits, uniforms.pathIDGranularity);

    // Indicate even-odd fill rule by making pathID negative.
    if ((pathParams & EVEN_ODD_PATH_FLAG) != 0u)
        v_pathID = -v_pathID;

    uint paintType = (pathParams >> 20) & 7u;
#ifdef @ENABLE_CLIPPING
    uint clipIDBits = (pathParams >> 4) & 0xffffu;
    v_clipID = id_bits_to_f16(clipIDBits, uniforms.pathIDGranularity);
    // Negative clipID means to update the clip buffer instead of the framebuffer.
    if (paintType == CLIP_UPDATE_PAINT_TYPE)
        v_clipID = -v_clipID;
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    v_blendMode = float(pathParams & 0xfu);
#endif

#ifdef @ENABLE_CLIP_RECT
    // clipRectInverseMatrix transforms from pixel coordinates to a space where the clipRect is the
    // normalized rectangle: [-1, -1, 1, 1].
    float2x2 clipRectInverseMatrix = make_float2x2(
        uintBitsToFloat(TEXEL_DEREF_FETCH(textures, @pathTexture, pathTexelCoord + int2(3, 0))));
    float2 clipRectInverseTranslate =
        uintBitsToFloat(TEXEL_DEREF_FETCH(textures, @pathTexture, pathTexelCoord + int2(4, 0)).xy);
    v_clipRect = find_clip_rect_coverage_distances(clipRectInverseMatrix,
                                                   clipRectInverseTranslate,
                                                   vertexPosition);
#endif

    // Unpack the paint once we have a position.
    uint4 paintData = TEXEL_DEREF_FETCH(textures, @pathTexture, pathTexelCoord + int2(2, 0));
    if (paintType == SOLID_COLOR_PAINT_TYPE)
    {
        float4 color = uintBitsToFloat(paintData);
        v_paint = color;
    }
#ifdef @ENABLE_CLIPPING
    else if (paintType == CLIP_UPDATE_PAINT_TYPE)
    {
        half outerClipID = id_bits_to_f16(paintData.r, uniforms.pathIDGranularity);
        v_paint = float4(outerClipID, 0, 0, 0);
    }
#endif
    else
    {
        float2 localCoord = MUL(inverse(M), vertexPosition - translate);
        if (paintType == LINEAR_GRADIENT_PAINT_TYPE || paintType == RADIAL_GRADIENT_PAINT_TYPE)
        {
            uint span = paintData.x;
            float row = float(span >> 20);
            float x1 = float((span >> 10) & 0x3ffu);
            float x0 = float(span & 0x3ffu);
            // v_paint.a contains "-row" of the gradient ramp at texel center, in normalized space.
            v_paint.a = (row + .5) * -uniforms.gradTextureInverseHeight;
            // abs(v_paint.b) contains either:
            //   - 2 if the gradient ramp spans an entire row.
            //   - x0 of the gradient ramp in normalized space, if it's a simple 2-texel ramp.
            if (x1 > x0 + 1.)
                v_paint.b = 2.; // This ramp spans an entire row. Set it to 2 to convey this.
            else
                v_paint.b = x0 * (1. / GRAD_TEXTURE_WIDTH) + (.5 / GRAD_TEXTURE_WIDTH);
            float3 gradCoeffs = uintBitsToFloat(paintData.yzw);
            if (paintType == LINEAR_GRADIENT_PAINT_TYPE)
            {
                // The paint is a linear gradient.
                v_paint.g = .0;
                v_paint.r = dot(localCoord, gradCoeffs.xy) + gradCoeffs.z;
            }
            else
            {
                // The paint is a radial gradient. Mark v_paint.b negative to indicate this to the
                // fragment shader. (v_paint.b can't be zero because the gradient ramp is aligned on
                // pixel centers, so negating it will always produce a negative number.)
                v_paint.b = -v_paint.b;
                v_paint.rg = (localCoord - gradCoeffs.xy) / gradCoeffs.z;
            }
        }
        else // IMAGE_PAINT_TYPE
        {
            // v_paint.a <= -1. signals that the paint is an image.
            // v_paint.b is the image opacity.
            // v_paint.rg is the normalized image texture coordinate. (For now, we just
            // pre-transform paths before rendering, such that localCoord == textureCoord.)
            float opacity = uintBitsToFloat(paintData.x);
            v_paint = float4(localCoord.x, localCoord.y, opacity, -2.);
        }
    }

    if (!shouldDiscardVertex)
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

    VARYING_PACK(varyings, v_paint);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_PACK(varyings, v_windingWeight);
#else
    VARYING_PACK(varyings, v_edgeDistance);
#endif
    VARYING_PACK(varyings, v_pathID);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(varyings, v_clipID);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_PACK(varyings, v_blendMode);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(varyings, v_clipRect);
#endif
    EMIT_VERTEX(varyings, _pos);
}
#endif

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

PLS_MAIN(@drawFragmentMain, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)
{
    VARYING_UNPACK(varyings, v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(varyings, v_windingWeight, half);
#else
    VARYING_UNPACK(varyings, v_edgeDistance, half2);
#endif
    VARYING_UNPACK(varyings, v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(varyings, v_blendMode, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(varyings, v_clipRect, float4);
#endif

#ifdef @TARGET_VULKAN
    // Strict validators require derivatives (i.e., for a mipmapped texture sample) to be computed
    // within uniform control flow.
    // Our control flow for texture sampling is uniform for an entire triangle, so we're fine, but
    // the validators don't know this.
    // If this might be a problem (e.g., for WebGPU), just find the potential image paint
    // derivatives here.
    float2 imagePaintDDX = dFdx(v_paint.rg);
    float2 imagePaintDDY = dFdy(v_paint.rg);
#endif

#ifndef @DRAW_INTERIOR_TRIANGLES
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_BEGIN;
#endif

    half2 coverageData = unpackHalf2x16(PLS_LOADUI(coverageCountBuffer, _plsCoord));
    half coverageBufferID = coverageData.g;
    half coverageCount = coverageBufferID == v_pathID ? coverageData.r : make_half(0);

#ifdef @DRAW_INTERIOR_TRIANGLES
    coverageCount += v_windingWeight;
#else
    if (v_edgeDistance.y >= .0) // Stroke.
        coverageCount = max(min(v_edgeDistance.x, v_edgeDistance.y), coverageCount);
    else // Fill. (Back-face culling ensures v_edgeDistance.x is appropriately signed.)
        coverageCount += v_edgeDistance.x;

    // Save the updated coverage.
    PLS_STOREUI(coverageCountBuffer, packHalf2x16(make_half2(coverageCount, v_pathID)), _plsCoord);
#endif

    // Convert coverageCount to coverage.
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if (v_pathID < .0 /*even-odd*/)
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
#endif
    coverage = min(coverage, make_half(1)); // This also caps stroke coverage, which can be >1.

#ifdef @ENABLE_CLIPPING
    if (v_clipID < .0) // Update the clip buffer.
    {
        half clipID = -v_clipID;
#ifdef @ENABLE_NESTED_CLIPPING
        half outerClipID = v_paint.r;
        if (outerClipID != .0)
        {
            // This is a nested clip. Intersect coverage with the enclosing clip (outerClipID).
            half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer, _plsCoord));
            half clipContentID = clipData.g;
            half outerClipCoverage;
            if (clipContentID != clipID)
            {
                // First hit: either clipBuffer contains outerClipCoverage, or this pixel is not
                // inside the outer clip and outerClipCoverage is zero.
                outerClipCoverage = clipContentID == outerClipID ? clipData.r : .0;
#ifndef @DRAW_INTERIOR_TRIANGLES
                // Stash outerClipCoverage before overwriting clipBuffer, in case we hit this pixel
                // again and need it. (Not necessary when drawing interior triangles because they
                // always go last and don't overlap.)
                PLS_STORE4F(originalDstColorBuffer,
                            make_half4(outerClipCoverage, 0, 0, 0),
                            _plsCoord);
#endif
            }
            else
            {
                // Subsequent hit: outerClipCoverage is stashed in originalDstColorBuffer.
                outerClipCoverage = PLS_LOAD4F(originalDstColorBuffer, _plsCoord).r;
#ifndef @DRAW_INTERIOR_TRIANGLES
                // Since interior triangles are always last, there's no need to preserve this value.
                PLS_PRESERVE_VALUE(originalDstColorBuffer, _plsCoord);
#endif
            }
            coverage = min(coverage, outerClipCoverage);
        }
#endif // @ENABLE_NESTED_CLIPPING
        PLS_STOREUI(clipBuffer, packHalf2x16(make_half2(coverage, clipID)), _plsCoord);
        PLS_PRESERVE_VALUE(framebuffer, _plsCoord);
    }
    else // Render to the main framebuffer.
#endif   // @ENABLE_CLIPPING
    {
#ifdef @ENABLE_CLIPPING
        // Apply the clip.
        if (v_clipID != .0)
        {
            // Clip IDs are not necessarily drawn in monotonically increasing order, so always check
            // exact equality of the clipID.
            half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer, _plsCoord));
            half clipContentID = clipData.g;
            half clipCoverage = clipContentID == v_clipID ? clipData.r : make_half(0);
            coverage = min(coverage, clipCoverage);
        }
#endif
#ifdef @ENABLE_CLIP_RECT
        half clipRectCoverage = min_value(make_half4(v_clipRect));
        coverage = clamp(clipRectCoverage, make_half(0), coverage);
#endif
        PLS_PRESERVE_VALUE(clipBuffer, _plsCoord);

        half4 color;
        if (v_paint.a >= .0) // Is the paint a solid color?
        {
            color = make_half4(v_paint);
        }
        else if (v_paint.a > -1.) // Is paint is a gradient (linear or radial)?
        {
            float t = v_paint.b > .0 ? /*linear*/ v_paint.r : /*radial*/ length(v_paint.rg);
            t = clamp(t, .0, 1.);
            float span = abs(v_paint.b);
            float x = span > 1. ? /*entire row*/ (1. - 1. / GRAD_TEXTURE_WIDTH) * t +
                                      (.5 / GRAD_TEXTURE_WIDTH)
                                : /*two texels*/ (1. / GRAD_TEXTURE_WIDTH) * t + span;
            float row = -v_paint.a;
            // Our gradient texture is not mipmapped. Issue a texture-sample that explicitly does
            // not find derivatives for LOD computation (by specifying derivatives directly).
            color = make_half4(TEXTURE_SAMPLE_LOD(TEXTURE_DEREF(textures, @gradTexture),
                                                  gradSampler,
                                                  float2(x, row),
                                                  .0));
        }
        else // The paint is an image.
        {
#ifdef @TARGET_VULKAN
            // Vulkan validators require explicit derivatives when sampling a texture in
            // "non-uniform" control flow. See above.
            color = TEXTURE_SAMPLE_GRAD(TEXTURE_DEREF(textures, @imageTexture),
                                        imageSampler,
                                        v_paint.rg,
                                        imagePaintDDX,
                                        imagePaintDDY);
#else
            color = TEXTURE_DEREF_SAMPLE(textures, @imageTexture, imageSampler, v_paint.rg);
#endif
            color.a *= v_paint.b; // paint.b holds the opacity of the image.
        }
        color.a *= coverage;

        half4 dstColor;
        if (coverageBufferID != v_pathID)
        {
            // This is the first fragment from pathID to touch this pixel.
            dstColor = PLS_LOAD4F(framebuffer, _plsCoord);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // We don't need to store coverage when drawing interior triangles because they always
            // go last and don't overlap, so every fragment is the final one in the path.
            PLS_STORE4F(originalDstColorBuffer, dstColor, _plsCoord);
#endif
        }
        else
        {
            dstColor = PLS_LOAD4F(originalDstColorBuffer, _plsCoord);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // Since interior triangles are always last, there's no need to preserve this value.
            PLS_PRESERVE_VALUE(originalDstColorBuffer, _plsCoord);
#endif
        }

        // Blend with the framebuffer color.
#ifdef @ENABLE_ADVANCED_BLEND
        if (v_blendMode != make_half(BLEND_SRC_OVER))
        {
#ifdef @ENABLE_HSL_BLEND_MODES
            color = advanced_hsl_blend(
#else
            color = advanced_blend(
#endif
                color,
                unmultiply(dstColor),
                make_ushort(v_blendMode));
        }
        else
#endif
        {
#ifndef @PLS_IMPL_NONE // Only attempt to blend if we can read the framebuffer.
            color.rgb *= color.a;
            color = color + dstColor * (1. - color.a);
#endif
        }

        PLS_STORE4F(framebuffer, color, _plsCoord);
    }

#ifndef @DRAW_INTERIOR_TRIANGLES
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_END;
#endif

    EMIT_PLS;
}
#endif
