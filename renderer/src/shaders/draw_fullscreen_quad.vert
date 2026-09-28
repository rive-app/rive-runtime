/*
 * Copyright 2025 Rive
 */

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
// No attributes: the quad comes from the vertex index.
ATTR_BLOCK_END

VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexIdx, _instanceIdx)
{
    // Fill the entire screen. The caller will use a scissor test to control the
    // bounds being drawn.
    float4 pos;
    pos.x = (_vertexIdx & 1) == 0 ? -1. : 1.;
    pos.y = (_vertexIdx & 2) == 0 ? -1. : 1.;
    pos.z = 0.;
    pos.w = 1.;
    EMIT_VERTEX(pos);
}
#endif
