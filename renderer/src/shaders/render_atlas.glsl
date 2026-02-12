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
VERTEX_MAIN(@atlasVertexMain, Attrs, attrs, _vertexID, _instanceID)
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
        float3 atlasTransform = uintBitsToFloat(pathData2.yzw);
        vertexPosition = vertexPosition * atlasTransform.x + atlasTransform.yz;

        pos = pixel_coord_to_clip_coord(vertexPosition,
                                        uniforms.atlasContentInverseViewport.x,
                                        uniforms.atlasContentInverseViewport.y);
#ifdef @POST_INVERT_Y
        pos.y = -pos.y;
#endif
    }
    else
    {
        pos = float4(uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue);
    }

    VARYING_PACK(v_coverages);
    EMIT_VERTEX(pos);
}
#endif // @VERTEX

#ifdef @FRAGMENT

#ifdef @ATLAS_FEATHERED_FILL
INLINE half signed_fill_coverage(float4 coverages,
                                 bool clockwise TEXTURE_CONTEXT_DECL)
{
    half coverage = eval_feathered_fill(coverages TEXTURE_CONTEXT_FORWARD);
    if (!clockwise)
        coverage = -coverage;
    return coverage;
}
#endif

#ifdef @ATLAS_RENDER_TARGET_R32UI_FRAMEBUFFER_FETCH

// Store coverage as fp32 data bits in an r32ui color buffer, and use
// framebuffer-fetch to manipulate it.
layout(location = 0) inout highp uvec4 _fragCoverage;

#ifdef @ATLAS_FEATHERED_FILL
void main()
{
    float coverage = uintBitsToFloat(_fragCoverage.r);
    coverage += signed_fill_coverage(v_coverages,
                                     gl_FrontFacing TEXTURE_CONTEXT_FORWARD);
    _fragCoverage.r = floatBitsToUint(coverage);
}
#endif

#ifdef @ATLAS_FEATHERED_STROKE
void main()
{
    float coverage = uintBitsToFloat(_fragCoverage.r);
    coverage = max(coverage, eval_feathered_stroke(v_coverages));
    _fragCoverage.r = floatBitsToUint(coverage);
}
#endif

#elif defined(@ATLAS_RENDER_TARGET_R32UI_PLS_EXT)

// Manipulate fp32 coverage in pixel local storage, which will be written out
// to an r32ui color buffer during a separate resolve step.
__pixel_localEXT PLS { layout(r32f) highp float _plsCoverage; };

#ifdef @ATLAS_FEATHERED_FILL
void main()
{
    _plsCoverage +=
        signed_fill_coverage(v_coverages,
                             gl_FrontFacing TEXTURE_CONTEXT_FORWARD);
}
#endif

#ifdef @ATLAS_FEATHERED_STROKE
void main()
{
    _plsCoverage = max(_plsCoverage, eval_feathered_stroke(v_coverages));
}
#endif

#elif defined(@ATLAS_RENDER_TARGET_R32UI_PLS_ANGLE)

// Store and manipulate coverage as fp32 data bits in r32ui-texture-backed pixel
// local storage.
layout(binding = 0, r32ui) uniform highp upixelLocalANGLE _plsCoverage;

#ifdef @ATLAS_FEATHERED_FILL
void main()
{
    float coverage = uintBitsToFloat(pixelLocalLoadANGLE(_plsCoverage).r);
    coverage += signed_fill_coverage(v_coverages,
                                     gl_FrontFacing TEXTURE_CONTEXT_FORWARD);
    pixelLocalStoreANGLE(_plsCoverage, uint4(floatBitsToUint(coverage)));
}
#endif

#ifdef @ATLAS_FEATHERED_STROKE
void main()
{
    float coverage = uintBitsToFloat(pixelLocalLoadANGLE(_plsCoverage).r);
    coverage = max(coverage, eval_feathered_stroke(v_coverages));
    pixelLocalStoreANGLE(_plsCoverage, uint4(floatBitsToUint(coverage)));
}
#endif

#elif defined(@ATLAS_RENDER_TARGET_R32I_ATOMIC_TEXTURE)

// Store coverage as 16:16 fixed point in an r32i texture, which we manipulate
// with atomics.
layout(binding = 0, r32i) uniform highp coherent iimage2D _atlasImage;
ivec2 image_coord() { return ivec2(floor(_fragCoord)); }
int fixedpoint_coverage(float coverage)
{
    return int(coverage * ATLAS_R32I_FIXED_POINT_FACTOR);
}

#ifdef @ATLAS_FEATHERED_FILL
void main()
{
    int coverage = fixedpoint_coverage(
        signed_fill_coverage(v_coverages,
                             gl_FrontFacing TEXTURE_CONTEXT_FORWARD));
    imageAtomicAdd(_atlasImage, image_coord(), coverage);
}
#endif

#ifdef @ATLAS_FEATHERED_STROKE
void main()
{
    int coverage = fixedpoint_coverage(eval_feathered_stroke(v_coverages));
    imageAtomicMax(_atlasImage, image_coord(), coverage);
}
#endif

#elif defined(@ATLAS_RENDER_TARGET_RGBA8_UNORM)

// We don't have any extensions to count high precision coverage. (This is very
// rare.). Just split up coverage across rgba8 components and hope for the best.

#ifdef @ATLAS_FEATHERED_FILL
FRAG_DATA_MAIN_WITH_CLOCKWISE(half4, @atlasFillFragmentMain)
{
    VARYING_UNPACK(v_coverages, float4);
    half coverage =
        signed_fill_coverage(v_coverages, _clockwise TEXTURE_CONTEXT_FORWARD);
    // i.e., is abs(coverage) ~= FEATHER(1), allowing for some sub-8-bit slop in
    // the texture unit performing a clamp to edge.
    if (abs(coverage) > MAX_FEATHER - 1e-3)
    {
        // All the "fan triangles" in a feather have solid coverage. This is a
        // substantial number of triangles, so we dedicate 2 channels to
        // counting solid coverage (i.e, +1 or -1). These channels are also much
        // slower to overflow, so it preserves a basic skeleton of the feather
        // when the fractional channels overflow.
        EMIT_FRAG_DATA(coverage > .0
                           // B counts integer, positive coverage.
                           ? make_half4(.0, .0, 1. / 255., .0)
                           // A counts integer, negative coverage.
                           : make_half4(.0, .0, .0, 1. / 255.));
    }
    else
    {
        coverage *= 1. / ATLAS_UNORM8_COVERAGE_SCALE_FACTOR;
        EMIT_FRAG_DATA(make_half4(
            max(coverage, .0),  // R counts fractional, positive coverage.
            max(-coverage, .0), // G counts fractional, negative coverage.
            .0,
            .0));
    }
}
#endif // @ATLAS_FEATHERED_FILL

#ifdef @ATLAS_FEATHERED_STROKE
FRAG_DATA_MAIN(half4, @atlasStrokeFragmentMain)
{
    VARYING_UNPACK(v_coverages, float4);
    half coverage = eval_feathered_stroke(v_coverages TEXTURE_CONTEXT_FORWARD);
    // Strokes only have positive coverage, and since we only need to saturate
    // the max for stroking, we can just use the R channel.
    coverage *= 1. / ATLAS_UNORM8_COVERAGE_SCALE_FACTOR;
    EMIT_FRAG_DATA(make_half4(coverage, .0, .0, .0));
}
#endif // @ATLAS_FEATHERED_STROKE

#else

// This is the ideal case. We have full support for floating point color
// buffers, including blending. Render to float and let the fixed function blend
// hardware count the coverage.

#ifdef @ATLAS_FEATHERED_FILL
FRAG_DATA_MAIN_WITH_CLOCKWISE(float, @atlasFillFragmentMain)
{
    VARYING_UNPACK(v_coverages, float4);
    EMIT_FRAG_DATA(
        signed_fill_coverage(v_coverages, _clockwise TEXTURE_CONTEXT_FORWARD));
}
#endif

#ifdef @ATLAS_FEATHERED_STROKE
FRAG_DATA_MAIN(float, @atlasStrokeFragmentMain)
{
    VARYING_UNPACK(v_coverages, float4);
    EMIT_FRAG_DATA(eval_feathered_stroke(v_coverages TEXTURE_CONTEXT_FORWARD));
}
#endif

#endif

#endif // FRAGMENT
