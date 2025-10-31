/*
 * Copyright 2022 Rive
 */

// undef GENERATE_PREMULTIPLIED_PAINT_COLORS first because this file gets
// included multiple times with different defines in the Metal library.
#undef GENERATE_PREMULTIPLIED_PAINT_COLORS
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

// undef COVERAGE_TYPE first because this file gets included multiple times with
// different defines in the Metal library.
#undef COVERAGE_TYPE
#ifdef @ENABLE_FEATHER
#define COVERAGE_TYPE float4
#else
#define COVERAGE_TYPE half2
#endif

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
#if defined(@DRAW_INTERIOR_TRIANGLES) || defined(@ATLAS_BLIT)
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
#else
NO_PERSPECTIVE VARYING(2, COVERAGE_TYPE, v_coverages);
#endif //@DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(3, half, v_pathID);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
#ifdef @ATLAS_BLIT
@OPTIONALLY_FLAT VARYING(4, half, v_clipID); // [clipID, outerClipID]
#else
@OPTIONALLY_FLAT VARYING(4, half2, v_clipIDs); // [clipID, outerClipID]
#endif
#endif // @ENABLE_CLIPPING
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_MSAA)
NO_PERSPECTIVE VARYING(5, float4, v_clipRect);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(6, half, v_blendMode);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
#if defined(@DRAW_INTERIOR_TRIANGLES) || defined(@ATLAS_BLIT)
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
#else
    VARYING_INIT(v_coverages, COVERAGE_TYPE);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_pathID, half);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
#ifdef @ATLAS_BLIT
    VARYING_INIT(v_clipID, half);
#else
    VARYING_INIT(v_clipIDs, half2);
#endif
#endif // @ENABLE_CLIPPING
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_MSAA)
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
#ifdef @ATLAS_BLIT
        v_clipID = clipID;
#else
        v_clipIDs.x = clipID;
#endif
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
                                      fragCoord CLIP_CONTEXT_UNPACK);
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
#if defined(@ENABLE_CLIPPING) && !defined(@ATLAS_BLIT)
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
#else
    VARYING_PACK(v_coverages);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_PACK(v_pathID);
#endif // !@RENDER_MODE_MSAA

#ifdef @ENABLE_CLIPPING
#ifdef @ATLAS_BLIT
    VARYING_PACK(v_clipID);
#else
    VARYING_PACK(v_clipIDs);
#endif
#endif // @ENABLE_CLIPPING
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_MSAA)
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

// Add a function here for fragments to unpack the paint since we're the ones
// who packed it in the vertex shader.
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

#if !defined(@DRAW_INTERIOR_TRIANGLES) && !defined(@ATLAS_BLIT)

// Add functions here for fragments to unpack and evaluate coverage since we're
// the ones who packed the coverage components in the vertex shader.
INLINE half find_stroke_coverage(COVERAGE_TYPE coverages TEXTURE_CONTEXT_DECL)
{
#ifdef @ENABLE_FEATHER
    if (@ENABLE_FEATHER && is_feathered_stroke(coverages))
        return eval_feathered_stroke(coverages TEXTURE_CONTEXT_FORWARD);
    else
#endif // @ENABLE_FEATHER
        return min(coverages.x, coverages.y);
}

INLINE half find_fill_coverage(COVERAGE_TYPE coverages TEXTURE_CONTEXT_DECL)
{
#if defined(@ENABLE_FEATHER)
    if (@ENABLE_FEATHER && is_feathered_fill(coverages))
        return eval_feathered_fill(coverages TEXTURE_CONTEXT_FORWARD);
    else
#endif // @ENABLE_FEATHER
        return coverages.x;
}

INLINE half find_frag_coverage(COVERAGE_TYPE coverages TEXTURE_CONTEXT_DECL)
{
    if (is_stroke(coverages))
        return find_stroke_coverage(coverages TEXTURE_CONTEXT_FORWARD);
    else // Fill. (Back-face culling handles the sign of coverages.x.)
        return find_fill_coverage(coverages TEXTURE_CONTEXT_FORWARD);
}

INLINE half apply_frag_coverage(half initialCoverage,
                                COVERAGE_TYPE coverages TEXTURE_CONTEXT_DECL)
{
    if (is_stroke(coverages))
    {
        half fragCoverage =
            find_stroke_coverage(coverages TEXTURE_CONTEXT_FORWARD);
        return max(fragCoverage, initialCoverage);
    }
    else // Fill. (Back-face culling handles the sign of coverages.x.)
    {
        half fragCoverage =
            find_fill_coverage(coverages TEXTURE_CONTEXT_FORWARD);
        return initialCoverage + fragCoverage;
    }
}

#endif // !@DRAW_INTERIOR_TRIANGLES && !@ATLAS_BLIT

#endif // @FRAGMENT
