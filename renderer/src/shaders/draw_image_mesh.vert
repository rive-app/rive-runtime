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

ATTR_BLOCK_BEGIN(ImageDrawAttrs)
ATTR(IMAGE_VIEW_MATRIX_ATTRIB_IDX, float4, @a_imageDrawViewMatrix);
ATTR(IMAGE_CLIP_RECT_INVERSE_MATRIX_ATTRIB_IDX,
     float4,
     @a_imageDrawClipRectInverseMatrix);
ATTR(IMAGE_TRANSLATES_ATTRIB_IDX, float4, @a_imageDrawTranslates);
ATTR(IMAGE_OPACITY_ATTRIB_IDX, float, @a_imageDrawOpacity);
ATTR(IMAGE_CLIP_ID_ATTRIB_IDX, uint, @a_imageDrawClipID);
ATTR(IMAGE_BLEND_MODE_ATTRIB_IDX, uint, @a_imageDrawBlendMode);
ATTR(IMAGE_ZINDEX_ATTRIB_IDX, uint, @a_imageDrawZIndex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_imageTexCoord);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(1, half, v_clipID);
#endif
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_DEPTH_STENCIL)
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
@OPTIONALLY_FLAT VARYING(3, half, v_imageOpacity);
#ifdef @ENABLE_ADVANCED_BLEND
FLAT VARYING(4, ushort, v_imageBlendMode);
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
                       ImageDrawAttrs,
                       imageDrawAttrs,
                       _vertexID)
{
    ATTR_UNPACK(_vertexID, position, @a_position, float2);
    ATTR_UNPACK(_vertexID, uv, @a_texCoord, float2);
    ATTR_UNPACK(_instanceID, imageDrawAttrs, @a_imageDrawViewMatrix, float4);
    ATTR_UNPACK(_instanceID,
                imageDrawAttrs,
                @a_imageDrawClipRectInverseMatrix,
                float4);
    ATTR_UNPACK(_instanceID, imageDrawAttrs, @a_imageDrawTranslates, float4);
    ATTR_UNPACK(_instanceID, imageDrawAttrs, @a_imageDrawOpacity, float);
    ATTR_UNPACK(_instanceID, imageDrawAttrs, @a_imageDrawClipID, uint);
    ATTR_UNPACK(_instanceID, imageDrawAttrs, @a_imageDrawBlendMode, uint);
    ATTR_UNPACK(_instanceID, imageDrawAttrs, @a_imageDrawZIndex, uint);

    VARYING_INIT(v_imageTexCoord, float2);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(v_clipID, half);
#endif
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_DEPTH_STENCIL)
    VARYING_INIT(v_clipRect, float4);
#endif
    VARYING_INIT(v_imageOpacity, half);
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_INIT(v_imageBlendMode, ushort);
#endif

    float2 vertexPosition =
        MUL(make_float2x2(@a_imageDrawViewMatrix), @a_position) +
        @a_imageDrawTranslates.xy;
    v_imageTexCoord = @a_texCoord;
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        v_clipID =
            id_bits_to_f16(@a_imageDrawClipID, uniforms.pathIDGranularity);
    }
#endif
#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
#ifndef @RENDER_MODE_DEPTH_STENCIL
        v_clipRect = find_clip_rect_coverage_distances(
            make_float2x2(@a_imageDrawClipRectInverseMatrix),
            @a_imageDrawTranslates.zw,
            vertexPosition CLIP_CONTEXT_UNPACK);
#else
        set_clip_rect_plane_distances(
            make_float2x2(@a_imageDrawClipRectInverseMatrix),
            @a_imageDrawTranslates.zw,
            vertexPosition CLIP_CONTEXT_UNPACK);
#endif
    }
#endif // ENABLE_CLIP_RECT
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
#ifdef @POST_INVERT_Y
    pos.y = -pos.y;
#endif
#ifdef @RENDER_MODE_DEPTH_STENCIL
    pos.z = normalize_z_index(@a_imageDrawZIndex);
#endif

    v_imageOpacity = @a_imageDrawOpacity;
#ifdef @ENABLE_ADVANCED_BLEND
    v_imageBlendMode = cast_uint_to_ushort(@a_imageDrawBlendMode);
#endif

    VARYING_PACK(v_imageTexCoord);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(v_clipID);
#endif
#if defined(@ENABLE_CLIP_RECT) && !defined(@RENDER_MODE_DEPTH_STENCIL)
    VARYING_PACK(v_clipRect);
#endif
    VARYING_PACK(v_imageOpacity);
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_PACK(v_imageBlendMode);
#endif
    EMIT_VERTEX(pos);
}
#endif
