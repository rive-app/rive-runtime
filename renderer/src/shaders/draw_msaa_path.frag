/*
 * Copyright 2022 Rive
 */

#ifdef @FRAGMENT

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

#endif // FRAGMENT
