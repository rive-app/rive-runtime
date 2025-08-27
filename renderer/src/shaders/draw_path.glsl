/*
 * Copyright 2022 Rive
 */

#ifdef @ENABLE_ADVANCED_BLEND
// If advanced blend is enabled, we generate unmultiplied paint colors in the
// shader. Otherwise we would have to just turn around and unmultiply them in
// order to run the blend equation.
#define GENERATE_PREMULTIPLIED_PAINT_COLORS !@ENABLE_ADVANCED_BLEND
#else
// As long as advanced blend is not enabled, it's more efficient for the shader
// to generate premultiplied paint colors from the start.
#define GENERATE_PREMULTIPLIED_PAINT_COLORS true
#endif

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
#ifdef @DRAW_INTERIOR_TRIANGLES
ATTR(0, packed_float3, @a_triangleVertex);
#else
ATTR(0,
     float4,
     @a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, @a_mirroredVertexData);
#endif
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_paint);

#ifdef @ATLAS_BLIT
NO_PERSPECTIVE VARYING(1, float2, v_atlasCoord);
#elif !defined(@RENDER_MODE_MSAA)
#ifdef @DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(1, half, v_windingWeight);
#elif defined(@ENABLE_FEATHER)
NO_PERSPECTIVE VARYING(2, float4, v_coverages);
#else
NO_PERSPECTIVE VARYING(2, half2, v_coverages);
#endif //@DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(3, half, v_pathID);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(4, half2, v_clipIDs); // [clipID, outerClipID]
#endif
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(5, float4, v_clipRect);
#endif
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

#ifdef @ATLAS_BLIT
    VARYING_INIT(v_atlasCoord, float2);
#elif !defined(@RENDER_MODE_MSAA)
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_windingWeight, half);
#elif defined(@ENABLE_FEATHER)
    VARYING_INIT(v_coverages, float4);
#else
    VARYING_INIT(v_coverages, half2);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_pathID, half);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
    VARYING_INIT(v_clipIDs, half2);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_INIT(v_blendMode, half);
#endif

    bool shouldDiscardVertex = false;
    uint pathID;
    float2 vertexPosition;
#ifdef @RENDER_MODE_MSAA
    ushort pathZIndex;
#endif

#ifdef @ATLAS_BLIT
    vertexPosition =
        unpack_atlas_coverage_vertex(@a_triangleVertex,
                                     pathID,
#ifdef @RENDER_MODE_MSAA
                                     pathZIndex,
#endif
                                     v_atlasCoord VERTEX_CONTEXT_UNPACK);
#elif defined(@DRAW_INTERIOR_TRIANGLES)
    vertexPosition = unpack_interior_triangle_vertex(@a_triangleVertex,
                                                     pathID
#ifdef @RENDER_MODE_MSAA
                                                     ,
                                                     pathZIndex
#else
                                                     ,
                                                     v_windingWeight
#endif
                                                         VERTEX_CONTEXT_UNPACK);
#else // !@DRAW_INTERIOR_TRIANGLES
    float4 coverages;
    shouldDiscardVertex =
        !unpack_tessellated_path_vertex(@a_patchVertexData,
                                        @a_mirroredVertexData,
                                        _instanceID,
                                        pathID,
                                        vertexPosition
#ifndef @RENDER_MODE_MSAA
                                        ,
                                        coverages
#else
                                        ,
                                        pathZIndex
#endif
                                            VERTEX_CONTEXT_UNPACK);
#ifndef @RENDER_MODE_MSAA
#ifdef @ENABLE_FEATHER
    v_coverages = coverages;
#else
    v_coverages.xy = cast_float2_to_half2(coverages.xy);
#endif
#endif
#endif // !DRAW_INTERIOR_TRIANGLES

    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, pathID);

#if !defined(@ATLAS_BLIT) && !defined(@RENDER_MODE_MSAA)
    // Encode the integral pathID as a "half" that we know the hardware will see
    // as a unique value in the fragment shader.
    v_pathID = id_bits_to_f16(pathID, uniforms.pathIDGranularity);

    // Indicate even-odd fill rule by making pathID negative.
    if ((paintData.x & PAINT_FLAG_EVEN_ODD_FILL) != 0u)
        v_pathID = -v_pathID;
#endif // !@ATLAS_BLIT && !@RENDER_MODE_MSAA

    uint paintType = paintData.x & 0xfu;
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        uint clipIDBits =
            (paintType == CLIP_UPDATE_PAINT_TYPE ? paintData.y : paintData.x) >>
            16;
        half clipID = id_bits_to_f16(clipIDBits, uniforms.pathIDGranularity);
        // Negative clipID means to update the clip buffer instead of the color
        // buffer.
        if (paintType == CLIP_UPDATE_PAINT_TYPE)
            clipID = -clipID;
        v_clipIDs.x = clipID;
    }
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
        v_blendMode = float((paintData.x >> 4) & 0xfu);
    }
#endif

    // Paint matrices operate on the fragment shader's "_fragCoord", which is
    // bottom-up in GL.
    float2 fragCoord = vertexPosition;
#ifdef @FRAMEBUFFER_BOTTOM_UP
    fragCoord.y = float(uniforms.renderTargetHeight) - fragCoord.y;
#endif

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        // clipRectInverseMatrix transforms from pixel coordinates to a space
        // where the clipRect is the normalized rectangle: [-1, -1, 1, 1].
        float2x2 clipRectInverseMatrix = make_float2x2(
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 2u));
        float4 clipRectInverseTranslate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 3u);
#ifndef @RENDER_MODE_MSAA
        v_clipRect =
            find_clip_rect_coverage_distances(clipRectInverseMatrix,
                                              clipRectInverseTranslate.xy,
                                              fragCoord);
#else  // !@RENDER_MODE_MSAA => @RENDER_MODE_MSAA
        set_clip_rect_plane_distances(clipRectInverseMatrix,
                                      clipRectInverseTranslate.xy,
                                      fragCoord);
#endif // @RENDER_MODE_MSAA
    }
#endif // ENABLE_CLIP_RECT
       // #endif // TARGET_VULKAN

    // Unpack the paint once we have a position.
    if (paintType == SOLID_COLOR_PAINT_TYPE)
    {
        half4 color = unpackUnorm4x8(paintData.y);
        if (GENERATE_PREMULTIPLIED_PAINT_COLORS)
            color.rgb *= color.a;
        v_paint = float4(color);
    }
#ifdef @ENABLE_CLIPPING
    else if (@ENABLE_CLIPPING && paintType == CLIP_UPDATE_PAINT_TYPE)
    {
        half outerClipID =
            id_bits_to_f16(paintData.x >> 16, uniforms.pathIDGranularity);
        v_clipIDs.y = outerClipID;
    }
#endif
    else
    {
        float2x2 paintMatrix =
            make_float2x2(STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u));
        float4 paintTranslate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer, pathID * 4u + 1u);
        float2 paintCoord = MUL(paintMatrix, fragCoord) + paintTranslate.xy;
        if (paintType == LINEAR_GRADIENT_PAINT_TYPE ||
            paintType == RADIAL_GRADIENT_PAINT_TYPE)
        {
            // v_paint.a contains "-row" of the gradient ramp at texel center,
            // in normalized space.
            v_paint.a = -uintBitsToFloat(paintData.y);
            // abs(v_paint.b) contains either:
            //   - 2 if the gradient ramp spans an entire row.
            //   - x0 of the gradient ramp in normalized space, if it's a simple
            //   2-texel ramp.
            float gradientSpan = paintTranslate.z;
            // gradientSpan is either ~1 (complex gradients span the whole width
            // of the texture minus 1px), or 1/GRAD_TEXTURE_WIDTH (simple
            // gradients span 1px).
            if (gradientSpan > .9)
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
                // The paint is a radial gradient. Mark v_paint.b negative to
                // indicate this to the fragment shader. (v_paint.b can't be
                // zero because the gradient ramp is aligned on pixel centers,
                // so negating it will always produce a negative number.)
                v_paint.b = -v_paint.b;
                v_paint.rg = paintCoord.xy;
            }
        }
        else // IMAGE_PAINT_TYPE
        {
            // v_paint.a <= -1. signals that the paint is an image.
            // -v_paint.a - 2 is the texture mipmap level-of-detail.
            // v_paint.b is the image opacity.
            // v_paint.rg is the normalized image texture coordinate (built into
            // the paintMatrix).
            float opacity = uintBitsToFloat(paintData.y);
            float lod = paintTranslate.z;
            v_paint = float4(paintCoord.x, paintCoord.y, opacity, -2. - lod);
        }
    }

    float4 pos;
    if (!shouldDiscardVertex)
    {
        pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
#ifdef @POST_INVERT_Y
        pos.y = -pos.y;
#endif
#ifdef @RENDER_MODE_MSAA
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
#ifdef @ATLAS_BLIT
    VARYING_PACK(v_atlasCoord);
#elif !defined(@RENDER_MODE_MSAA)
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_PACK(v_windingWeight);
#elif defined(@ENABLE_FEATHER)
    VARYING_PACK(v_coverages);
#else
    VARYING_PACK(v_coverages);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_PACK(v_pathID);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
    VARYING_PACK(v_clipIDs);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_PACK(v_blendMode);
#endif
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

INLINE half4 find_paint_color(float4 paint,
                              float coverage FRAGMENT_CONTEXT_DECL)
{
    half4 color;
    if (paint.a >= .0) // Is the paint a solid color?
    {
        // The vertex shader will have premultiplied 'paint' (or not) based on
        // GENERATE_PREMULTIPLIED_PAINT_COLORS.
        color = cast_float4_to_half4(paint);
        if (GENERATE_PREMULTIPLIED_PAINT_COLORS)
            color *= coverage;
        else
            color.a *= coverage;
    }
    else if (paint.a > -1.) // Is paint is a gradient (linear or radial)?
    {
        float t =
            paint.b > .0 ? /*linear*/ paint.r : /*radial*/ length(paint.rg);
        t = clamp(t, .0, 1.);
        float span = abs(paint.b);
        float x = span > 1.
                      ? /*entire row*/ (1. - 1. / GRAD_TEXTURE_WIDTH) * t +
                            (.5 / GRAD_TEXTURE_WIDTH)
                      : /*two texels*/ (1. / GRAD_TEXTURE_WIDTH) * t + span;
        float row = -paint.a;
        // Our gradient texture is not mipmapped. Issue a texture-sample that
        // explicitly does not find derivatives for LOD computation.
        color =
            TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, float2(x, row), .0);
        color.a *= coverage;
        // Gradients are always unmultiplied so we don't lose color data while
        // doing the hardware filter.
        if (GENERATE_PREMULTIPLIED_PAINT_COLORS)
            color.rgb *= color.a;
    }
    else // The paint is an image.
    {
        half lod = -paint.a - 2.;
        color = TEXTURE_SAMPLE_DYNAMIC_LOD(@imageTexture,
                                           imageSampler,
                                           paint.rg,
                                           lod);
        half opacity = paint.b * coverage;
        // Images are always premultiplied so the (transparent) background color
        // doesn't bleed into the edges during the hardware filter.
        if (GENERATE_PREMULTIPLIED_PAINT_COLORS)
            color *= opacity;
        else
            color = make_half4(unmultiply_rgb(color), color.a * opacity);
    }
    return color;
}

#ifndef @RENDER_MODE_MSAA

PLS_BLOCK_BEGIN
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_BLOCK_END

PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_paint, float4);

#ifdef @ATLAS_BLIT
    VARYING_UNPACK(v_atlasCoord, float2);
#elif !defined(@RENDER_MODE_MSAA)
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_windingWeight, half);
#elif defined(@ENABLE_FEATHER)
    VARYING_UNPACK(v_coverages, float4);
#else
    VARYING_UNPACK(v_coverages, half2);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_pathID, half);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(v_clipIDs, half2);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(v_blendMode, half);
#endif

#if !defined(@DRAW_INTERIOR_TRIANGLES) || defined(@ATLAS_BLIT)
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_BEGIN;
#endif

    half coverage;
#ifdef @ATLAS_BLIT
    coverage = filter_feather_atlas(
        v_atlasCoord,
        uniforms.atlasTextureInverseSize TEXTURE_CONTEXT_FORWARD);
#else
    half2 coverageData = unpackHalf2x16(PLS_LOADUI(coverageCountBuffer));
    half coverageBufferID = coverageData.g;
    half coverageCount =
        coverageBufferID == v_pathID ? coverageData.r : make_half(.0);

#ifdef @DRAW_INTERIOR_TRIANGLES
    coverageCount += v_windingWeight;
    PLS_PRESERVE_UI(coverageCountBuffer);
#else
    if (is_stroke(v_coverages))
    {
        half fragCoverage;
#ifdef @ENABLE_FEATHER
        if (@ENABLE_FEATHER && is_feathered_stroke(v_coverages))
        {
            fragCoverage =
                eval_feathered_stroke(v_coverages TEXTURE_CONTEXT_FORWARD);
        }
        else
#endif // @ENABLE_FEATHER
        {
            fragCoverage = min(v_coverages.x, v_coverages.y);
        }
        coverageCount = max(fragCoverage, coverageCount);
    }
    else // Fill. (Back-face culling handles the sign of v_coverages.x.)
    {
        half fragCoverage;
#if defined(@ENABLE_FEATHER)
        if (@ENABLE_FEATHER && is_feathered_fill(v_coverages))
        {
            fragCoverage =
                eval_feathered_fill(v_coverages TEXTURE_CONTEXT_FORWARD);
        }
        else
#endif // @CLOCKWISE_FILL && @ENABLE_FEATHER
        {
            fragCoverage = v_coverages.x;
        }
        coverageCount += fragCoverage;
    }

    // Save the updated coverage.
    PLS_STOREUI(coverageCountBuffer,
                packHalf2x16(make_half2(coverageCount, v_pathID)));
#endif // !@DRAW_INTERIOR_TRIANGLES

    // Convert coverageCount to coverage.
#ifdef @CLOCKWISE_FILL
    if (@CLOCKWISE_FILL)
    {
#ifdef @VULKAN_VENDOR_ID
        if (@VULKAN_VENDOR_ID == VULKAN_VENDOR_ARM)
        {
            // ARM hits a bug if we use clamp() here.
            if (coverageCount < .0)
                coverage = .0;
            else if (coverageCount <= 1.)
                coverage = coverageCount;
            else
                coverage = 1.;
        }
        else
#endif
        {
            coverage = clamp(coverageCount, make_half(.0), make_half(1.));
        }
    }
    else
#endif // CLOCKWISE_FILL
    {
        coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
        if (@ENABLE_EVEN_ODD && v_pathID < .0 /*even-odd*/)
        {
            coverage = 1. - make_half(abs(fract(coverage * .5) * 2. + -1.));
        }
#endif
        // This also caps stroke coverage, which can be >1.
        coverage = min(coverage, make_half(1.));
    }
#endif // !@ATLAS_BLIT

#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING && v_clipIDs.x < .0) // Update the clip buffer.
    {
        half clipID = -v_clipIDs.x;
#ifdef @ENABLE_NESTED_CLIPPING
        if (@ENABLE_NESTED_CLIPPING)
        {
            half outerClipID = v_clipIDs.y;
            if (outerClipID != .0)
            {
                // This is a nested clip. Intersect coverage with the enclosing
                // clip (outerClipID).
                half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
                half clipContentID = clipData.g;
                half outerClipCoverage;
                if (clipContentID != clipID)
                {
                    // First hit: either clipBuffer contains outerClipCoverage,
                    // or this pixel is not inside the outer clip and
                    // outerClipCoverage is zero.
                    outerClipCoverage =
                        clipContentID == outerClipID ? clipData.r : .0;
#ifndef @DRAW_INTERIOR_TRIANGLES
                    // Stash outerClipCoverage before overwriting clipBuffer, in
                    // case we hit this pixel again and need it. (Not necessary
                    // when drawing interior triangles because they always go
                    // last and don't overlap.)
                    PLS_STORE4F(scratchColorBuffer,
                                make_half4(outerClipCoverage, .0, .0, .0));
#endif
                }
                else
                {
                    // Subsequent hit: outerClipCoverage is stashed in
                    // scratchColorBuffer.
                    outerClipCoverage = PLS_LOAD4F(scratchColorBuffer).r;
#ifndef @DRAW_INTERIOR_TRIANGLES
                    // Since interior triangles are always last, there's no need
                    // to preserve this value.
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
            half clipID = v_clipIDs.x;
            if (clipID != .0)
            {
                // Clip IDs are not necessarily drawn in monotonically
                // increasing order, so always check exact equality of the
                // clipID.
                half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
                half clipContentID = clipData.g;
                coverage = (clipContentID == clipID) ? min(clipData.r, coverage)
                                                     : make_half(.0);
            }
        }
#endif
#ifdef @ENABLE_CLIP_RECT
        if (@ENABLE_CLIP_RECT)
        {
            half clipRectCoverage = min_value(cast_float4_to_half4(v_clipRect));
            coverage = clamp(clipRectCoverage, make_half(.0), coverage);
        }
#endif // ENABLE_CLIP_RECT

        half4 color =
            find_paint_color(v_paint, coverage FRAGMENT_CONTEXT_UNPACK);

        half4 dstColorPremul;
#ifdef @ATLAS_BLIT
        dstColorPremul = PLS_LOAD4F(colorBuffer);
#else
        if (coverageBufferID != v_pathID)
        {
            // This is the first fragment from pathID to touch this pixel.
            dstColorPremul = PLS_LOAD4F(colorBuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // We don't need to store coverage when drawing interior triangles
            // because they always go last and don't overlap, so every fragment
            // is the final one in the path.
            PLS_STORE4F(scratchColorBuffer, dstColorPremul);
#endif
        }
        else
        {
            dstColorPremul = PLS_LOAD4F(scratchColorBuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // Since interior triangles are always last, there's no need to
            // preserve this value.
            PLS_PRESERVE_4F(scratchColorBuffer);
#endif
        }
#endif // @ATLAS_BLIT

        // Blend with the framebuffer color.
#ifdef @ENABLE_ADVANCED_BLEND
        if (@ENABLE_ADVANCED_BLEND)
        {
            // GENERATE_PREMULTIPLIED_PAINT_COLORS is false in this case because
            // advanced blend needs unmultiplied colors.
            if (v_blendMode != cast_uint_to_half(BLEND_SRC_OVER))
            {
                color.rgb =
                    advanced_color_blend(color.rgb,
                                         dstColorPremul,
                                         cast_half_to_ushort(v_blendMode));
            }
            // Premultiply alpha now.
            color.rgb *= color.a;
        }
#endif

        // Certain platforms give us less control of the format of what we are
        // rendering too. Specifically, we are auto converted from linear ->
        // sRGB on render target writes in unreal. In those cases we made need
        // to end up in linear color space
#ifdef @NEEDS_GAMMA_CORRECTION
        if (@NEEDS_GAMMA_CORRECTION)
        {
            color = gamma_to_linear(color);
        }
#endif

        color += dstColorPremul * (1. - color.a);

        PLS_STORE4F(colorBuffer, color);
        PLS_PRESERVE_UI(clipBuffer);
    }

#if !defined(@DRAW_INTERIOR_TRIANGLES) || defined(@ATLAS_BLIT)
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_END;
#endif

    EMIT_PLS;
}

#else // !@RENDER_MODE_MSAA => @RENDER_MODE_MSAA

FRAG_DATA_MAIN(half4, @drawFragmentMain)
{
    VARYING_UNPACK(v_paint, float4);
#ifdef @ATLAS_BLIT
    VARYING_UNPACK(v_atlasCoord, float2);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(v_blendMode, half);
#endif

    half coverage =
#ifdef @ATLAS_BLIT
        filter_feather_atlas(
            v_atlasCoord,
            uniforms.atlasTextureInverseSize TEXTURE_CONTEXT_FORWARD);
#else
        1.;
#endif
    half4 color = find_paint_color(v_paint, coverage FRAGMENT_CONTEXT_UNPACK);

#if defined(@ENABLE_ADVANCED_BLEND) && !defined(@FIXED_FUNCTION_COLOR_OUTPUT)
    if (@ENABLE_ADVANCED_BLEND)
    {
        // Do the color portion of the blend mode in the shader.
        //
        // NOTE: "color" is already unmultiplied because
        // GENERATE_PREMULTIPLIED_PAINT_COLORS is false when using advanced
        // blend.
        half4 dstColorPremul = DST_COLOR_FETCH(@dstColorTexture);
        color.rgb = advanced_color_blend(color.rgb,
                                         dstColorPremul,
                                         cast_half_to_ushort(v_blendMode));
        // Src-over blending is enabled, so just premultiply and let the HW
        // finish the the the alpha portion of the blend mode.
        color.rgb *= color.a;
    }
#endif // @ENABLE_ADVANCED_BLEND && !@FIXED_FUNCTION_COLOR_OUTPUT

    // Certain platforms give us less control of the format of what we are
    // rendering too. Specifically, we are auto converted from linear -> sRGB on
    // render target writes in unreal. In those cases we made need to end up in
    // linear color space
#ifdef @NEEDS_GAMMA_CORRECTION
    if (@NEEDS_GAMMA_CORRECTION)
    {
        color = gamma_to_linear(color);
    }
#endif

    EMIT_FRAG_DATA(color);
}

#endif // @RENDER_MODE_MSAA

#endif // FRAGMENT
