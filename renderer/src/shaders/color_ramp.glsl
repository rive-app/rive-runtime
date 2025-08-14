/*
 * Copyright 2022 Rive
 */

// This shader draws horizontal color ramps into a gradient texture, which will
// later be sampled by the renderer for drawing gradients.

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
#ifdef SPLIT_UINT4_ATTRIBUTES
ATTR(0, uint, @a_spanX);
ATTR(1, uint, @a_yWithFlags);
ATTR(2, uint, @a_color0);
ATTR(3, uint, @a_color1);
#else
ATTR(0, uint4, @a_span); // [spanX, y, color0, color1]
#endif
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, half4, v_rampColor);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

half4 unpackColorInt(uint color)
{
    return cast_uint4_to_half4(
               (uint4(color, color, color, color) >> uint4(16, 8, 0, 24)) &
               0xffu) /
           255.;
}

VERTEX_MAIN(@colorRampVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
#ifdef SPLIT_UINT4_ATTRIBUTES
    ATTR_UNPACK(_instanceID, attrs, @a_spanX, uint);
    ATTR_UNPACK(_instanceID, attrs, @a_yWithFlags, uint);
    ATTR_UNPACK(_instanceID, attrs, @a_color0, uint);
    ATTR_UNPACK(_instanceID, attrs, @a_color1, uint);
    uint4 @a_span = uint4(@a_spanX, @a_yWithFlags, @a_color0, @a_color1);
#else
    ATTR_UNPACK(_instanceID, attrs, @a_span, uint4);
#endif
    VARYING_INIT(v_rampColor, half4);

    int columnWithinSpan = _vertexID >> 1;
    float x =
        float(columnWithinSpan <= 1 ? @a_span.x & 0xffffu : @a_span.x >> 16) /
        65536.;
    float offsetY = (_vertexID & 1) == 0 ? .0 : 1.;
    if (uniforms.gradInverseViewportY < .0)
    {
        // Swap the top and bottom vertices to make sure we always emit
        // clockwise triangles. vertices.
        offsetY = 1. - offsetY;
    }
    uint yWithFlags = @a_span.y;
    float y = float(yWithFlags & ~GRAD_SPAN_FLAGS_MASK) + offsetY;
    if ((yWithFlags & GRAD_SPAN_FLAG_LEFT_BORDER) != 0u &&
        columnWithinSpan == 0)
    {
        if ((yWithFlags & GRAD_SPAN_FLAG_COMPLEX_BORDER) != 0u)
            x = .0; // Borders of complex gradients go to the far edge.
        else
            // Simple gradients are empty with 1px borders on either side.
            x -= GRAD_TEXTURE_INVERSE_WIDTH;
    }
    if ((yWithFlags & GRAD_SPAN_FLAG_RIGHT_BORDER) != 0u &&
        columnWithinSpan == 3)
    {
        if ((yWithFlags & GRAD_SPAN_FLAG_COMPLEX_BORDER) != 0u)
            x = 1.; // Borders of complex gradients go to the far edge.
        else
            // Simple gradients are empty with 1px borders on either side.
            x += GRAD_TEXTURE_INVERSE_WIDTH;
    }
    v_rampColor = unpackColorInt(columnWithinSpan <= 1 ? @a_span.z : @a_span.w);

    float4 pos = pixel_coord_to_clip_coord(float2(x, y),
                                           2.,
                                           uniforms.gradInverseViewportY);
#ifdef @POST_INVERT_Y
    pos.y = -pos.y;
#endif

    VARYING_PACK(v_rampColor);
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
FRAG_TEXTURE_BLOCK_END

FRAG_DATA_MAIN(half4, @colorRampFragmentMain)
{
    VARYING_UNPACK(v_rampColor, half4);
    EMIT_FRAG_DATA(v_rampColor);
}
#endif
