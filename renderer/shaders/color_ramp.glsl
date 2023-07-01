/*
 * Copyright 2022 Rive
 */

// This shader draws horizontal color ramps into a gradient texture, which will later be sampled by
// the renderer for drawing gradients.

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING half4 rampColor;
VARYING_BLOCK_END

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0) uint4 span; // [horizontalSpan, y, color0, color1]
ATTR_BLOCK_END

half4 unpackColorInt(uint color)
{
    return make_half4((uint4(color) >> uint4(16, 8, 0, 24)) & 0xffu) / 255.;
}

VERTEX_MAIN(
#ifdef METAL
    @colorRampVertexMain,
    Varyings,
    varyings,
    uint VERTEX_ID [[vertex_id]],
    uint INSTANCE_ID [[instance_id]],
    constant @Uniforms& uniforms [[buffer(0)]],
    constant Attrs* attrs [[buffer(1)]]
#endif
)
{
    ATTR_LOAD(uint4, attrs, span, INSTANCE_ID);
    uint horizontalSpan = span.x;
    float2 coord = float2(
        float((VERTEX_ID & 1) == 0 ? horizontalSpan & 0xffffu : horizontalSpan >> 16) / 65536.,
        float(span.y) + ((VERTEX_ID & 2) == 0 ? .0 : 1.));
    FLD(varyings, rampColor) = unpackColorInt((VERTEX_ID & 1) == 0 ? span.z : span.w);
    POSITION.xy = coord * float2(2, uniforms.gradInverseViewportY) - 1.;
    POSITION.zw = float2(0, 1);
    EMIT_OFFSCREEN_VERTEX(varyings);
}
#endif

#ifdef @FRAGMENT
FRAG_DATA_TYPE(half4)
FRAG_DATA_MAIN(
#ifdef METAL
    @colorRampFragmentMain,
    Varyings varyings [[stage_in]]
#endif
)
{
    EMIT_FRAG_DATA(FLD(varyings, rampColor));
}
#endif
