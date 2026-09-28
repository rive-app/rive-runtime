/*
 * Copyright 2026 Rive
 */

// Vertex shader for depthStencil fills.
//
// Fills never outset in depthStencil mode: a fill vertex is just a literal
// tessellation vertex or contour midpoint. Therefore, these draws don't use
// input attribs or instancing. The patch topology lives in a flat, repeating
// index buffer (See gpu::generateDepthStencilFillIndices()), and each
// invocation derives its attributes from gl_VertexIndex.
//
// For now, strokes stay on DrawType::depthStrokes and draw_path.vert.

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
// No attributes: everything comes from the vertex index.
ATTR_BLOCK_END
#endif

// Locations must match draw_path.vert; the fragment stage is
// draw_depthstencil_object.frag, which is built against that file's varyings.
VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_paint);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(4, half2, v_clipIDs); // [clipID, outerClipID]
#endif
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(6, half, v_blendMode);
#endif
#ifdef @ENABLE_MODULATED_IMAGE
NO_PERSPECTIVE VARYING(9, float3, v_image);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX

VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexIdx, _instanceIdx)
{
    VARYING_INIT(v_paint, float4);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(v_clipIDs, half2);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_INIT(v_blendMode, half);
#endif
#ifdef @ENABLE_MODULATED_IMAGE
    VARYING_INIT(v_image, float3);
#endif

    // In order to avoid push constants, Rive smuggles in flags via vertex ID.
    // This works because there are no actual vertex attrib buffers.
    bool isOuterCubicPatch = (_vertexIdx & VERTEX_FLAG_OUTER_CUBIC) != 0;
    bool colorWriteDisabled =
        (_vertexIdx & VERTEX_FLAG_DISABLE_COLOR_WRITE) != 0;
    int vertexIdxNoFlags = _vertexIdx & ((1 << VERTEX_FLAGS_SHIFT) - 1);

    // The index buffers align per-patch vertex IDs on pow2 strides,
    // specifically so this shader can decode _vertexIdx without divides and
    // mods. (Using integer division costs 10% total frame perf on PowerVR and
    // 3% on Adreno.)
    // NOTE: The patches don't actually have pow2 numbers of vertices; the index
    // buffers just never reference those vertex IDs between the end of one
    // patch and the beginning of another.
    int patchStrideLog2 = DS_PATCH_STRIDE_LOG2(isOuterCubicPatch);
    int patchIdx = vertexIdxNoFlags >> patchStrideLog2;
    int patchVertexID = vertexIdxNoFlags & ((1 << patchStrideLog2) - 1);
    int patchTessVertexStride =
        isOuterCubicPatch
            // outerCubics tessellate a redundant bowtie vertex at the end of
            // every patch (for AA), which is never referenced by the
            // depthStencil fill index buffers. But we still need to stride over
            // it when indexing into the tessellation texture.
            ? int(OUTER_CUBIC_PATCH_SEGMENT_SPAN_PLUS_BOWTIE)
            : int(MIDPOINT_FAN_PATCH_SEGMENT_SPAN);
    bool isMidpointVertex =
        !isOuterCubicPatch && patchVertexID == DS_MIDPOINT_VERTEX_ID;
    int localVertexID = isMidpointVertex ? 0 : patchVertexID;

    // Fetch a vertex that definitely belongs to the contour we're drawing.
    int vertexIDOnContour = min(localVertexID, patchTessVertexStride - 1);
    int tessVertexIdx = patchIdx * patchTessVertexStride + vertexIDOnContour;
    uint4 tessVertexData =
        TEXEL_FETCH(@tessVertexTexture, tess_texel_coord(tessVertexIdx));
    uint contourIDWithFlags = tessVertexData.w;

    // Fetch and unpack the contour referenced by the tessellation vertex.
    // NOTE: The contourID is guaranteed to be >= 1 at this point, but clamp it
    // anyway because in the event of a bug, a buffer load at index "0u - 1" can
    // be very serious and hard to catch.
    uint contourID = max(contourIDWithFlags & CONTOUR_ID_MASK, 1u);
    uint4 contourData = STORAGE_BUFFER_LOAD4(@contourBuffer, contourID - 1u);
    float2 midpoint = uintBitsToFloat(contourData.xy);
    uint pathID = contourData.z & 0xffffu;
    uint vertexIndex0 = contourData.w;

    // Fetch and unpack the path.
    float2x2 M = make_float2x2(
        uintBitsToFloat(STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * 4u)));
    uint4 pathData = STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * 4u + 1u);
    float2 translate = uintBitsToFloat(pathData.xy);

    // Fix the tessellation vertex if we fetched the wrong one in order to
    // guarantee we got the correct contour ID and flags, or if we belong to a
    // mirrored contour and this vertex has an alternate position when mirrored.
    // (Only midpoint-fan fan columns have one; the patch buffer gave outer
    // cubics and the midpoint the same vertex either way.)
    uint mirroredContourFlag =
        contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG;
    if (mirroredContourFlag != 0u && !isOuterCubicPatch && !isMidpointVertex)
    {
        localVertexID = localVertexID - 1;
    }
    if (localVertexID != vertexIDOnContour)
    {
        // This can peek one vertex before or after the contour, but the
        // tessellator guarantees there is always at least one padding vertex at
        // the beginning and end of the data.
        int replacementTessVertexIdx =
            tessVertexIdx + localVertexID - vertexIDOnContour;
        uint4 replacementTessVertexData =
            TEXEL_FETCH(@tessVertexTexture,
                        tess_texel_coord(replacementTessVertexIdx));
        if ((replacementTessVertexData.w &
             (MIRRORED_CONTOUR_CONTOUR_FLAG | 0xffffu)) !=
            (contourIDWithFlags & (MIRRORED_CONTOUR_CONTOUR_FLAG | 0xffffu)))
        {
            // We crossed over into a new contour. Fills are always closed, so
            // wrap to the first vertex in the contour.
            tessVertexData = TEXEL_FETCH(@tessVertexTexture,
                                         tess_texel_coord(int(vertexIndex0)));
        }
        else
        {
            tessVertexData = replacementTessVertexData;
        }
        // MIRRORED_CONTOUR_CONTOUR_FLAG is not preserved at vertexIndex0.
        // Preserve it here. By not preserving this flag, the normal and
        // mirrored contour can both share the same contour record.
        contourIDWithFlags =
            (tessVertexData.w & ~MIRRORED_CONTOUR_CONTOUR_FLAG) |
            mirroredContourFlag;
    }

    float2 origin =
        isMidpointVertex ? midpoint : uintBitsToFloat(tessVertexData.xy);
    float2 vertexPosition = MUL(M, origin) + translate;

    uint2 paintData = STORAGE_BUFFER_LOAD2(@paintBuffer, pathID);
    uint paintType = paintData.x & 0xfu;
#ifdef @ENABLE_CLIPPING
    if (@ENABLE_CLIPPING)
    {
        uint clipIDBits =
            (paintType == CLIP_UPDATE_PAINT_TYPE ? paintData.y : paintData.x) >>
            16;
        half clipID = id_bits_to_f16(clipIDBits, uniforms.pathIDGranularity);
        // Negative clipID means to update the clip buffer instead of the color
        // buffer.
        if (paintType == CLIP_UPDATE_PAINT_TYPE)
            clipID = -clipID;
        v_clipIDs.x = clipID;
    }
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
        v_blendMode = float((paintData.x >> 4) & 0xfu);
    }
#endif

    // Paint matrices operate on the fragment shader's "_fragCoord", which
    // counts from memory row 0. A bottom up target needs it flipped into Rive
    // pixel space.
    float2 fragCoord = vertexPosition;
#ifdef @ENABLE_RENDER_TARGET_BOTTOM_UP
    if (uniforms.renderTargetBottomUp != 0u)
    {
        fragCoord.y = float(uniforms.renderTargetHeight) - fragCoord.y;
    }
#endif

#ifdef @ENABLE_CLIP_RECT
    if (@ENABLE_CLIP_RECT)
    {
        // clipRectInverseMatrix transforms from pixel coordinates to a space
        // where the clipRect is the normalized rectangle: [-1, -1, 1, 1].
        float2x2 clipRectInverseMatrix = make_float2x2(
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer,
                                 pathID * PAINT_AUX_ENTRY_ELEMENT_COUNT + 2u));
        float4 clipRectInverseTranslate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer,
                                 pathID * PAINT_AUX_ENTRY_ELEMENT_COUNT + 3u);
        set_clip_rect_plane_distances(clipRectInverseMatrix,
                                      clipRectInverseTranslate.xy,
                                      fragCoord CLIP_CONTEXT_UNPACK);
    }
#endif // ENABLE_CLIP_RECT

    // Unpack the paint once we have a position.
    if (paintType == SOLID_COLOR_PAINT_TYPE)
    {
        v_paint = float4(unpackUnorm4x8(paintData.y));
    }
#ifdef @ENABLE_CLIPPING
    else if (@ENABLE_CLIPPING && paintType == CLIP_UPDATE_PAINT_TYPE)
    {
        half outerClipID =
            id_bits_to_f16(paintData.x >> 16, uniforms.pathIDGranularity);
        v_clipIDs.y = outerClipID;
    }
#endif
    else
    {
        float2x2 paintMatrix = make_float2x2(
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer,
                                 pathID * PAINT_AUX_ENTRY_ELEMENT_COUNT));
        float4 paintTranslate =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer,
                                 pathID * PAINT_AUX_ENTRY_ELEMENT_COUNT + 1u);

        // paintData.y (gradient texture row + 1) in the integer part
        // additiveness in range 0/256 to 255/256 in the fraction.
        v_paint = packGradientData(fragCoord,
                                   paintMatrix,
                                   paintTranslate.xy,
                                   float(paintType),
                                   paintTranslate.zw,
                                   uintBitsToFloat(paintData.y));

        // Make this negative to signal to the fragment shader that it's a
        // gradient
        v_paint.a = -v_paint.a;
    }
    if (colorWriteDisabled)
    {
        // Zeroing v_paint is all we need to disable color write; float4(0) gets
        // interpreted by the fragment shader as a fully transparent
        // SOLID_COLOR_PAINT_TYPE, and then discarded at the blend step.
        v_paint = float4(.0, .0, .0, .0);
    }

#ifdef @ENABLE_MODULATED_IMAGE
    if (@ENABLE_MODULATED_IMAGE && (paintData.x & PAINT_FLAG_HAS_IMAGE) != 0u)
    {
        float2x2 imageMatrix = make_float2x2(
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer,
                                 pathID * PAINT_AUX_ENTRY_ELEMENT_COUNT + 4u));
        float4 paintTranslateAndLOD =
            STORAGE_BUFFER_LOAD4(@paintAuxBuffer,
                                 pathID * PAINT_AUX_ENTRY_ELEMENT_COUNT + 5u);
        float2 imageCoord =
            MUL(imageMatrix, fragCoord) + paintTranslateAndLOD.xy;

        // Add 1 to the LOD because a z value of 0 means "we don't have an
        // image"
        v_image =
            float3(imageCoord.x, imageCoord.y, 1. + paintTranslateAndLOD.z);
    }
    else
    {
        v_image = float3(0.0, 0.0, 0.0);
    }
#endif

    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
#ifdef @POST_INVERT_Y
    pos.y = -pos.y;
#endif
    uint4 pathData2 = STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * 4u + 2u);
    pos.z = packNormalizedDepth(cast_uint_to_ushort(pathData2.x), 0xffu);

    VARYING_PACK(v_paint);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(v_clipIDs);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_PACK(v_blendMode);
#endif
#ifdef @ENABLE_MODULATED_IMAGE
    VARYING_PACK(v_image);
#endif
    EMIT_VERTEX(pos);
}
#endif // @VERTEX
