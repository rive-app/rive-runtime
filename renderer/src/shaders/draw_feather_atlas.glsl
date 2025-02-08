/*
 * Copyright 2025 Rive
 */

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
// [localVertexID, outset, fillCoverage, vertexType]
ATTR(0, float4, @a_patchVertexData);
ATTR(1, float4, @a_mirroredVertexData);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_coverages);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);

    VARYING_INIT(v_coverages, float4);

    float4 pos;
    uint pathID;
    float2 vertexPosition;
    if (unpack_tessellated_path_vertex(@a_patchVertexData,
                                       @a_mirroredVertexData,
                                       _instanceID,
                                       pathID,
                                       vertexPosition,
                                       v_coverages VERTEX_CONTEXT_UNPACK))
    {
        // Offset from on-screen coordinates to atlas coordinates.
        uint4 pathData2 = STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * 4u + 2u);
        float2 screenToAtlasOffset =
            float2(int2(pathData2.y << 16, pathData2.y) >> 16);
        vertexPosition += screenToAtlasOffset;

        pos = pixel_coord_to_clip_coord(vertexPosition,
                                        uniforms.atlasContentInverseViewport.x,
                                        uniforms.atlasContentInverseViewport.y);
    }
    else
    {
        pos = float4(uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue);
    }

    VARYING_PACK(v_coverageCoord);
    EMIT_VERTEX(pos);
}
#endif // @VERTEX

#ifdef @FRAGMENT
FRAG_DATA_MAIN(half4, @drawFragmentMain)
{
    VARYING_UNPACK(v_coverages, float4);

    half coverage;
#ifdef @ATLAS_DRAW_FEATHERED_STROKE
    coverage =
        eval_feathered_stroke(v_coverages,
                              SAMPLED_R16F(@featherTexture, featherSampler));
#else
    coverage =
        eval_feathered_fill(v_coverages,
                            SAMPLED_R16F(@featherTexture, featherSampler));
#endif

    EMIT_FRAG_DATA(make_half4(coverage));
}
#endif // FRAGMENT
