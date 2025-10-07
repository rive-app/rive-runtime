/*
 * Copyright 2022 Rive
 */

#ifdef @FRAGMENT

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
#else
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_windingWeight, half);
#else
    VARYING_UNPACK(v_coverages, COVERAGE_TYPE);
#endif //@DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(v_pathID, half);
#endif

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
    // Preserve the coverage buffer even though we don't use it, so it doesn't
    // get overwritten in a way that would corrupt a future draw (e.g., by
    // accidentally writing the next path's id with a bogus coverage.)
    PLS_PRESERVE_UI(coverageCountBuffer);
#else
    coverageCount =
        apply_frag_coverage(coverageCount, v_coverages TEXTURE_CONTEXT_FORWARD);
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

#endif // FRAGMENT
