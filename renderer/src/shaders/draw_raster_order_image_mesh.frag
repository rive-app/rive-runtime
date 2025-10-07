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

FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
FRAG_TEXTURE_BLOCK_END

DYNAMIC_SAMPLER_BLOCK_BEGIN
SAMPLER_DYNAMIC(PER_DRAW_BINDINGS_SET, IMAGE_SAMPLER_IDX, imageSampler)
DYNAMIC_SAMPLER_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

PLS_MAIN_WITH_IMAGE_UNIFORMS(@drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif

    half4 color = TEXTURE_SAMPLE_DYNAMIC_LODBIAS(@imageTexture,
                                                 imageSampler,
                                                 v_texCoord,
                                                 uniforms.mipMapLODBias);

    half coverage = 1.;

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        half clipRectCoverage = min_value(cast_float4_to_half4(v_clipRect));
        coverage = clamp(clipRectCoverage, make_half(.0), coverage);
    }
#endif

    PLS_INTERLOCK_BEGIN;

#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING && v_clipID != .0)
    {
        half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
        half clipContentID = clipData.g;
        half clipCoverage =
            clipContentID == v_clipID ? clipData.r : make_half(.0);
        coverage = min(coverage, clipCoverage);
    }
#endif

    // Blend with the framebuffer color.
    half4 dstColorPremul = PLS_LOAD4F(colorBuffer);
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND && imageDrawUniforms.blendMode != BLEND_SRC_OVER)
    {
        color.rgb = advanced_color_blend(
                        unmultiply_rgb(color),
                        dstColorPremul,
                        cast_uint_to_ushort(imageDrawUniforms.blendMode)) *
                    color.a;
    }
#endif
    color *= imageDrawUniforms.opacity * coverage;
    color += dstColorPremul * (1. - color.a);

    PLS_STORE4F(colorBuffer, color);
    PLS_PRESERVE_UI(clipBuffer);
    PLS_PRESERVE_UI(coverageCountBuffer);

    PLS_INTERLOCK_END;

    EMIT_PLS;
}

#endif // @FRAGMENT
