/*
 * Copyright 2024 Rive
 */

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

VERTEX_MAIN(@blitVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    // Fill the entire screen. The caller will use a scissor test to control the bounds being drawn.
    float2 coord;
    coord.x = (_vertexID & 1) == 0 ? -1. : 1.;
    coord.y = (_vertexID & 2) == 0 ? -1. : 1.;
    float4 pos = float4(coord, 0, 1);
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, 0, @blitTextureSource);
FRAG_TEXTURE_BLOCK_END

FRAG_DATA_MAIN(half4, @blitFragmentMain)
{
    half4 srcColor = TEXEL_FETCH(@blitTextureSource, int2(floor(_fragCoord.xy)));
    EMIT_FRAG_DATA(srcColor);
}
#endif // FRAGMENT
