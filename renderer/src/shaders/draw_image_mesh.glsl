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
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_MSAA)
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

IMAGE_MESH_VERTEX_MAIN(@drawVertexMain,
                       PositionAttr,
                       position,
                       UVAttr,
                       uv,
                       _vertexID)
{
    ATTR_UNPACK(_vertexID, position, @a_position, float2);
    ATTR_UNPACK(_vertexID, uv, @a_texCoord, float2);

    VARYING_INIT(v_texCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(v_clipID, half);
#endif
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_MSAA)
    VARYING_INIT(v_clipRect, float4);
#endif

    float2 vertexPosition =
        MUL(make_float2x2(imageDrawUniforms.viewMatrix), @a_position) +
        imageDrawUniforms.translate;
    v_texCoord = @a_texCoord;
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        v_clipID = id_bits_to_f16(imageDrawUniforms.clipID,
                                  uniforms.pathIDGranularity);
    }
#endif
#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
#ifndef @RENDER_MODE_MSAA
        v_clipRect = find_clip_rect_coverage_distances(
            make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
            imageDrawUniforms.clipRectInverseTranslate,
            vertexPosition);
#else
        set_clip_rect_plane_distances(
            make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
            imageDrawUniforms.clipRectInverseTranslate,
            vertexPosition);
#endif
    }
#endif // ENABLE_CLIP_RECT
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
#ifdef @POST_INVERT_Y
    pos.y = -pos.y;
#endif
#ifdef @RENDER_MODE_MSAA
    pos.z = normalize_z_index(imageDrawUniforms.zIndex);
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
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
#if defined(@RENDER_MODE_MSAA) && defined(@ENABLE_ADVANCED_BLEND)
DST_COLOR_TEXTURE(@dstColorTexture);
#endif
FRAG_TEXTURE_BLOCK_END

DYNAMIC_SAMPLER_BLOCK_BEGIN
SAMPLER_DYNAMIC(PER_DRAW_BINDINGS_SET, IMAGE_SAMPLER_IDX, imageSampler)
DYNAMIC_SAMPLER_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

#ifndef @RENDER_MODE_MSAA

PLS_BLOCK_BEGIN
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
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

#else // !@RENDER_MODE_MSAA => @RENDER_MODE_MSAA

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

#endif // @RENDER_MODE_MSAA
#endif // FRAGMENT
