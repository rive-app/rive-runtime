/*
 * Copyright 2022 Rive
 */

// This shader draws horizontal color ramps into a gradient texture, which will later be sampled by
// the renderer for drawing gradients.

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, uint4, @a_span); // [spanX, y, color0, color1]
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
    return cast_uint4_to_half4((uint4(color, color, color, color) >> uint4(16, 8, 0, 24)) & 0xffu) /
           255.;
}

VERTEX_MAIN(@colorRampVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_instanceID, attrs, @a_span, uint4);

    VARYING_INIT(v_rampColor, half4);

    float x = float((_vertexID & 1) == 0 ? @a_span.x & 0xffffu : @a_span.x >> 16) / 65536.;
    float offsetY = (_vertexID & 2) == 0 ? 1. : .0;
    if (uniforms.gradInverseViewportY < .0)
    {
        // Make sure we always emit clockwise triangles. Swap the top and bottom vertices.
        offsetY = 1. - offsetY;
    }
    v_rampColor = unpackColorInt((_vertexID & 1) == 0 ? @a_span.z : @a_span.w);

    float4 pos;
    pos.x = x * 2. - 1.;
    pos.y = (float(@a_span.y) + offsetY) * uniforms.gradInverseViewportY -
            sign(uniforms.gradInverseViewportY);
    pos.zw = float2(0, 1);

    VARYING_PACK(v_rampColor);
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_DATA_MAIN(half4, @colorRampFragmentMain)
{
    VARYING_UNPACK(v_rampColor, half4);
    EMIT_FRAG_DATA(v_rampColor);
}
#endif
