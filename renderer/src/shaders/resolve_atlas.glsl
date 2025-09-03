/*
 * Copyright 2025 Rive
 */

// The EXT_shader_pixel_local_storage extension does not provide a mechanism to
// load, store, or clear pixel local storage contents. This shader provides
// mechanisms to clear and store when rendering the atlas via PLS (i.e., when
// floating point render targets are not supported).

// TODO: We should eventually use this shader to convert the atlas from gamma to
// linear as well, so we don't have to do that conversion for 4 different pixels
// every time we sample and bilerp the atlas.

#ifdef @VERTEX
VERTEX_MAIN(@atlasResolveVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    // [-1, -1] .. [+1, +1]
    float4 pos = float4(mix(float2(-1, 1),
                            float2(1, -1),
                            equal(_vertexID & int2(1, 2), int2(0))),
                        .0,
                        1.);

    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT

#ifdef @CLEAR_COVERAGE
__pixel_local_outEXT PLS { layout(r32f) highp float _plsCoverage; };
#else
__pixel_local_inEXT PLS { layout(r32f) highp float _plsCoverage; };
layout(location = 0) out highp uvec4 _fragCoverage;
#endif

void main()
{
#ifdef @CLEAR_COVERAGE
    _plsCoverage = .0;
#else
    _fragCoverage.r = floatBitsToUint(_plsCoverage);
#endif
}

#endif // FRAGMENT
