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

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(half4, v_rampColor);
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
VERTEX_TEXTURE_BLOCK_END

half4 unpackColorInt(uint color)
{
    return make_half4((uint4(color, color, color, color) >> uint4(16, 8, 0, 24)) & 0xffu) / 255.;
}

VERTEX_MAIN(@colorRampVertexMain,
            @Uniforms,
            uniforms,
            Attrs,
            attrs,
            Varyings,
            varyings,
            VertexTextures,
            textures,
            _vertexID,
            _instanceID,
            _pos)
{
    ATTR_UNPACK(_instanceID, attrs, @a_span, uint4);

    VARYING_INIT(varyings, v_rampColor, half4);

    float x = float((_vertexID & 1) == 0 ? @a_span.x & 0xffffu : @a_span.x >> 16) / 65536.;
    float y = float(@a_span.y) + ((_vertexID & 2) == 0 ? .0 : 1.);
    v_rampColor = unpackColorInt((_vertexID & 1) == 0 ? @a_span.z : @a_span.w);
    _pos.x = x * 2. - 1.;
    _pos.y = y * uniforms.gradInverseViewportY - sign(uniforms.gradInverseViewportY);
    _pos.zw = float2(0, 1);

    VARYING_PACK(varyings, v_rampColor);
    EMIT_VERTEX(varyings, _pos);
}
#endif

#ifdef @FRAGMENT
FRAG_DATA_MAIN(half4, @colorRampFragmentMain, Varyings, varyings)
{
    VARYING_UNPACK(varyings, v_rampColor, half4);
    EMIT_FRAG_DATA(v_rampColor);
}
#endif
