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

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_paint);
#ifndef @USING_DEPTH_STENCIL
#ifdef @DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(1, half, v_windingWeight);
#else
NO_PERSPECTIVE VARYING(2, half2, v_edgeDistance);
#endif
@OPTIONALLY_FLAT VARYING(3, half, v_pathID);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(4, half, v_clipID);
#endif
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(5, float4, v_clipRect);
#endif
#endif // !USING_DEPTH_STENCIL
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(6, half, v_blendMode);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
#ifdef @DRAW_INTERIOR_TRIANGLES
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);
#else
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);
#endif

    VARYING_INIT(v_paint, float4);
#ifndef USING_DEPTH_STENCIL
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_windingWeight, half);
#else
    VARYING_INIT(v_edgeDistance, half2);
#endif
    VARYING_INIT(v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif
#endif // !USING_DEPTH_STENCIL
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_INIT(v_blendMode, half);
#endif

    bool shouldDiscardVertex = false;
    ushort pathID;
    float2 vertexPosition;
#ifdef @USING_DEPTH_STENCIL
    ushort pathZIndex;
#endif

#ifdef @DRAW_INTERIOR_TRIANGLES
    vertexPosition = unpack_interior_triangle_vertex(@a_triangleVertex,
                                                     pathID,
                                                     v_windingWeight VERTEX_CONTEXT_UNPACK);
#else
    shouldDiscardVertex = !unpack_tessellated_path_vertex(@a_patchVertexData,
                                                          @a_mirroredVertexData,
                                                          _instanceID,
                                                          pathID,
                                                          vertexPosition
#ifndef @USING_DEPTH_STENCIL
                                                          ,
                                                          v_edgeDistance
#else
                                                          ,
                                                          pathZIndex
#endif
                                                              VERTEX_CONTEXT_UNPACK);
#endif // !DRAW_INTERIOR_TRIANGLES

    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, pathID);

#ifndef @USING_DEPTH_STENCIL
    // Encode the integral pathID as a "half" that we know the hardware will see as a unique value
    // in the fragment shader.
    v_pathID = id_bits_to_f16(pathID, uniforms.pathIDGranularity);

    // Indicate even-odd fill rule by making pathID negative.
    if ((paintData.x & PAINT_FLAG_EVEN_ODD) != 0u)
        v_pathID = -v_pathID;
#endif // !USING_DEPTH_STENCIL

    uint paintType = paintData.x & 0xfu;
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        uint clipIDBits = (paintType == CLIP_UPDATE_PAINT_TYPE ? paintData.y : paintData.x) >> 16;
        v_clipID = id_bits_to_f16(clipIDBits, uniforms.pathIDGranularity);
        // Negative clipID means to update the clip buffer instead of the color buffer.
        if (paintType == CLIP_UPDATE_PAINT_TYPE)
            v_clipID = -v_clipID;
    }
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
        v_blendMode = float((paintData.x >> 4) & 0xfu);
    }
#endif

    // Paint matrices operate on the fragment shader's "_fragCoord", which is bottom-up in GL.
    float2 fragCoord = vertexPosition;
#ifdef FRAG_COORD_BOTTOM_UP
    fragCoord.y = float(uniforms.renderTargetHeight) - fragCoord.y;
#endif

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        // clipRectInverseMatrix transforms from pixel coordinates to a space where the clipRect is
        // the normalized rectangle: [-1, -1, 1, 1].
        float2x2 clipRectInverseMatrix =
            make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 2u));
        float4 clipRectInverseTranslate = STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 3u);
#ifndef @USING_DEPTH_STENCIL
        v_clipRect = find_clip_rect_coverage_distances(clipRectInverseMatrix,
                                                       clipRectInverseTranslate.xy,
                                                       fragCoord);
#else  // USING_DEPTH_STENCIL
        set_clip_rect_plane_distances(clipRectInverseMatrix,
                                      clipRectInverseTranslate.xy,
                                      fragCoord);
#endif // USING_DEPTH_STENCIL
    }
#endif // ENABLE_CLIP_RECT

    // Unpack the paint once we have a position.
    if (paintType == SOLID_COLOR_PAINT_TYPE)
    {
        half4 color = unpackUnorm4x8(paintData.y);
        v_paint = float4(color);
    }
#ifdef @ENABLE_CLIPPING
    else if (@ENABLE_CLIPPING && paintType == CLIP_UPDATE_PAINT_TYPE)
    {
        half outerClipID = id_bits_to_f16(paintData.x >> 16, uniforms.pathIDGranularity);
        v_paint = float4(outerClipID, 0, 0, 0);
    }
#endif
    else
    {
        float2x2 paintMatrix = make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u));
        float4 paintTranslate = STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 1u);
        float2 paintCoord = MUL(paintMatrix, fragCoord) + paintTranslate.xy;
        if (paintType == LINEAR_GRADIENT_PAINT_TYPE || paintType == RADIAL_GRADIENT_PAINT_TYPE)
        {
            // v_paint.a contains "-row" of the gradient ramp at texel center, in normalized space.
            v_paint.a = -uintBitsToFloat(paintData.y);
            // abs(v_paint.b) contains either:
            //   - 2 if the gradient ramp spans an entire row.
            //   - x0 of the gradient ramp in normalized space, if it's a simple 2-texel ramp.
            if (paintTranslate.z > .9) // paintTranslate.z is either ~1 or ~1/GRAD_TEXTURE_WIDTH.
            {
                // Complex ramps span an entire row. Set it to 2 to convey this.
                v_paint.b = 2.;
            }
            else
            {
                // This is a simple ramp.
                v_paint.b = paintTranslate.w;
            }
            if (paintType == LINEAR_GRADIENT_PAINT_TYPE)
            {
                // The paint is a linear gradient.
                v_paint.g = .0;
                v_paint.r = paintCoord.x;
            }
            else
            {
                // The paint is a radial gradient. Mark v_paint.b negative to indicate this to the
                // fragment shader. (v_paint.b can't be zero because the gradient ramp is aligned on
                // pixel centers, so negating it will always produce a negative number.)
                v_paint.b = -v_paint.b;
                v_paint.rg = paintCoord.xy;
            }
        }
        else // IMAGE_PAINT_TYPE
        {
            // v_paint.a <= -1. signals that the paint is an image.
            // v_paint.b is the image opacity.
            // v_paint.rg is the normalized image texture coordinate (built into the paintMatrix).
            float opacity = uintBitsToFloat(paintData.y);
            v_paint = float4(paintCoord.x, paintCoord.y, opacity, -2.);
        }
    }

    float4 pos;
    if (!shouldDiscardVertex)
    {
        pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
#ifdef @USING_DEPTH_STENCIL
        pos.z = normalize_z_index(pathZIndex);
#endif
    }
    else
    {
        pos = float4(uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue);
    }

    VARYING_PACK(v_paint);
#ifndef @USING_DEPTH_STENCIL
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_PACK(v_windingWeight);
#else
    VARYING_PACK(v_edgeDistance);
#endif
    VARYING_PACK(v_pathID);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(v_clipID);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
#endif // !USING_DEPTH_STENCIL
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_PACK(v_blendMode);
#endif
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, GRAD_TEXTURE_IDX, @gradTexture);
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
#ifdef @USING_DEPTH_STENCIL
#ifdef @ENABLE_ADVANCED_BLEND
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, DST_COLOR_TEXTURE_IDX, @dstColorTexture);
#endif
#endif
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

INLINE half4 find_paint_color(float4 paint
#ifdef @TARGET_VULKAN
                              ,
                              float2 imagePaintDDX,
                              float2 imagePaintDDY
#endif
                                  FRAGMENT_CONTEXT_DECL)
{
    if (paint.a >= .0) // Is the paint a solid color?
    {
        return cast_float4_to_half4(paint);
    }
    else if (paint.a > -1.) // Is paint is a gradient (linear or radial)?
    {
        float t = paint.b > .0 ? /*linear*/ paint.r : /*radial*/ length(paint.rg);
        t = clamp(t, .0, 1.);
        float span = abs(paint.b);
        float x = span > 1. ? /*entire row*/ (1. - 1. / GRAD_TEXTURE_WIDTH) * t +
                                  (.5 / GRAD_TEXTURE_WIDTH)
                            : /*two texels*/ (1. / GRAD_TEXTURE_WIDTH) * t + span;
        float row = -paint.a;
        // Our gradient texture is not mipmapped. Issue a texture-sample that explicitly does not
        // find derivatives for LOD computation (by specifying derivatives directly).
        return TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, float2(x, row), .0);
    }
    else // The paint is an image.
    {
        half4 color;
#ifdef @TARGET_VULKAN
        // Vulkan validators require explicit derivatives when sampling a texture in
        // "non-uniform" control flow. See above.
        color = TEXTURE_SAMPLE_GRAD(@imageTexture,
                                    imageSampler,
                                    paint.rg,
                                    imagePaintDDX,
                                    imagePaintDDY);
#else
        color = TEXTURE_SAMPLE(@imageTexture, imageSampler, paint.rg);
#endif
        color.a *= paint.b; // paint.b holds the opacity of the image.
        return color;
    }
}

#ifndef @USING_DEPTH_STENCIL

PLS_BLOCK_BEGIN
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#if defined(@ENABLE_CLIPPING) || defined(@PLS_IMPL_ANGLE)
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#endif
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_BLOCK_END

PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_windingWeight, half);
#else
    VARYING_UNPACK(v_edgeDistance, half2);
#endif
    VARYING_UNPACK(v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(v_blendMode, half);
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

    half2 coverageData = unpackHalf2x16(PLS_LOADUI(coverageCountBuffer));
    half coverageBufferID = coverageData.g;
    half coverageCount = coverageBufferID == v_pathID ? coverageData.r : make_half(.0);

#ifdef @DRAW_INTERIOR_TRIANGLES
    coverageCount += v_windingWeight;
#else
    if (v_edgeDistance.y >= .0) // Stroke.
        coverageCount = max(min(v_edgeDistance.x, v_edgeDistance.y), coverageCount);
    else // Fill. (Back-face culling ensures v_edgeDistance.x is appropriately signed.)
        coverageCount += v_edgeDistance.x;

    // Save the updated coverage.
    PLS_STOREUI(coverageCountBuffer, packHalf2x16(make_half2(coverageCount, v_pathID)));
#endif

    // Convert coverageCount to coverage.
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if (@ENABLE_EVEN_ODD && v_pathID < .0 /*even-odd*/)
    {
        coverage = 1. - make_half(abs(fract(coverage * .5) * 2. + -1.));
    }
#endif
    coverage = min(coverage, make_half(1.)); // This also caps stroke coverage, which can be >1.

#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING && v_clipID < .0) // Update the clip buffer.
    {
        half clipID = -v_clipID;
#ifdef @ENABLE_NESTED_CLIPPING
        if (@ENABLE_NESTED_CLIPPING)
        {
            half outerClipID = v_paint.r;
            if (outerClipID != .0)
            {
                // This is a nested clip. Intersect coverage with the enclosing clip (outerClipID).
                half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
                half clipContentID = clipData.g;
                half outerClipCoverage;
                if (clipContentID != clipID)
                {
                    // First hit: either clipBuffer contains outerClipCoverage, or this pixel is not
                    // inside the outer clip and outerClipCoverage is zero.
                    outerClipCoverage = clipContentID == outerClipID ? clipData.r : .0;
#ifndef @DRAW_INTERIOR_TRIANGLES
                    // Stash outerClipCoverage before overwriting clipBuffer, in case we hit this
                    // pixel again and need it. (Not necessary when drawing interior triangles
                    // because they always go last and don't overlap.)
                    PLS_STORE4F(scratchColorBuffer, make_half4(outerClipCoverage, .0, .0, .0));
#endif
                }
                else
                {
                    // Subsequent hit: outerClipCoverage is stashed in scratchColorBuffer.
                    outerClipCoverage = PLS_LOAD4F(scratchColorBuffer).r;
#ifndef @DRAW_INTERIOR_TRIANGLES
                    // Since interior triangles are always last, there's no need to preserve this
                    // value.
                    PLS_PRESERVE_4F(scratchColorBuffer);
#endif
                }
                coverage = min(coverage, outerClipCoverage);
            }
        }
#endif // @ENABLE_NESTED_CLIPPING
        PLS_STOREUI(clipBuffer, packHalf2x16(make_half2(coverage, clipID)));
        PLS_PRESERVE_4F(colorBuffer);
    }
    else // Render to the main framebuffer.
#endif   // @ENABLE_CLIPPING
    {
#ifdef @ENABLE_CLIPPING
        if (@ENABLE_CLIPPING)
        {
            // Apply the clip.
            if (v_clipID != .0)
            {
                // Clip IDs are not necessarily drawn in monotonically increasing order, so always
                // check exact equality of the clipID.
                half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
                half clipContentID = clipData.g;
                half clipCoverage = clipContentID == v_clipID ? clipData.r : make_half(.0);
                coverage = min(coverage, clipCoverage);
            }
            PLS_PRESERVE_UI(clipBuffer);
        }
#endif
#ifdef @ENABLE_CLIP_RECT
        if (@ENABLE_CLIP_RECT)
        {
            half clipRectCoverage = min_value(cast_float4_to_half4(v_clipRect));
            coverage = clamp(clipRectCoverage, make_half(.0), coverage);
        }
#endif // ENABLE_CLIP_RECT

        half4 color = find_paint_color(v_paint
#ifdef @TARGET_VULKAN
                                       ,
                                       imagePaintDDX,
                                       imagePaintDDY
#endif
                                           FRAGMENT_CONTEXT_UNPACK);
        color.a *= coverage;

        half4 dstColor;
        if (coverageBufferID != v_pathID)
        {
            // This is the first fragment from pathID to touch this pixel.
            dstColor = PLS_LOAD4F(colorBuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // We don't need to store coverage when drawing interior triangles because they always
            // go last and don't overlap, so every fragment is the final one in the path.
            PLS_STORE4F(scratchColorBuffer, dstColor);
#endif
        }
        else
        {
            dstColor = PLS_LOAD4F(scratchColorBuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // Since interior triangles are always last, there's no need to preserve this value.
            PLS_PRESERVE_4F(scratchColorBuffer);
#endif
        }

        // Blend with the framebuffer color.
#ifdef @ENABLE_ADVANCED_BLEND
        if (@ENABLE_ADVANCED_BLEND && v_blendMode != cast_uint_to_half(BLEND_SRC_OVER))
        {
            color = advanced_blend(color, unmultiply(dstColor), cast_half_to_ushort(v_blendMode));
        }
        else
#endif
        {
            color.rgb *= color.a;
            color = color + dstColor * (1. - color.a);
        }

        PLS_STORE4F(colorBuffer, color);
    }

#ifndef @DRAW_INTERIOR_TRIANGLES
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_END;
#endif

    EMIT_PLS;
}

#else // USING_DEPTH_STENCIL

FRAG_DATA_MAIN(half4, @drawFragmentMain)
{
    VARYING_UNPACK(v_paint, float4);
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(v_blendMode, half);
#endif

    half4 color = find_paint_color(v_paint);

#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
        half4 dstColor = TEXEL_FETCH(@dstColorTexture, int2(floor(_fragCoord.xy)));
        color = advanced_blend(color, unmultiply(dstColor), cast_half_to_ushort(v_blendMode));
    }
    else
#endif // !ENABLE_ADVANCED_BLEND
    {
        color = premultiply(color);
    }
    EMIT_FRAG_DATA(color);
}

#endif // !USING_DEPTH_STENCIL

#endif // FRAGMENT
