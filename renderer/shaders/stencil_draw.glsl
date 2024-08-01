/*
 * Copyright 2024 Rive
 */

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, packed_float3, @a_triangleVertex);
ATTR_BLOCK_END

VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

VERTEX_MAIN(@stencilVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(@a_triangleVertex.xy);
    uint zIndex = floatBitsToUint(@a_triangleVertex.z) & 0xffffu;
    pos.z = normalize_z_index(zIndex);
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
FRAG_TEXTURE_BLOCK_END

FRAG_DATA_MAIN(half4, @blitFragmentMain) { EMIT_FRAG_DATA(make_half4(.0)); }
#endif // FRAGMENT
