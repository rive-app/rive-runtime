/*
 * Copyright 2022 Rive
 */

#ifdef @FRAGMENT

// Path draws include draw_path_common.glsl, which declares the textures &
// samplers, so we only need to declare these for image meshes.
#ifdef @DRAW_IMAGE_MESH
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
#ifdef @ENABLE_ADVANCED_BLEND
DST_COLOR_TEXTURE(@dstColorTexture);
#endif
FRAG_TEXTURE_BLOCK_END

DYNAMIC_SAMPLER_BLOCK_BEGIN
SAMPLER_DYNAMIC(PER_DRAW_BINDINGS_SET, IMAGE_SAMPLER_IDX, imageSampler)
DYNAMIC_SAMPLER_BLOCK_END
#endif // @DRAW_IMAGE_MESH

FRAG_DATA_MAIN(half4, @drawFragmentMain)
{
#ifdef @DRAW_IMAGE_MESH
    VARYING_UNPACK(v_texCoord, float2);
#else
    VARYING_UNPACK(v_paint, float4);
#ifdef @ATLAS_BLIT
    VARYING_UNPACK(v_atlasCoord, float2);
#endif // @ATLAS_BLIT
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(v_blendMode, half);
#endif
#endif // !@DRAW_IMAGE_MESH

#ifdef @DRAW_IMAGE_MESH
    half4 color = TEXTURE_SAMPLE_DYNAMIC_LODBIAS(@imageTexture,
                                                 imageSampler,
                                                 v_texCoord,
                                                 uniforms.mipMapLODBias) *
                  imageDrawUniforms.opacity;
#else
    half coverage =
#ifdef @ATLAS_BLIT
        filter_feather_atlas(
            v_atlasCoord,
            uniforms.atlasTextureInverseSize TEXTURE_CONTEXT_FORWARD);
#else
        1.;
#endif
    half4 color = find_paint_color(v_paint, coverage FRAGMENT_CONTEXT_UNPACK);
#endif

#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
        // Do the color portion of the blend mode in the shader.
#ifdef @DRAW_IMAGE_MESH
        color.rgb = unmultiply_rgb(color);
        ushort blendMode = cast_uint_to_ushort(imageDrawUniforms.blendMode);
#else
        // NOTE: for non-image-meshes, "color" is already unmultiplied because
        // GENERATE_PREMULTIPLIED_PAINT_COLORS is false when using advanced
        // blend.
        ushort blendMode = cast_half_to_ushort(v_blendMode);
#endif
        half4 dstColorPremul = DST_COLOR_FETCH(@dstColorTexture);
        color.rgb = advanced_color_blend(color.rgb, dstColorPremul, blendMode);
#endif // @FIXED_FUNCTION_COLOR_OUTPUT

        // Src-over blending is enabled, so just premultiply and let the HW
        // finish the the the alpha portion of the blend mode.
        color.rgb *= color.a;
    }
#endif // @ENABLE_ADVANCED_BLEND

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

    color.rgb = add_dither(color.rgb,
                           _fragCoord.xy,
                           uniforms.ditherScale,
                           uniforms.ditherBias);
    EMIT_FRAG_DATA(color);
}

#endif // FRAGMENT
