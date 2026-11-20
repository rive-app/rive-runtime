/*
 * Copyright 2020 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from
 * skia:src/gpu/ganesh/tessellate/GrStrokeTessellationShader.cpp
 *
 * Copyright 2022 Rive
 */

#define MAX_PARAMETRIC_SEGMENTS_LOG2 10 // Max 1024 segments.

INLINE void swap(INOUT(bool) a, INOUT(bool) b)
{
  bool c = a;
  a = b;
  b = c;
}

// The CPU sends TESS_FLAG_[FIRST,LAST]_IN_MEMORY with the entire first/last
// cubics of a contour. This resolves them to just the boundary spokes of those
// cubics, in memory order.
// NOTE: If there are padding spokes, they may also carry boundary flags. This
// is harmless because there is never >= one patch worth of padding, and every
// cubic has at least two real (non-padding) spokes, so no spoke will ever carry
// both boundary flags.
INLINE uint resolveContourBoundaryFlags(
    uint contourIDWithFlags,
    float vertexIdx,
    float totalVertexCount)
{
  bool isContourFirst = (contourIDWithFlags & TESS_FLAG_FIRST_IN_MEMORY) !=
                            0u &&
                        vertexIdx <= .0;
  bool isContourLast = (contourIDWithFlags & TESS_FLAG_LAST_IN_MEMORY) != 0u &&
                       vertexIdx >= totalVertexCount - 1.;

  // Mirrored contour data is rendered right to left, so swap last/first.
  if ((contourIDWithFlags & TESS_FLAG_MIRRORED) != 0u)
    swap(isContourFirst, isContourLast);

  // Clear the boundary flags first, before re-setting the applicable ones. We
  // can't just mask them off because if the contour is mirrored, we also swap
  // first/last.
  contourIDWithFlags &= ~(TESS_FLAG_FIRST_IN_MEMORY | TESS_FLAG_LAST_IN_MEMORY);
  if (isContourFirst)
    contourIDWithFlags |= TESS_FLAG_FIRST_IN_MEMORY;
  if (isContourLast)
    contourIDWithFlags |= TESS_FLAG_LAST_IN_MEMORY;
  return contourIDWithFlags;
}

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
// RHI Version > 5.8 fails to parse this when it's multi line
// End in '_' because D3D interprets the '1' as a semantic index.
ATTR(0, float4, @a_p0p1_);
ATTR(1, float4, @a_p2p3_);
ATTR(2, float4, @a_joinTan_and_ys); // [joinTangent, y, reflectionY]
  #ifdef SPLIT_UINT4_ATTRIBUTES
ATTR(3, uint, @a_x0x1);
ATTR(4, uint, @a_reflectionX0X1);
ATTR(5, uint, @a_segmentCounts);
ATTR(6, uint, @a_contourIDWithFlags);
  #else
// [x0x1, reflectionX0X1, segmentCounts, contourIDWithFlags]
// RHI Version > 5.8 fails to parse this when it's multi line
ATTR(3, uint4, @a_args);
  #endif
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_p0p1);
NO_PERSPECTIVE VARYING(1, float4, v_p2p3);
// [vertexIdx, totalVertexCount, joinSegmentCount,
//  parametricSegmentCount, radsPerPolarSegment]
NO_PERSPECTIVE VARYING(2, float4, v_args);
// [joinTangent, radsPerJoinSegment,
//  dsStrokeOutsetScale (if depthStencil, else -1)]
NO_PERSPECTIVE VARYING(3, float4, v_joinArgs);
NO_PERSPECTIVE VARYING(4, float4, v_dsViewMatrix); // (depthStencil only)
FLAT VARYING(5, uint, v_contourIDWithFlags);
VARYING_BLOCK_END

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
TEXTURE_R16F_1D_ARRAY(
    PER_FLUSH_BINDINGS_SET,
    GAUSSIAN_INTEGRAL_TEXTURE_IDX,
    @gaussianIntegralTexture);
VERTEX_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GAUSSIAN_INTEGRAL_TEXTURE_IDX, gaussianIntegralSampler)

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x4(PATH_BUFFER_IDX, PathBuffer, @pathBuffer);
STORAGE_BUFFER_U32x4(CONTOUR_BUFFER_IDX, ContourBuffer, @contourBuffer);
VERTEX_STORAGE_BUFFER_BLOCK_END

VERTEX_MAIN(@tessellateVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
  // Each instance repeats twice. Once for normal patch(es) and once for
  // reflection(s).
  ATTR_UNPACK(_instanceID, attrs, @a_p0p1_, float4);
  ATTR_UNPACK(_instanceID, attrs, @a_p2p3_, float4);
  ATTR_UNPACK(_instanceID, attrs, @a_joinTan_and_ys, float4);
  #ifdef SPLIT_UINT4_ATTRIBUTES
  ATTR_UNPACK(_instanceID, attrs, @a_x0x1, uint);
  ATTR_UNPACK(_instanceID, attrs, @a_reflectionX0X1, uint);
  ATTR_UNPACK(_instanceID, attrs, @a_segmentCounts, uint);
  ATTR_UNPACK(_instanceID, attrs, @a_contourIDWithFlags, uint);

  uint4 @a_args = uint4(
      @a_x0x1,
      @a_reflectionX0X1,
      @a_segmentCounts,
      @a_contourIDWithFlags);
  #else
  ATTR_UNPACK(_instanceID, attrs, @a_args, uint4);
  #endif

  VARYING_INIT(v_p0p1, float4);
  VARYING_INIT(v_p2p3, float4);
  VARYING_INIT(v_args, float4);
  VARYING_INIT(v_joinArgs, float4);
  VARYING_INIT(v_dsViewMatrix, float4);
  VARYING_INIT(v_contourIDWithFlags, uint);

  float2 p0 = @a_p0p1_.xy;
  float2 p1 = @a_p0p1_.zw;
  float2 p2 = @a_p2p3_.xy;
  float2 p3 = @a_p2p3_.zw;
  // Each instance has two spans, potentially for both a forward copy and and
  // reflection. (If the second span isn't needed, the client will have placed
  // it offscreen.)
  bool isFirstSpan = _vertexID < 4;
  float y = isFirstSpan ? @a_joinTan_and_ys.z : @a_joinTan_and_ys.w;
  int x0x1 = int(isFirstSpan ? @a_args.x : @a_args.y);
  #ifdef GLSL
  int x1up = x0x1 << 16;
  if (@a_args.z == 0xffffffffu)
  {
    // Pixel 8 with ARM Mali-G715 throws away "x0x1 << 16 >> 16". We need
    // this in order to sign-extend the bottom 16 bits of x0x1. Create a
    // branch that we know won't be taken, in order to convince the compiler
    // not to throw this operation away. NOTE: we could use
    // bitfieldExtract(), but it isn't available on ES 3.0.
    --x1up;
  }
  float x0 = float(x1up >> 16);
  #else
  float x0 = float(x0x1 << 16 >> 16);
  #endif
  float x1 = float(x0x1 >> 16);
  float2 coord = float2(
      (_vertexID & 1) == 0 ? x0 : x1,
      (_vertexID & 2) == 0 ? y + 1. : y);
  if ((x1 - x0) * uniforms.tessInverseViewportY < .0)
  {
    // Make sure we always emit clockwise triangles. Swap the top and bottom
    // vertices.
    coord.y = 2. * y + 1. - coord.y;
  }

  // Unpack arguments.
  uint parametricSegmentCount = @a_args.z & 0x3ffu;
  uint polarSegmentCount = (@a_args.z >> 10) & 0x3ffu;
  uint joinSegmentCount = @a_args.z >> 20;
  uint contourIDWithFlags = @a_args.w;
  uint contourID = contourIDWithFlags & CONTOUR_ID_MASK;
  // Clamp contourStartIdx at 0, even though we gate contourBuffer loads on
  // "contourID > 0u", and even though GLSL specifies short-circuit evaluation
  // for the ternary operator.
  // PowerVR (B-Series BXM-8-256, build 1.15) appears to evaluate both ternary
  // sides unconditionally, leading to a load at buffer index 0xffffffff when
  // contourID == 0, which crashes the render pass.
  uint contourStartIdx = (max(contourID, 1u) - 1u) * 2u;
  uint4 contourData = contourID > 0u ? STORAGE_BUFFER_LOAD4(
                                           @contourBuffer,
                                           contourStartIdx)
                                     : uint4(0u, 0u, 0u, 0u);
  uint pathID = contourData.z & CONTOUR_PATH_ID_MASK;
  bool isStroke = (contourData.z & CONTOUR_FLAG_STROKE) != 0u;
  float featherRadius = .0;
  // (contourData is all zeros when contourID == 0, so this also skips the
  // load for contourID 0.)
  if ((contourData.z & CONTOUR_FLAG_HAS_OUTSET_OR_FEATHER) != 0u)
  {
    featherRadius = uintBitsToFloat(
        STORAGE_BUFFER_LOAD4(@contourBuffer, contourStartIdx + 1u).w);
  }

  if (featherRadius != .0 && !isStroke)
  {
    // We're a cubic from a feathered fill. To simulate the
    // feather-softening effect that happens with curvature, reduce the
    // height of the curve proportionally.
    // Start by finding the point of maximum height on the cubic.
    float maxHeightT;
    float height = find_cubic_max_height(p0, p1, p2, p3, maxHeightT);

    // Measure curvature across one standard deviation of the feather.
    float oneStddev = featherRadius * (1. / GAUSSIAN_INTEGRAL_TEXTURE_STDDEVS);
    float curvature = measure_cubic_local_curvature(
        p0,
        p1,
        p2,
        p3,
        maxHeightT,
        oneStddev);

    // The feather gets softer with curvature. Find a dimming factor based
    // on the strength of curvature at maximum height.
    float dimming = 1. - curvature * (1. / PI);

    // It gets hard to measure curvature on short segments. Also taper down
    // to completely flat as the distance between endpoints moves from 2
    // standard deviations to 1.
    float stddevsPow2 = dot(p3 - p0, p3 - p0) / (oneStddev * oneStddev);
    float dimmingByStddevs = (stddevsPow2 - 1.) * .5;
    dimming = min(dimming, dimmingByStddevs);

    // Unfortunately, the best method we have to get rid of some final
    // speckles on cusps is to dim everything by 1%.
    dimming = min(dimming, .99);

    // Soften the feather by reducing the curve height. Find a new height
    // such that the center of the feather (currently 50% opacity) is
    // reduced to "50% * dimming".
    float desiredOpacityOnCenter = .5 * dimming;
    float x = INVERSE_FEATHER(desiredOpacityOnCenter) * -2. + 1.;
    float softness = clamped_divide(x * featherRadius, height);

    // Flatten the curve down to "softenedHeight". (Height scales linearly
    // as we lerp the control points to "flatLinePoints".)
    float4 flatLinePoints = mix(
        p0.xyxy,
        p3.xyxy,
        float4(1. / 3., 1. / 3., 2. / 3., 2. / 3.));
    p1 = mix(p1, flatLinePoints.xy, softness);
    p2 = mix(p2, flatLinePoints.zw, softness);
  }

  if ((contourIDWithFlags & TESS_FLAG_CULL_EXCESS_TESSELLATION_SEGMENTS) != 0u)
  {
    // This span may have more tessellation vertices allocated to it than
    // necessary (e.g., outerCurve patches all have a fixed patch size,
    // regardless of how many segments the curve actually needs). Re-run
    // Wang's formula to figure out how many segments we actually need, and
    // make any excess segments degenerate by co-locating their vertices at
    // T=0.
    float2x2 mat = make_float2x2(uintBitsToFloat(
        STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * PATH_DATA_ELEMENT_COUNT)));
    float2 d0 = MUL(mat, -2. * p1 + p2 + p0);
    float2 d1 = MUL(mat, -2. * p2 + p3 + p1);
    float m = max(dot(d0, d0), dot(d1, d1));
    float n = max(ceil(sqrt(.75 * 4. * sqrt(m))), 1.);
    parametricSegmentCount = min(uint(n), parametricSegmentCount);
  }

  float2 joinTangent = @a_joinTan_and_ys.xy;
  float dsStrokeOutsetScale = -1.; // -1 indicates a non-depthStencil spoke
                                   // (non-depthStencil flush or feather).
  float2x2 dsViewMatrix = float2x2(1., .0, .0, 1.);
  if (uniforms.tessellationMode != 0u && featherRadius == .0)
  {
    // We are a depthStencil spoke. dsStrokeOutsetScale >= 0 indicates this to
    // the fragment stage.
    dsStrokeOutsetScale = .0;
    if (pathID != 0u) // Don't transform padding vertices.
    {
      // depthStencil paths get transformed inside the tessellator. Transform as
      // much as we can in the vertex stage.
      float2x2 M = make_float2x2(uintBitsToFloat(
          STORAGE_BUFFER_LOAD4(@pathBuffer, pathID * PATH_DATA_ELEMENT_COUNT)));
      float2 translate = uintBitsToFloat(
          STORAGE_BUFFER_LOAD4(
              @pathBuffer,
              pathID * PATH_DATA_ELEMENT_COUNT + 1u)
              .xy);
      if (isStroke)
      {
        // In the next stage, strokes tessellate and outset in path space. Apply
        // the translate now to avoid a varying, but let the fragment stage
        // apply the 2x2.
        float2 pathTranslate = MUL(inverse(M), translate);
        p0 += pathTranslate;
        p1 += pathTranslate;
        p2 += pathTranslate;
        p3 += pathTranslate;
        dsViewMatrix = M;
        // Strokes repurpose "midpoint" (contourData.xy) since they don't use
        // the real midpoint.
        float oneOverMatrixMaxScale = uintBitsToFloat(contourData.x);
        // depthStencil strokeOutsets are normalized to the range:
        //
        //   [0, 0xffff] => [-RIVE_MITER_LIMIT, +RIVE_MITER_LIMIT]
        //
        // (See the fragment stage for details.)
        dsStrokeOutsetScale = oneOverMatrixMaxScale * (.5 / RIVE_MITER_LIMIT);
      }
      else
      {
        // Fills and hairlines have no path-relative stroke width, and cubics
        // are affine invariant, so we can fully transform p0..p3 upfront.
        p0 = MUL(M, p0) + translate;
        p1 = MUL(M, p1) + translate;
        p2 = MUL(M, p2) + translate;
        p3 = MUL(M, p3) + translate;
        joinTangent = MUL(M, joinTangent);
        if ((contourIDWithFlags & TESS_FLAG_RETROFIT_TRI_STRIP) != 0u)
        {
          // Retrofit tri strips actually use joinTangent as a 5th point.
          joinTangent += translate;
        }
      }
    }
  }

  // Polar and parametric segments share the same beginning and ending
  // vertices, so the merged *vertex* count is equal to the sum of polar and
  // parametric *segment* counts.
  uint totalVertexCount = parametricSegmentCount + polarSegmentCount +
                          joinSegmentCount - 1u;

  float2x2 tangents = find_cubic_tangents(p0, p1, p2, p3);
  float theta = acos(cosine_between_vectors(tangents[0], tangents[1]));
  float radsPerPolarSegment = theta / float(polarSegmentCount);
  // Adjust sign of radsPerPolarSegment to match the direction the curve
  // turns.
  // NOTE: Since the curve is not allowed to inflect, we can just check
  // F'(.5) x F''(.5).
  // NOTE: F'(.5) x F''(.5) has the same sign as (p2 - p0) x (p3 - p1).
  float turn = determinant(float2x2(p2 - p0, p3 - p1));
  // This is the case for joins and cusps where points are co-located.
  if (turn == .0)
    turn = determinant(tangents);
  if (turn < .0)
    radsPerPolarSegment = -radsPerPolarSegment;

  v_p0p1 = float4(p0, p1);
  v_p2p3 = float4(p2, p3);
  v_args = float4(
      float(totalVertexCount) - abs(x1 - coord.x), // vertexIdx
      float(totalVertexCount),                     // totalVertexCount
      (joinSegmentCount << 10) | parametricSegmentCount,
      radsPerPolarSegment);
  // Always forward the joinTangent slot. A real join (joinSegmentCount > 1)
  // also needs to take the branch below and get radsPerJoinSegment in .z, but
  // retrofitted triangle strips reuse .xy, and do not take the branch.
  v_joinArgs.xy = joinTangent;
  if (joinSegmentCount > 1u)
  {
    float2x2 joinTangents = float2x2(tangents[1], joinTangent);
    float joinTheta = acos(
        cosine_between_vectors(joinTangents[0], joinTangents[1]));
    float joinSpan = float(joinSegmentCount);
    if ((contourIDWithFlags &
         (TESS_MASK_JOIN_TYPE | TESS_FLAG_EMULATED_STROKE_CAP)) ==
        (TESS_FLAG_ROUND_JOIN | TESS_FLAG_EMULATED_STROKE_CAP))
    {
      // Round caps emulated as joins need to emit vertices at T=0 and
      // T=1, unlike normal round joins. The fragment shader will handle
      // most of this, but here we need to adjust radsPerJoinSegment to
      // account for the fact that this join will be rotating around two
      // more segments.
      joinSpan -= 2.;
    }
    float radsPerJoinSegment = joinTheta / joinSpan;
    if (determinant(joinTangents) < .0)
      radsPerJoinSegment = -radsPerJoinSegment;
    v_joinArgs.z = radsPerJoinSegment;
  }
  v_joinArgs.w = dsStrokeOutsetScale;
  v_dsViewMatrix = float4(dsViewMatrix[0], dsViewMatrix[1]);

  if (x1 < x0) // Reflections are drawn right to left.
  {
    contourIDWithFlags |= TESS_FLAG_MIRRORED;
  }

  v_contourIDWithFlags = contourIDWithFlags;

  float4 pos = pixel_coord_to_clip_coord(
      coord,
      2. / TESS_TEXTURE_WIDTH,
      uniforms.tessInverseViewportY);
  #ifdef @POST_INVERT_Y
  pos.y = -pos.y;
  #endif

  VARYING_PACK(v_p0p1);
  VARYING_PACK(v_p2p3);
  VARYING_PACK(v_args);
  VARYING_PACK(v_joinArgs);
  VARYING_PACK(v_dsViewMatrix);
  VARYING_PACK(v_contourIDWithFlags);
  EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
FRAG_TEXTURE_BLOCK_END

// True if we are processing a spoke for the depthStencil shader. Those shaders
// expect different tessData than the others.
INLINE bool isDepthStencilSpoke(float dsStrokeOutsetScale)
{
  // depthStencil is indicated via sign(dsStrokeOutsetScale).
  return dsStrokeOutsetScale >= .0;
}

// True if we are processing a depthStencil stroke, which finds its outsets,
// including joins, here in the tessellator.
INLINE bool isDepthStencilStroke(float dsStrokeOutsetScale)
{
  // Fills (and padding) set dsStrokeOutsetScale to 0.
  return dsStrokeOutsetScale > .0;
}

// inverse(m) == adjugate(m) / determinant(m).
// When scale doesn't matter, adjucate() is faster than inverse().
INLINE float2x2 adjugate(float2x2 m)
{
  return float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
}

FRAG_DATA_MAIN(uint4, @tessellateFragmentMain)
{
  VARYING_UNPACK(v_p0p1, float4);
  VARYING_UNPACK(v_p2p3, float4);
  VARYING_UNPACK(v_args, float4);
  VARYING_UNPACK(v_joinArgs, float4);
  VARYING_UNPACK(v_dsViewMatrix, float4);
  VARYING_UNPACK(v_contourIDWithFlags, uint);

  float2 p0 = v_p0p1.xy;
  float2 p1 = v_p0p1.zw;
  float2 p2 = v_p2p3.xy;
  float2 p3 = v_p2p3.zw;
  float2x2 tangents = find_cubic_tangents(p0, p1, p2, p3);
  // Colocate any padding vertices at T=0.
  float vertexIdx = max(floor(v_args.x), .0);
  float totalVertexCount = v_args.y;
  uint joinSegmentCount_and_parametricSegmentCount = uint(v_args.z);
  float parametricSegmentCount = float(
      joinSegmentCount_and_parametricSegmentCount & 0x3ffu);
  float joinSegmentCount = float(
      joinSegmentCount_and_parametricSegmentCount >> 10);
  float radsPerPolarSegment = v_args.w;
  float dsStrokeOutsetScale = v_joinArgs.w;
  uint contourIDWithFlags = resolveContourBoundaryFlags(
      v_contourIDWithFlags,
      vertexIdx,
      totalVertexCount);

  // mergedVertexID/mergedSegmentCount are relative to the sub-section of the
  // instance this vertex belongs to (either the curve section that consists
  // of merged polar and parametric segments, or the join section composed of
  // just polar segments).
  //
  // Begin with the assumption that we belong to the curve section.
  float mergedSegmentCount = totalVertexCount - joinSegmentCount;
  float mergedVertexID = vertexIdx;
  if (mergedVertexID <= mergedSegmentCount)
  {
    // We do belong to the curve section. Clear out any stroke join flags.
    contourIDWithFlags &= ~TESS_MASK_JOIN_TYPE;
  }
  else
  {
    // We actually belong to the join section following the curve. Construct
    // a point-cubic with rotation.
    p0 = p1 = p2 = p3;
    tangents = float2x2(tangents[1], v_joinArgs.xy /*joinTangent*/);
    parametricSegmentCount = 1.;
    mergedVertexID -= mergedSegmentCount;
    mergedSegmentCount = joinSegmentCount;
    radsPerPolarSegment = v_joinArgs.z; // radsPerJoinSegment.
    bool isEmulatedCap = (contourIDWithFlags & TESS_FLAG_EMULATED_STROKE_CAP) !=
                         0u;
    if (isEmulatedCap ||
        (contourIDWithFlags & TESS_MASK_JOIN_TYPE) == TESS_FLAG_FEATHER_JOIN)
    {
      // Unlike cubics, typical joins don't emit spokes at the beginning
      // or end of the section (they share those spokes with their
      // neighbor cubics instead.)
      //
      // BUT, caps being emulated by joins don't have a neighbor to share
      // one of their sides with, so those do emit spokes at the beginning
      // and end of the section. Feather joins also need spokes at the
      // beginning and end.
      //
      // Preserve the same number of vertices, but adjust our stepping
      // parameters so we begin at T=0 and end at T=1. (The CPU should
      // have known we were going to add vertices here and increased our
      // count to make sure the tessellation would still be smooth).
      mergedSegmentCount -= 2.;
      --mergedVertexID;
    }
    bool isCapClosingSpoke = isEmulatedCap &&
                             (mergedVertexID == 0. ||
                              mergedVertexID == mergedSegmentCount);
    if (isCapClosingSpoke)
    {
      // The tessellator and vertex shader need to treat opening and
      // closing spokes of caps emulated as joins like normal cubic spokes
      // (leave them full-width as opposed to half so we don't create a
      // T-junction with the cubic edge we're capping, don't reposition
      // miter/bevel spokes, etc.).
      //
      // The purpose of these spokes is to stand in for what normally
      // would have been a neighboring full-length cubic spoke, but is
      // absent from the neighbor-less side of a 180-degree join that is
      // standing in for a cap.
      contourIDWithFlags &= ~TESS_MASK_JOIN_TYPE;
    }
    else if ((contourIDWithFlags & TESS_MASK_JOIN_TYPE) != TESS_FLAG_CUSP_JOIN)
    {
      contourIDWithFlags |= radsPerPolarSegment < .0 ? TESS_FLAG_LEFT_JOIN
                                                     : TESS_FLAG_RIGHT_JOIN;
    }
    if ((contourIDWithFlags & TESS_MASK_JOIN_TYPE) > TESS_FLAG_ROUND_JOIN)
    {
      // We belong to a miter or bevel join.
      float halfJoin = mergedSegmentCount * .5;
      if (mergedVertexID < halfJoin)
        contourIDWithFlags |= TESS_FLAG_JOIN_TANGENT_0;
      if (mergedSegmentCount > 3. && mergedVertexID > halfJoin - 1. &&
          mergedVertexID < halfJoin + 1.)
        contourIDWithFlags |= TESS_FLAG_JOIN_TANGENT_INNER;
      // Miter and bevel spokes snap to either the beginning or end
      // of the join section, and then get placed later on.
      mergedVertexID = mergedVertexID < halfJoin ? .0 : mergedSegmentCount;
    }
  }

  float2 tessCoord;
  float theta = .0; // Unused by (non-feathered) depthStencil.
  float2 spokeTangent = float2(1., .0); // [cos(theta == 0), sin(theta == 0)]
  if (mergedVertexID == .0 || mergedVertexID == mergedSegmentCount)
  {
    // Tessellated vertices at the beginning and end of the strip use exact
    // endpoints and tangents. This ensures crack-free seaming between
    // instances.
    bool isTan0 = mergedVertexID < mergedSegmentCount * .5;
    tessCoord = isTan0 ? p0 : p3;
    spokeTangent = isTan0 ? tangents[0] : tangents[1];
    if (!isDepthStencilSpoke(dsStrokeOutsetScale))
      theta = atan2(spokeTangent);
  }
  else if ((contourIDWithFlags & TESS_FLAG_RETROFIT_TRI_STRIP) != 0u)
  {
    // This cubic should actually be drawn as a (non-AA) 5-point triangle
    // strip: [p0, p1, p3, p2, joinTangent]. This is used to reduce draws
    // and pipeline transitions by squeezing in triangles that don't
    // otherwise need special state or shading logic.
    tessCoord = p0;
    if (mergedVertexID >= float(OUTER_CUBIC_PATCH_SEGMENT_SPAN / 2u))
      tessCoord = p1;
    if (mergedVertexID >= float(OUTER_CUBIC_PATCH_SEGMENT_SPAN * 3u / 4u))
      tessCoord = p2;
    if (mergedVertexID >= float(OUTER_CUBIC_PATCH_SEGMENT_SPAN * 7u / 8u))
      tessCoord = v_joinArgs.xy; // joinTangent
  }
  else
  {
    float T, polarT;
    if (parametricSegmentCount == mergedSegmentCount)
    {
      // There are no polar vertices. This is (probably) a fill. Vertices
      // are spaced evenly in parametric space.
      T = mergedVertexID / parametricSegmentCount;
      polarT = .0; // Set polarT != T to ensure we calculate the
                   // parametric tangent later.
    }
    else
    {
      // Compute the location and tangent direction of the tessellated
      // stroke vertex with the integral id "mergedVertexID", where
      // mergedVertexID is the sorted-order index of parametric and polar
      // vertices. Start by finding the tangent function's power basis
      // coefficients. These define a tangent direction (scaled by some
      // uniform value) as:
      //
      //                                                 |T^2|
      //     Tangent_Direction(T) = dx,dy = |A  2B  C| * |T  |
      //                                    |.   .  .|   |1  |
      float2 A, B, C = p1 - p0;
      float2 D = p3 - p0;
      float2 E = p2 - p1;
      B = E - C;
      A = -3. * E + D;
      // FIXME(crbug.com/800804,skbug.com/11268): Consider normalizing the
      // exponents in A,B,C at this point in order to prevent fp32
      // overflow.

      // Now find the coefficients that give a tangent direction from a
      // parametric vertex ID:
      //
      //  Tangent_Direction(parametricVertexID) = dx,dy =
      //
      //                     |parametricVertexID^2|
      //      |A  B_  C_| *  |parametricVertexID  |
      //      |.   .   .|    |1                   |
      //
      float2 B_ = B * (parametricSegmentCount * 2.);
      float2 C_ = C * (parametricSegmentCount * parametricSegmentCount);

      // Run a binary search to determine the highest parametric vertex
      // that is located on or before the mergedVertexID. A merged ID is
      // determined by the sum of complete parametric and polar segments
      // behind it. i.e., find the highest parametric vertex where:
      //
      //    parametricVertexID + floor(numPolarSegmentsAtParametricT) <=
      //    mergedVertexID
      //
      float lastParametricVertexID = .0;
      float maxParametricVertexID = min(
          parametricSegmentCount - 1.,
          mergedVertexID);
      // FIXME(crbug.com/800804,skbug.com/11268): This normalize() can
      // overflow.
      float2 tan0norm = normalize(tangents[0]);
      float negAbsRadsPerSegment = -abs(radsPerPolarSegment);
      float maxRotation0 = (1. + mergedVertexID) * abs(radsPerPolarSegment);
      for (int p = MAX_PARAMETRIC_SEGMENTS_LOG2 - 1; p >= 0; --p)
      {
        // Test the parametric vertex at lastParametricVertexID + 2^p.
        float testParametricID = lastParametricVertexID + exp2(float(p));
        if (testParametricID <= maxParametricVertexID)
        {
          float2 testTan = testParametricID * A + B_;
          testTan = testParametricID * testTan + C_;
          float cosRotation = dot(normalize(testTan), tan0norm);
          float maxRotation = testParametricID * negAbsRadsPerSegment +
                              maxRotation0;
          maxRotation = min(maxRotation, PI);
          // Is rotation <= maxRotation? (i.e., is the number of
          // complete polar segments behind testT, + testParametricID
          // <= mergedVertexID?)
          if (cosRotation >= cos(maxRotation))
            lastParametricVertexID = testParametricID;
        }
      }

      // Find the T value of the parametric vertex at
      // lastParametricVertexID.
      float parametricT = lastParametricVertexID / parametricSegmentCount;

      // Now that we've identified the highest parametric vertex on or
      // before the mergedVertexID, the highest polar vertex is easy:
      float lastPolarVertexID = mergedVertexID - lastParametricVertexID;

      // Find the angle of tan0, or the angle between tan0norm and the
      // positive x axis.
      float theta0 = acos(clamp(tan0norm.x, -1., 1.));
      theta0 = tan0norm.y >= .0 ? theta0 : -theta0;

      // Find the tangent vector on the vertex at lastPolarVertexID.
      theta = lastPolarVertexID * radsPerPolarSegment + theta0;
      float2 norm = float2(sin(theta), -cos(theta));
      spokeTangent = float2(-norm.y, norm.x);

      // Find the T value where the tangent is orthogonal to norm. This is
      // a quadratic:
      //
      //     dot(norm, Tangent_Direction(T)) == 0
      //
      //                         |T^2|
      //     norm * |A  2B  C| * |T  | == 0
      //            |.   .  .|   |1  |
      //
      float a = dot(norm, A), b_over_2 = dot(norm, B), c = dot(norm, C);
      float discr_over_4 = max(b_over_2 * b_over_2 - a * c, .0);
      float q = sqrt(discr_over_4);
      if (b_over_2 > .0)
        q = -q;
      q -= b_over_2;

      // Roots are q/a and c/q. Since each curve section does not inflect
      // or rotate more than 180 degrees, there can only be one tangent
      // orthogonal to "norm" inside 0..1. Pick the root nearest .5.
      float _5qa = -.5 * q * a;
      float2 root = (abs(q * q + _5qa) < abs(a * c + _5qa)) ? float2(q, a)
                                                            : float2(c, q);
      polarT = (root.t != .0) ? root.s / root.t : .0;
      polarT = clamp(polarT, .0, 1.);

      // The root finder above can become unstable when lastPolarVertexID
      // == 0 (e.g., if there are roots at exatly 0 and 1 both). polarT
      // should always == 0 in this case.
      if (lastPolarVertexID == .0)
        polarT = .0;

      // Now that we've identified the T values of the last parametric and
      // polar vertices, our final T value for mergedVertexID is whichever
      // is larger.
      T = max(parametricT, polarT);
    }

    // Evaluate the cubic at T. Use De Casteljau's for its accuracy and
    // stability.
    float2 ab = unchecked_mix(p0, p1, T);
    float2 bc = unchecked_mix(p1, p2, T);
    float2 cd = unchecked_mix(p2, p3, T);
    float2 abc = unchecked_mix(ab, bc, T);
    float2 bcd = unchecked_mix(bc, cd, T);
    tessCoord = unchecked_mix(abc, bcd, T);

    // If we went with T=parametricT, then update theta. Otherwise leave it
    // at the polar theta found previously. (In the event that
    // parametricT == polarT, we keep the polar theta.)
    if (T != polarT)
    {
      spokeTangent = bcd - abc;
      if (!isDepthStencilSpoke(dsStrokeOutsetScale))
        theta = atan2(spokeTangent);
    }
  }

  uint tessData = 0u;
  uint outContourIDWithFlags = contourIDWithFlags;
  if (!isDepthStencilSpoke(dsStrokeOutsetScale))
  {
    if ((contourIDWithFlags & TESS_MASK_JOIN_TYPE) == TESS_FLAG_FEATHER_JOIN)
    {
      // Feather joins work out their stepping in the vertex shader, so we
      // emit the original tessellation parameters instead of just the tangent
      // angle and let the vertex shader work it all out.
      // Pack these as integers instead of using packHalf2x16() because the
      // latter does not work on ARM Mali.
      tessData = (uint(mergedSegmentCount) << 16) | uint(mergedVertexID);
    }
    else
    {
      // Otherwise, provide the vertex shader with "tangentAngle:miterRatio"
      // in z, both 16-bit unorm.
      // NOTE: multiply theta by 65536 (NOT 65535) because this is a cyclic
      // function, and 0xffff is the final discrete step before wrapping back
      // to 0. This also makes "& 0xffffu" the natural modular reduction of
      // the angle without having to mod().
      uint theta16 = uint(int(round(theta * (65536. / _2PI)))) & 0xffffu;
      uint miterRatio16 = 0u;
      if ((contourIDWithFlags & TESS_MASK_JOIN_TYPE) > TESS_FLAG_ROUND_JOIN)
      {
        // This spoke belongs to a miter or bevel join, which need the
        // miterRatio.
        float cosJoinAngle = clamp(
            cosine_between_vectors(tangents[0], tangents[1]),
            -1.,
            1.);
        // miterRatio is cos(joinAngle/2). Use a trig identity to find it.
        // NOTE: multiply by 65535 here because miterRatio is 0..1.
        miterRatio16 = uint(round(sqrt((1. + cosJoinAngle) * .5) * 65535.));
      }
      tessData = (theta16 << 16) | miterRatio16;
    }
  }
  else
  {
    float2 norm = dot(spokeTangent, spokeTangent) > .0
                      ? normalize(float2(spokeTangent.y, -spokeTangent.x))
                      : float2(.0, -1.); // atan2(0, 0), matching legacy modes.
    float2 aaDir = norm;

    // The depthStencil TESS_FLAG inputs and outputs alias each other because
    // we ran out of bits. Clear the input flags on our output variable.
    outContourIDWithFlags &= ~TESS_MASK_DS_INPUT_ONLY;

    if (isDepthStencilStroke(dsStrokeOutsetScale))
    {
      // Unlike legacy modes, depthStencil strokes compute their outsets,
      // including joins, here in the tessellator.
      float2 strokeOutset = norm;
      uint joinType = contourIDWithFlags & TESS_MASK_JOIN_TYPE;
      if (joinType > TESS_FLAG_ROUND_JOIN)
      {
        // This spoke belongs to a miter, bevel, or miterClip join.
        float cosJoinAngle = clamp(
            cosine_between_vectors(tangents[0], tangents[1]),
            -1.,
            1.);
        // miterRatio is strokeRadius/miterRadius, which comes out to
        // cos(joinAngle/2). Use the identity: cos(t/2) == sqrt((1 + cos(t))/2).
        float miterRatioPow2 = .5 * cosJoinAngle + .5;
        float miterRatio = sqrt(miterRatioPow2);
        float sinJoinAngleOver2 = sqrt(max(1. - miterRatioPow2, .0));
        if (!isExactlyOneFlagSet(
                contourIDWithFlags,
                TESS_FLAG_JOIN_TANGENT_0,
                TESS_FLAG_LEFT_JOIN))
        {
          sinJoinAngleOver2 = -sinJoinAngleOver2;
        }
        // Find the bisector by rotating norm by joinAngle/2.
        float2x2 rot = float2x2(
            miterRatio,
            sinJoinAngleOver2,
            -sinJoinAngleOver2,
            miterRatio);
        float2 bisector = MUL(rot, norm);
        // Bevels have 2 spokes (0 inner), miters have 3 (1 inner), and
        // miterClips have 4 (2 inner).
        bool isInnerJoinSpoke = (contourIDWithFlags &
                                 TESS_FLAG_JOIN_TANGENT_INNER) != 0u;
        if (joinType == TESS_FLAG_MITER_CLIP_JOIN)
        {
          // For now, square caps are the only miterClips: 180-degree joins with
          // a miter limit of 1. Every spoke sits on a corner at norm +
          // bisector. The inner pair outsets AA along the bisector, the outer
          // pair along norm.
          strokeOutset = norm + bisector;
          if (isInnerJoinSpoke)
            aaDir = bisector;
        }
        else if (
            joinType == TESS_FLAG_BEVEL_JOIN ||
            miterRatio < 1. / RIVE_MITER_LIMIT)
        {
          // Bevel (or a miter past the miter limit). Spokes stay on the
          // curve, except a reverted miter's center, which sits on the bevel
          // edge. They all outset AA along the bisector.
          if (isInnerJoinSpoke)
            strokeOutset = bisector * miterRatio;
          aaDir = bisector;
          // The vertex shader treats MSAA bevels (and butt caps) as a special
          // case and manually inkbleeds them 1/2px. MSAA doesn't need inkbleed,
          // but every analytic AA mode inkbleeds bevels, so we do it in MSAA
          // as well, just to match.
          // NOTE: uniforms.tessellationMode sets the
          // TESS_FLAG_DS_OUT_MSAA_BEVEL bit only in MSAA mode.
          outContourIDWithFlags |= TESS_FLAG_DS_OUT_MSAA_BEVEL &
                                   uniforms.tessellationMode;
        }
        else
        {
          // Miter. Every join spoke sits on the miter corner; the center spoke
          // outsets AA along the bisector, the others along their own sides.
          strokeOutset = bisector * (1. / miterRatio);
          if (isInnerJoinSpoke)
            aaDir = bisector;
        }
      }

      // Now that the strokes have been outset in path space, finish
      // transforming them to screen space.
      // (p0..p3 already arrived pre-translated, so we just need the 2x2.)
      float2x2 M = make_float2x2(v_dsViewMatrix);
      tessCoord = MUL(M, tessCoord);

      // Normals go to screen space as:
      //
      //   screenNorm == M^-T * aaDir == aaDir * adj(M) / det(M)
      //
      // Since scale doesn't matter for aaDir, just use adj(M). A flip for
      // negative determinant occurs below, for both strokes and fills.
      aaDir = MUL(aaDir, adjugate(M));

      // Pack the strokeOutset into 32-bit tessData. Since we are limited in
      // precision, use unorm16 normalized relative to
      // "strokeRadius * matrixMaxScale". i.e.:
      //
      //   [-RIVE_MITER_LIMIT, +RIVE_MITER_LIMIT] => [0, 0xffff]
      //
      // (This works because strokeOutset is never wider than the miter limit.)
      //
      // At 1000px wide, this errs by up to ~0.03px everywhere.
      // fp16 would err by up to ~0.12px on normal spokes and ~0.5px on miters.
      tessData = packUnorm2x16(MUL(M, strokeOutset) * dsStrokeOutsetScale + .5);
    }

    // Fill control points came into this stage fully transformed, and strokes
    // just barely finished transforming into screen space. In both cases,
    // everything is now screen space except that we still need to flip aaDir
    // in the case of a left-handed viewMatrix (and for different reasons):
    //
    //   Fills: We blindly rotated the tangent 90 deg with [y, -x].
    //          For a left-handed matrix, that should have been [-y, x].
    //
    //   Strokes: We transformed aaDir by adj(M), ignoring sign(det(M)).
    //            (See above.)
    //
    if ((contourIDWithFlags & TESS_FLAG_NEGATIVE_DETERMINANT) != 0u)
      aaDir = -aaDir;

    // AA always outsets to a 45 degree corner in screen space. When x or y are
    // 0, we therefore have to arbitrarily pick a corner. Picking the sign of
    // the other component seems like a reasonable-enough rule that renders
    // deterministically and evenly splits which side we pick.
    if (aaDir.x == .0)
      aaDir.x = aaDir.y;
    if (aaDir.y == .0)
      aaDir.y = aaDir.x;

    // The vertex shader only needs to know which of 4 screen-space corners to
    // outset AA into. (2 bits.)
    if (aaDir.x < .0)
      outContourIDWithFlags |= TESS_FLAG_DS_OUT_AA_LEFT;
    if (aaDir.y < .0)
      outContourIDWithFlags |= TESS_FLAG_DS_OUT_AA_UP;
  }
  EMIT_FRAG_DATA(
      uint4(floatBitsToUint(tessCoord), tessData, outContourIDWithFlags));
}
#endif
