/*
 * Copyright 2026 Rive
 */

// Defined as a function because Naga doesn't accept
// "@DS_POLAR_STROKE || @DS_HAIRLINE_STROKE".
INLINE bool isStroke()
{
#ifdef @DS_POLAR_STROKE
  if (@DS_POLAR_STROKE)
    return true;
#endif
#ifdef @DS_HAIRLINE_STROKE
  if (@DS_HAIRLINE_STROKE)
    return true;
#endif
  return false;
}

// Defined as a function because Naga doesn't accept any operation on a
// specialization constant, including "!@DS_POLAR_STROKE".
INLINE bool isContourClosed(uint contourFlags)
{
  // Fills and hairlines are always closed, and strokes encode whether they are
  // closed in their flags.
#ifdef @DS_POLAR_STROKE
  if (@DS_POLAR_STROKE)
    return (contourFlags & CONTOUR_FLAG_CLOSED) != 0u;
#endif
  return true;
}

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
// No attributes: everything comes from the vertex index.
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_paint);
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(1, half, v_blendMode);
#endif
#ifdef @ENABLE_MODULATED_IMAGE
NO_PERSPECTIVE VARYING(2, float3, v_image);
#endif
VARYING_BLOCK_END

#ifdef @VERTEX

VERTEX_MAIN(@drawVertexMain, Attrs, attrs, _vertexIdx, _instanceIdx)
{
  VARYING_INIT(v_paint, float4);
  #ifdef @ENABLE_ADVANCED_BLEND
  VARYING_INIT(v_blendMode, half);
  #endif
  #ifdef @ENABLE_MODULATED_IMAGE
  VARYING_INIT(v_image, float3);
  #endif

  #ifdef @DS_POLAR_STROKE
  const bool isPolarStroke = @DS_POLAR_STROKE;
  #else
  const bool isPolarStroke = false;
  #endif

  #ifdef @DS_HAIRLINE_STROKE
  const bool isHairlineStroke = @DS_HAIRLINE_STROKE;
  #else
  const bool isHairlineStroke = false;
  #endif

  int vertexIdx = _vertexIdx;

  // Since there are no actual vertex attrib buffers, Rive smuggles in flags
  // via vertex ID. This proves to be a very efficient way of drawing
  // different variations of things without changing pipeline state or push
  // constants.
  //
  // These flags are always valid in virtual vertexIDs as well.
  // (i.e., for draws that use the virtual patch buffer.)
  //
  // NOTE: spirv-opt verifiably folds isOuterCubicPatch and isAAPolarStroke
  // into a single value because VERTEX_FLAG_OUTER_CUBIC_FILL and
  // VERTEX_FLAG_AA_POLAR_STROKE are equal aliases.
  bool isOuterCubicPatch = (vertexIdx & VERTEX_FLAG_OUTER_CUBIC_FILL) != 0;
  bool isAAPolarStroke = (vertexIdx & VERTEX_FLAG_AA_POLAR_STROKE) != 0;

  int patchStrideLog2;
  #if defined(@DS_POLAR_STROKE) || defined(@DS_HAIRLINE_STROKE)
  if (isStroke())
  {
    // Strokes (and hairlines) get their baseVertex indirectly via the virtual
    // patch buffer. This indirection allows us to batch multiple overlapping
    // ranges and subpass flags together in a single draw command.
    patchStrideLog2 = DS_STROKE_PATCH_STRIDE_LOG2(isAAPolarStroke);
    int virtualVertexIdxNoFlags = vertexIdx & VERTEX_IDX_MASK;
    // The real vertexIdx gets its baseVertex from the virtual patch buffer
    // (flags and tessellation base index), and its localVertexOffset from the
    // virtual vertexIdx.
    uint virtualPatchIdx = uint(virtualVertexIdxNoFlags >> patchStrideLog2);
    int localVertexOffset = virtualVertexIdxNoFlags &
                            ((1 << patchStrideLog2) - 1);
    vertexIdx = int(STORAGE_BUFFER_LOAD1(
                    @virtualPatchBuffer,
                    virtualPatchIdx)) +
                localVertexOffset;
  }
  else
  #endif
  {
    // Fills have their baseVertex built in directly to _vertexIdx.
    patchStrideLog2 = DS_FILL_PATCH_STRIDE_LOG2(isOuterCubicPatch);
  }

  // These flags are not valid until after virtual patch resolution.
  bool colorWriteDisabled = (vertexIdx & VERTEX_FLAG_DISABLE_COLOR_WRITE) != 0;
  bool isStrokeDepthPass = (vertexIdx & VERTEX_FLAG_STROKE_DEPTH_PASS) != 0;
  int vertexIdxNoFlags = vertexIdx & VERTEX_IDX_MASK;

  // The index buffers align per-patch vertex IDs on pow2 strides,
  // specifically so this shader can decode _vertexIdx without divides and
  // mods. (Using integer division costs 10% total frame perf on PowerVR and
  // 3% on Adreno.)
  // NOTE: The patches don't actually have pow2 numbers of vertices; the index
  // buffers just never reference those vertex IDs between the end of one
  // patch and the beginning of another.
  int patchIdx = vertexIdxNoFlags >> patchStrideLog2;
  int patchVertexIdx = vertexIdxNoFlags & ((1 << patchStrideLog2) - 1);
  float outset = .0;
  float coverage = 1.;
  bool isFillMidpointVertex = false;
  int patchSpokeIdx;
  int patchTessVertexStride;
  if (isStroke())
  {
    int lanesPerSpokeLog2 = DS_STROKE_LANES_PER_SPOKE_LOG2(isAAPolarStroke);
    int lane = patchVertexIdx & ((1 << lanesPerSpokeLog2) - 1);
    if (isPolarStroke && !isAAPolarStroke)
    {
      // Non-AA strokes only draw the solid body band (no AA bands).
      // The body band is lanes 1 & 2.
      ++lane;
    }
    outset = lane < 2 ? 1. : -1.;
    coverage = (lane == 0 || lane == 3) ? .0 : 1.;
    patchSpokeIdx = patchVertexIdx >> lanesPerSpokeLog2;
    patchTessVertexStride = int(MIDPOINT_FAN_PATCH_SEGMENT_SPAN);
  }
  else
  {
    isFillMidpointVertex = !isOuterCubicPatch &&
                           patchVertexIdx == DS_MIDPOINT_VERTEX_IDX;
    patchSpokeIdx = isFillMidpointVertex ? 0 : patchVertexIdx;
    patchTessVertexStride =
        isOuterCubicPatch
            // outerCubics tessellate a redundant bowtie vertex at the end
            // of every patch (for AA), which is never referenced by the
            // depthStencil fill index buffers. But we still need to stride
            // over it when indexing into the tessellation texture.
            ? int(OUTER_CUBIC_PATCH_SEGMENT_SPAN_PLUS_BOWTIE)
            : int(MIDPOINT_FAN_PATCH_SEGMENT_SPAN);
  }

  // Fetch our tessellation spoke.
  // NOTE: In the case of the contour-closing spoke, this will be the wrong
  // data. It gets patched shortly after.
  int tessSpokeIdx = patchIdx * patchTessVertexStride + patchSpokeIdx;
  uint4 tessVertexData = TEXEL_FETCH(
      @tessVertexTexture,
      tess_texel_coord(tessSpokeIdx));
  uint contourIDWithFlags = tessVertexData.w;
  uint contourID = contourIDWithFlags & CONTOUR_ID_MASK;
  bool isContourClosingSpoke = false;

  // The contour-closing spoke reaches into our neighbor's contour. Update
  // contourID to our own before fetching data. (The tessellator guarantees the
  // invariant that "contourID == neighborContourID - 1".)
  if (patchSpokeIdx == patchTessVertexStride &&
      (contourIDWithFlags & TESS_FLAG_FIRST_IN_MEMORY) != 0u)
  {
    --contourID;
    isContourClosingSpoke = true;
  }

  // Fetch and unpack the contour.
  // NOTE: The contourID is guaranteed to be >= 1 at this point, but clamp it
  // anyway because in the event of a bug, a buffer load at index "0u - 1" can
  // be very serious and hard to catch.
  uint contourStartIdx = (max(contourID, 1u) - 1u) * 2u;
  uint4 contourData = STORAGE_BUFFER_LOAD4(@contourBuffer, contourStartIdx);
  float2 midpoint = uintBitsToFloat(contourData.xy);
  uint contourFlags = contourData.z;
  uint pathID = contourFlags & CONTOUR_PATH_ID_MASK;
  uint firstSpokeIndexMemory = contourData.w;

  // Fetch and unpack the path.
  float2x2 M = make_float2x2(uintBitsToFloat(
      STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * PATH_DATA_ELEMENT_COUNT)));
  uint4 pathData = STORAGE_BUFFER_LOAD4(
      @pathBuffer,
      pathID * PATH_DATA_ELEMENT_COUNT + 1u);
  float2 translate = uintBitsToFloat(pathData.xy);
  ushort pathZIndex = cast_uint_to_ushort(pathData.z >> 16u);
  // Strokes repurpose "midpoint" since they don't use the real midpoint.
  float strokeRadius = isPolarStroke ? midpoint.y : .0;

  // The contour-closing spoke reached into our neighbor's contour. It needs to
  // either wrap back around to the starting spoke (fills, closed strokes), or
  // else stay clamped at the final vertex (open strokes).
  if (isContourClosingSpoke)
  {
    tessVertexData = TEXEL_FETCH(
        @tessVertexTexture,
        tess_texel_coord(
            isContourClosed(contourFlags) ? int(firstSpokeIndexMemory)
                                          : tessSpokeIdx - 1));
    contourIDWithFlags = tessVertexData.w;
  }

  float2 vertexPosition;
  if (isStroke())
  {
    // Ensure strokes always emit clockwise triangles. A left-handed matrix and
    // a mirrored (backwards) contour each flip the outside.
    if (isExactlyOneFlagSet(
            contourIDWithFlags,
            TESS_FLAG_NEGATIVE_DETERMINANT,
            TESS_FLAG_MIRRORED))
    {
      outset = -outset;
    }

    // Find the tangent angle of the curve at our vertex.
    float theta = unpackTessTheta(tessVertexData.z);
    float2 norm = float2(sin(theta), -cos(theta));
    float2 origin = uintBitsToFloat(tessVertexData.xy);

    float2 vertexOffset = norm;
    float2 aaOutsetDir = norm;
    // NOTE: 'outset' is only ever -1, 0, or +1.
    float aaOutsetMagnitude = (coverage == .0) ? outset : .0;

    if (isPolarStroke)
    {
      // Joins only emanate from the outer side of the stroke (except full
      // round joins, which emit both sides like curves).
      if ((contourIDWithFlags & TESS_FLAG_LEFT_JOIN) != 0u)
        outset = min(outset, .0);
      if ((contourIDWithFlags & TESS_FLAG_RIGHT_JOIN) != 0u)
        outset = max(outset, .0);

      uint joinType = contourIDWithFlags & TESS_MASK_JOIN_TYPE;
      if (joinType > TESS_FLAG_ROUND_JOIN)
      {
        bool isTan0 = (contourIDWithFlags & TESS_FLAG_JOIN_TANGENT_0) != 0u;
        bool isLeftJoin = (contourIDWithFlags & TESS_FLAG_LEFT_JOIN) != 0u;
        // This vertex belongs to a miter or bevel join. Begin by
        // finding the bisector, which is the same as norm rotated by
        // joinAngle/2. The tessellator already packed cos(joinAngle/2)
        // (the miterRatio), so we use that.
        float miterRatio = unpackTessMiterJoinRatio(tessVertexData.z);
        // Trig identity to find sin(joinAngle/2).
        // (miterRatio == cos(joinAngle/2).)
        float sinJoinAngleOver2 = sqrt(max(1. - miterRatio * miterRatio, .0));
        if (isTan0 == isLeftJoin)
          sinJoinAngleOver2 = -sinJoinAngleOver2;
        // Rotate norm by joinAngle/2 using a sin/cos rotation matrix.
        float2x2 rot = float2x2(
            miterRatio,
            sinJoinAngleOver2,
            -sinJoinAngleOver2,
            miterRatio);
        float2 bisector = MUL(rot, norm);
        // Miter joins that are further away than 4x the stroke radius
        // snap to bevel joins. A miter clip is a cap, and never snaps.
        bool isBevel = joinType == TESS_FLAG_BEVEL_JOIN ||
                       (joinType != TESS_FLAG_MITER_CLIP_JOIN &&
                        miterRatio < .25);
        // Bevels have 2 spokes and miters 3; a miter clip has 4,
        // carrying an inner pair instead of a single center.
        bool isInnerJoinSpoke = (contourIDWithFlags &
                                 TESS_FLAG_JOIN_TANGENT_INNER) != 0u;
        if (joinType == TESS_FLAG_MITER_CLIP_JOIN)
        {
          // For now, square caps are the only miter-clip: a
          // 180-degree join whose miterLimit is 1. Simplify the
          // miter-clip to only handle this specific case.
          vertexOffset = norm + bisector;
        }
        else if (isInnerJoinSpoke || !isBevel)
        {
          // Fun little mathematical relationship: distance along the
          // bisector to the bevel edge and to the miter corner turn
          // out to be reciprocals of one another.
          float t = isBevel ? miterRatio : 1. / miterRatio;
          vertexOffset = bisector * t;
        }
        // Bevel spokes, the miter's center spoke, and the miter-clip's
        // inner pair all outset their AA in the direction of the
        // bisector.
        if (isBevel || isInnerJoinSpoke)
          aaOutsetDir = bisector;
        // Rive always inkbleeds butt caps and bevel joins (partly to
        // avoid hairline cracks and partly because it just makes the AA
        // easier). This is not necessary for MSAA, but bleed out half a
        // pixel anyway in MSAA so that strokes look the same in all
        // modes.
        if (!isAAPolarStroke && isBevel)
          aaOutsetMagnitude = .5 * outset;
      }
    }

    vertexPosition = MUL(M, origin + vertexOffset * (outset * strokeRadius)) +
                     translate;
    if (aaOutsetMagnitude != .0)
    {
      // Push outward from the stroke edge by 'aaOutsetMagnitude' in both
      // screen dimensions.
      // Using sign() instead of normalize() makes the AA line offset in
      // "Manhattan" distance instead of Euclidean, which is exactly what
      // we want for square pixels.
      vertexPosition += sign(MUL(aaOutsetDir, inverse(M))) * aaOutsetMagnitude;
    }
  }
  else
  {
    // depthStencil fills never outset.
    float2 origin = isFillMidpointVertex ? midpoint
                                         : uintBitsToFloat(tessVertexData.xy);
    vertexPosition = MUL(M, origin) + translate;
  }

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
        uintBitsToFloat(STORAGE_BUFFER_LOAD4(
            @pathBuffer,
            pathID * PATH_DATA_ELEMENT_COUNT + 5u)));
    float4 clipRectInverseTranslate = uintBitsToFloat(STORAGE_BUFFER_LOAD4(
        @pathBuffer,
        pathID * PATH_DATA_ELEMENT_COUNT + 6u));
    set_clip_rect_plane_distances(
        clipRectInverseMatrix,
        clipRectInverseTranslate.xy,
        fragCoord CLIP_CONTEXT_UNPACK);
  }
  #endif // ENABLE_CLIP_RECT

  if (colorWriteDisabled)
  {
    // No color writes. Skip the paint fetches and, in case the pipeline
    // hasn't explicitly disabled color writes, output a fully transparent
    // solid color.
    v_paint = float4(.0, .0, .0, .0);
  #ifdef @ENABLE_MODULATED_IMAGE
    v_image = float3(0.0, 0.0, 0.0);
  #endif
  }
  else
  {
    uint paintType = pathData.z & 0xfu;
    bool paintHasAdvancedBlend = false;
  #ifdef @ENABLE_ADVANCED_BLEND
    if (@ENABLE_ADVANCED_BLEND)
    {
      uint blendMode = (pathData.z >> 4) & 0xfu;
      v_blendMode = float(blendMode);
      paintHasAdvancedBlend = blendMode != BLEND_SRC_OVER;
    }
  #endif

    // Unpack the paint once we have a position.
    if (paintType == SOLID_COLOR_PAINT_TYPE)
    {
      v_paint = float4(unpackUnorm4x8(pathData.w));
      if (paintHasAdvancedBlend)
      {
        // Advanced blends take an unmultiplied color, so coverage only
        // belongs in alpha.
        v_paint.a *= coverage;
      }
      else
      {
        v_paint *= coverage;
      }
    }
    else // Gradient.
    {
      float2x2 paintMatrix = make_float2x2(uintBitsToFloat(STORAGE_BUFFER_LOAD4(
          @pathBuffer,
          pathID * PATH_DATA_ELEMENT_COUNT + 3u)));

      float4 paintTranslate = uintBitsToFloat(STORAGE_BUFFER_LOAD4(
          @pathBuffer,
          pathID * PATH_DATA_ELEMENT_COUNT + 4u));

      float additivenessComplement = uintBitsToFloat(pathData.w);
      v_paint = packGradientData(
          fragCoord,
          paintMatrix,
          paintTranslate.xy,
          paintTranslate.zw,
          paintType,
          additivenessComplement,
          coverage);
    }

  #ifdef @ENABLE_MODULATED_IMAGE
    if (@ENABLE_MODULATED_IMAGE && (pathData.z & PAINT_FLAG_HAS_IMAGE) != 0u)
    {
      float2x2 imageMatrix = make_float2x2(uintBitsToFloat(STORAGE_BUFFER_LOAD4(
          @pathBuffer,
          pathID * PATH_DATA_ELEMENT_COUNT + 7u)));
      float4 paintTranslateAndLOD = uintBitsToFloat(STORAGE_BUFFER_LOAD4(
          @pathBuffer,
          pathID * PATH_DATA_ELEMENT_COUNT + 8u));
      float2 imageCoord = MUL(imageMatrix, fragCoord) + paintTranslateAndLOD.xy;

      // Add 1 to the LOD because a z value of 0 means "we don't have an
      // image"
      float imageZ = 1. + paintTranslateAndLOD.z;
      v_image = float3(imageCoord.x, imageCoord.y, imageZ);
    }
    else
    {
      v_image = float3(0.0, 0.0, 0.0);
    }
  #endif
  }

  float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
  #ifdef @POST_INVERT_Y
  pos.y = -pos.y;
  #endif
  uint depthCoverage8;
  if (isStroke())
  {
    // Quantize depth coverage to 254 discrete values. This way, the color
    // pass can be exactly 1 (8-bit) ULP larger, allowing exactly 1 fragment
    // to pass. (The one with largest coverage.)
    depthCoverage8 = uint(coverage * 254.);
    if (!isStrokeDepthPass)
      ++depthCoverage8;
  }
  else
  {
    depthCoverage8 = 0xffu; // Fills always have full coverage.
  }
  pos.z = packNormalizedDepth(pathZIndex, depthCoverage8);

  VARYING_PACK(v_paint);
  #ifdef @ENABLE_ADVANCED_BLEND
  VARYING_PACK(v_blendMode);
  #endif
  #ifdef @ENABLE_MODULATED_IMAGE
  VARYING_PACK(v_image);
  #endif
  EMIT_VERTEX(pos);
}
#endif // @VERTEX

#ifdef @FRAGMENT

  #ifdef @RENDER_TO_UAV
PLS_MAIN(@drawFragmentMain)
  #else
FRAG_DATA_MAIN(half4, @drawFragmentMain)
  #endif
{
  VARYING_UNPACK(v_paint, float4);
  #ifdef @ENABLE_MODULATED_IMAGE
  VARYING_UNPACK(v_image, float3);
  #endif
  #ifdef @ENABLE_ADVANCED_BLEND
  VARYING_UNPACK(v_blendMode, half);
  #endif

  #ifdef @ENABLE_ADVANCED_BLEND
  ushort blendMode = cast_half_to_ushort(v_blendMode);
  bool paintHasAdvancedBlend = @ENABLE_ADVANCED_BLEND &&
                               blendMode != BLEND_SRC_OVER;
  #else
  const bool paintHasAdvancedBlend = false;
  #endif

  // Unpack the paint color, with coverage modulated in.
  half4 color;
  if (v_paint.a >= .0) // Is the paint a solid color?
  {
    // v_paint was sent unmultiplied for advanced-blend draws and
    // premultiplied otherwise, matching paintHasAdvancedBlend.
    // v_paint is already pre-modulated by coverage.
    color = cast_float4_to_half4(v_paint);
  }
  else // Paint is a gradient.
  {
    float2 gradTexCoord = getGradientUV(
        v_paint,
        uniforms.gradTextureYScale,
        uniforms.gradTextureYBias);
    color = TEXTURE_SAMPLE_LOD(@gradTexture, gradSampler, gradTexCoord, .0);
    if (isStroke())
    {
      color.a *= getGradientCoverage(v_paint);
    }

    if (!paintHasAdvancedBlend)
    {
      // Premultipy the gradient if we aren't using advanced blend.
      // (The gradient texture is always unmultiplied so that we don't
      // lose color data while doing the hardware filter.)
      color.rgb *= color.a;
      // Only non-advanced blend supports additiveness.
      color.a *= getGradientAdditivenessComplement(v_paint);
    }
  }

  #ifdef @ENABLE_MODULATED_IMAGE
  if (@ENABLE_MODULATED_IMAGE && v_image.z > 0.0)
  {
    half lod = v_image.z - 1.;
    half4 imageColor = TEXTURE_SAMPLE_DYNAMIC_LOD(
        @imageTexture,
        imageSampler,
        v_image.rg,
        lod);

    // Images are always premultiplied so the (transparent) background color
    // doesn't bleed into the edges during the hardware filter; unmultiply
    // to match this draw's convention if needed.
    if (paintHasAdvancedBlend)
      imageColor = make_half4(unmultiply_rgb(imageColor), imageColor.a);

    color *= imageColor;
  }
  #endif

  #if defined(@ENABLE_ADVANCED_BLEND) && !defined(@FIXED_FUNCTION_COLOR_OUTPUT)
    // Do the color portion of the blend mode in the shader.
    #ifdef @RENDER_TO_UAV
  half4 dstColorPremul = PLS_LOAD4F_UAV(@dstColorUav);
    #else
  half4 dstColorPremul = DST_COLOR_FETCH(@dstColorTexture);
    #endif
  color.rgb = advanced_color_blend(color.rgb, dstColorPremul, blendMode);
  // Premultiply before the hardware srcOver portion of the blend equation.
  color.rgb *= color.a;
  #elif defined(@RENDER_TO_UAV)
  // No hardware blend unit on this path; src-over happens in the shader.
  half4 dstColorPremul = PLS_LOAD4F_UAV(@dstColorUav);
  #endif

  color.rgb = add_dither_if_alpha_nonzero(
      color.rgb,
      color.a,
      _fragCoord.xy,
      uniforms.ditherScale,
      uniforms.ditherBias);

  #ifdef @RENDER_TO_UAV
  // Only an all-zero color is a no-op. Additive paints carry alpha 0 with
  // nonzero rgb.
  if (any(notEqual(color, make_half4(.0))))
  {
    PLS_STORE4F_UAV(@dstColorUav, blend_src_over(color, dstColorPremul));
  }
  #endif

  #ifdef @RENDER_TO_UAV
  EMIT_PLS
  #else
  EMIT_FRAG_DATA(color);
  #endif
}

#endif // FRAGMENT
