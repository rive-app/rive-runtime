/*
 * Copyright 2023 Rive
 */

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(PositionAttr)
ATTR(0, float2, @a_position);
ATTR_BLOCK_END

ATTR_BLOCK_BEGIN(UVAttr)
ATTR(1, float2, @a_texCoord);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(1, half, v_clipID);
#endif
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

IMAGE_MESH_VERTEX_MAIN(@drawVertexMain, PositionAttr, position, UVAttr, uv, _vertexID)
{
    ATTR_UNPACK(_vertexID, position, @a_position, float2);
    ATTR_UNPACK(_vertexID, uv, @a_texCoord, float2);

    VARYING_INIT(v_texCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif

    float2 vertexPosition =
        MUL(make_float2x2(imageDrawUniforms.viewMatrix), @a_position) + imageDrawUniforms.translate;
    v_texCoord = @a_texCoord;
#ifdef @ENABLE_CLIPPING
    v_clipID = id_bits_to_f16(imageDrawUniforms.clipID, uniforms.pathIDGranularity);
#endif
#ifdef @ENABLE_CLIP_RECT
    v_clipRect =
        find_clip_rect_coverage_distances(make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
                                          imageDrawUniforms.clipRectInverseTranslate,
                                          vertexPosition);
#endif
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
#ifdef @USING_DEPTH_STENCIL
    pos.z = 1. - float(imageDrawUniforms.zIndex) * (2. / 32768.);
#endif

    VARYING_PACK(v_texCoord);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(v_clipID);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(IMAGE_TEXTURE_IDX, @imageTexture);
FRAG_TEXTURE_BLOCK_END

SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

#ifndef @USING_DEPTH_STENCIL

PLS_BLOCK_BEGIN
PLS_DECL4F(FRAMEBUFFER_PLANE_IDX, framebuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
PLS_DECL4F(ORIGINAL_DST_COLOR_PLANE_IDX, originalDstColorBuffer);
PLS_BLOCK_END

PLS_MAIN_WITH_IMAGE_UNIFORMS(@drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif

    half4 color = TEXTURE_SAMPLE(@imageTexture, imageSampler, v_texCoord);
    half coverage = 1.;

#ifdef @ENABLE_CLIP_RECT
    half clipRectCoverage = min_value(make_half4(v_clipRect));
    coverage = clamp(clipRectCoverage, make_half(0), coverage);
#endif

    PLS_INTERLOCK_BEGIN;

#ifdef @ENABLE_CLIPPING
    if (v_clipID != .0)
    {
        half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
        half clipContentID = clipData.g;
        half clipCoverage = clipContentID == v_clipID ? clipData.r : make_half(0);
        coverage = min(coverage, clipCoverage);
    }
#endif

    // Blend with the framebuffer color.
    color.a *= imageDrawUniforms.opacity * coverage;
    half4 dstColor = PLS_LOAD4F(framebuffer);
#ifdef @ENABLE_ADVANCED_BLEND
    if (imageDrawUniforms.blendMode != 0u /*!srcOver*/)
    {
#ifdef @ENABLE_HSL_BLEND_MODES
        color = advanced_hsl_blend(
#else
        color = advanced_blend(
#endif
            color,
            unmultiply(dstColor),
            make_ushort(imageDrawUniforms.blendMode));
    }
    else
#endif
    {
        color.rgb *= color.a;
        color = color + dstColor * (1. - color.a);
    }

    PLS_STORE4F(framebuffer, color);
    PLS_PRESERVE_VALUE(coverageCountBuffer);
    PLS_PRESERVE_VALUE(clipBuffer);

    PLS_INTERLOCK_END;

    EMIT_PLS;
}

#else // USING_DEPTH_STENCIL

FRAG_DATA_MAIN(half4, @drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);
    half4 color = TEXTURE_SAMPLE(@imageTexture, imageSampler, v_texCoord);
    color.a *= imageDrawUniforms.opacity;
    EMIT_FRAG_DATA(premultiply(color));
}

#endif // USING_DEPTH_STENCIL
#endif // FRAGMENT
