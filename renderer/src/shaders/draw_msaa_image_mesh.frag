/*
 * Copyright 2023 Rive
 */

#ifdef @FRAGMENT

FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
#ifdef @ENABLE_ADVANCED_BLEND
DST_COLOR_TEXTURE(@dstColorTexture);
#endif
FRAG_TEXTURE_BLOCK_END

DYNAMIC_SAMPLER_BLOCK_BEGIN
SAMPLER_DYNAMIC(PER_DRAW_BINDINGS_SET, IMAGE_SAMPLER_IDX, imageSampler)
DYNAMIC_SAMPLER_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

FRAG_DATA_MAIN(half4, @drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);

    half4 color = TEXTURE_SAMPLE_DYNAMIC_LODBIAS(@imageTexture,
                                                 imageSampler,
                                                 v_texCoord,
                                                 uniforms.mipMapLODBias) *
                  imageDrawUniforms.opacity;

#if defined(@ENABLE_ADVANCED_BLEND) && !defined(@FIXED_FUNCTION_COLOR_OUTPUT)
    if (@ENABLE_ADVANCED_BLEND)
    {
        // Do the color portion of the blend mode in the shader.
        half4 dstColorPremul = DST_COLOR_FETCH(@dstColorTexture);
        color.rgb = advanced_color_blend(unmultiply_rgb(color),
                                         dstColorPremul,
                                         imageDrawUniforms.blendMode);
        // Src-over blending is enabled, so just premultiply and let the HW
        // finish the the the alpha portion of the blend mode.
        color.rgb *= color.a;
    }
#endif // @ENABLE_ADVANCED_BLEND && !@FIXED_FUNCTION_COLOR_OUTPUT

    EMIT_FRAG_DATA(color);
}

#endif // @FRAGMENT
