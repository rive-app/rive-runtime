/*
 * Copyright 2023 Rive
 */

#ifdef @FRAGMENT

PLS_BLOCK_BEGIN
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_BLOCK_END

// ATLAS_BLIT includes draw_path_common.glsl, which declares the textures &
// samplers, so we only need to declare these for image meshes.
#ifdef @DRAW_IMAGE_MESH
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
FRAG_TEXTURE_BLOCK_END

DYNAMIC_SAMPLER_BLOCK_BEGIN
SAMPLER_DYNAMIC(PER_DRAW_BINDINGS_SET, IMAGE_SAMPLER_IDX, imageSampler)
DYNAMIC_SAMPLER_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END
#endif // @DRAW_IMAGE_MESH

#ifdef @DRAW_IMAGE_MESH
PLS_MAIN_WITH_IMAGE_UNIFORMS(@drawFragmentMain)
#else
PLS_MAIN(@drawFragmentMain)
#endif
{
#ifdef @ATLAS_BLIT
    VARYING_UNPACK(v_paint, float4);
    VARYING_UNPACK(v_atlasCoord, float2);
#endif
#ifdef @DRAW_IMAGE_MESH
    VARYING_UNPACK(v_texCoord, float2);
#endif
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif
#if defined(@ATLAS_BLIT) && defined(@ENABLE_ADVANCED_BLEND)
    VARYING_UNPACK(v_blendMode, half);
#endif

#ifdef @ATLAS_BLIT
    half4 color = find_paint_color(v_paint, 1. FRAGMENT_CONTEXT_UNPACK);
    half coverage = filter_feather_atlas(
        v_atlasCoord,
        uniforms.atlasTextureInverseSize TEXTURE_CONTEXT_FORWARD);
#endif

#ifdef @DRAW_IMAGE_MESH
    half4 color = TEXTURE_SAMPLE_DYNAMIC_LODBIAS(@imageTexture,
                                                 imageSampler,
                                                 v_texCoord,
                                                 uniforms.mipMapLODBias);
    half coverage = 1.;
#endif

#ifdef @ENABLE_CLIP_RECT
    // Calculate the clip rect before entering the interlock.
    if (@ENABLE_CLIP_RECT)
    {
        half clipRectCoverage =
            max(min_value(cast_float4_to_half4(v_clipRect)), make_half(.0));
        coverage = min(clipRectCoverage, coverage);
    }
#endif

    PLS_INTERLOCK_BEGIN;

#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING && v_clipID != .0)
    {
        half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
        half clipContentID = clipData.g;
        half clipCoverage =
            max(clipContentID == v_clipID ? clipData.r : make_half(.0),
                make_half(.0));
        coverage = min(coverage, clipCoverage);
    }
#endif

#ifdef @DRAW_IMAGE_MESH
    // Apply opacity after clipping.
    coverage *= imageDrawUniforms.opacity;
#endif

    half4 dstColorPremul = PLS_LOAD4F(colorBuffer);
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
#ifdef @ATLAS_BLIT
        // GENERATE_PREMULTIPLIED_PAINT_COLORS is false in this case for
        // find_paint_color() because advanced blend needs unmultiplied colors.
        ushort blendMode = cast_half_to_ushort(v_blendMode);
#endif

#ifdef @DRAW_IMAGE_MESH
        // Unmultiply the image for advanced blend. Images are always
        // premultiplied so that the filtering works correctly.
        // TODO: This unmultiply technically isn't necessary with srcOver blend.
        // We may want to experiment with dynamically not premultiplying here
        // and in find_paint_color() when the blend mode is srcOver.
        color.rgb = unmultiply_rgb(color);
        ushort blendMode = cast_uint_to_ushort(imageDrawUniforms.blendMode);
#endif

        if (blendMode != BLEND_SRC_OVER)
        {
            color.rgb =
                advanced_color_blend(color.rgb, dstColorPremul, blendMode);
        }
        // Premultiply alpha now.
        color.a *= coverage;
        color.rgb *= color.a;
    }
    else
#endif // @ENABLE_ADVANCED_BLEND
    {
        color *= coverage;
    }

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

    PLS_STORE4F(colorBuffer, dstColorPremul * (1. - color.a) + color);

    PLS_PRESERVE_UI(clipBuffer);
    PLS_PRESERVE_UI(coverageCountBuffer);
    PLS_INTERLOCK_END;

    EMIT_PLS;
}

#endif // @FRAGMENT
