/*
 * Copyright 2023 Rive
 */

UNIFORM_BLOCK_BEGIN(IMAGE_DRAW_UNIFORM_BUFFER_IDX, @ImageDrawUniforms)
float4 viewMatrix;
float2 translate;
float opacity;
float padding;
// clipRectInverseMatrix transforms from pixel coordinates to a space where the clipRect is the
// normalized rectangle: [-1, -1, 1, 1].
float4 clipRectInverseMatrix;
float2 clipRectInverseTranslate;
uint clipID;
uint blendMode;
UNIFORM_BLOCK_END(imageDrawUniforms)

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(PositionAttr)
ATTR(0, float2, @a_position);
ATTR_BLOCK_END

ATTR_BLOCK_BEGIN(UVAttr)
ATTR(1, float2, @a_texCoord);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(1, half, v_clipID);
#endif
#ifdef @ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
VERTEX_TEXTURE_BLOCK_END

IMAGE_MESH_VERTEX_MAIN(@drawVertexMain,
                       @Uniforms,
                       uniforms,
                       @ImageDrawUniforms,
                       imageDrawUniforms,
                       PositionAttr,
                       position,
                       UVAttr,
                       uv,
                       Varyings,
                       varyings,
                       _vertexID,
                       _pos)
{
    ATTR_UNPACK(_vertexID, position, @a_position, float2);
    ATTR_UNPACK(_vertexID, uv, @a_texCoord, float2);

    VARYING_INIT(varyings, v_texCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_INIT(varyings, v_clipRect, float4);
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
    _pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(varyings, v_texCoord);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(varyings, v_clipID);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_PACK(varyings, v_clipRect);
#endif
    EMIT_VERTEX(varyings, _pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN(FragmentTextures)
TEXTURE_RGBA8(IMAGE_TEXTURE_IDX, @imageTexture);
FRAG_TEXTURE_BLOCK_END

SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)

PLS_BLOCK_BEGIN
PLS_DECL4F(FRAMEBUFFER_PLANE_IDX, framebuffer);
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
PLS_DECL4F(ORIGINAL_DST_COLOR_PLANE_IDX, originalDstColorBuffer);
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
PLS_BLOCK_END

IMAGE_DRAW_PLS_MAIN(@drawFragmentMain,
                    @ImageDrawUniforms,
                    imageDrawUniforms,
                    Varyings,
                    varyings,
                    FragmentTextures,
                    textures,
                    _pos,
                    _plsCoord)
{
    VARYING_UNPACK(varyings, v_texCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_CLIP_RECT
    VARYING_UNPACK(varyings, v_clipRect, float4);
#endif

    half4 color = TEXTURE_DEREF_SAMPLE(textures, @imageTexture, imageSampler, v_texCoord);
    half coverage = 1.;

#ifdef @ENABLE_CLIP_RECT
    half clipRectCoverage = min_value(make_half4(v_clipRect));
    coverage = clamp(clipRectCoverage, make_half(0), coverage);
#endif

    PLS_INTERLOCK_BEGIN;

#ifdef @ENABLE_CLIPPING
    if (v_clipID != .0)
    {
        half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer, _plsCoord));
        half clipContentID = clipData.g;
        half clipCoverage = clipContentID == v_clipID ? clipData.r : make_half(0);
        coverage = min(coverage, clipCoverage);
    }
#endif

    // Blend with the framebuffer color.
    color.a *= imageDrawUniforms.opacity * coverage;
    half4 dstColor = PLS_LOAD4F(framebuffer, _plsCoord);
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

    PLS_STORE4F(framebuffer, color, _plsCoord);
    PLS_PRESERVE_VALUE(coverageCountBuffer, _plsCoord);
    PLS_PRESERVE_VALUE(clipBuffer, _plsCoord);

    PLS_INTERLOCK_END;

    EMIT_PLS;
}
#endif
