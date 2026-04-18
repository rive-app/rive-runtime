/*
 * Copyright 2026 Rive
 */

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, packed_float3, @a_triangleVertex);
ATTR_BLOCK_END

VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, packed_float3);
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(@a_triangleVertex.xy);
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
PLS_BLOCK_BEGIN
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
PLS_DECL4F(CLIP_PLANE_IDX, clipBuffer);
PLS_BLOCK_END

CLOCKWISE_ATOMIC_PLS_MAIN(@drawFragmentMain)
{
    // srcOver blend is enabled: emit an alpha value of 1 to overwrite the
    // existing clip.
    PLS_STORE4F(clipBuffer, make_half4(.0, .0, .0, 1.));

    // srcOver blend is enabled: emit a color of 0 to make sure the framebuffer
    // remains unchanged.
    EMIT_CLOCKWISE_ATOMIC_PLS(make_half4(.0));
}
#endif // FRAGMENT
