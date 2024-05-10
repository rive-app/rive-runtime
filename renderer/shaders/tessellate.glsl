/*
 * Copyright 2020 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:src/gpu/ganesh/tessellate/GrStrokeTessellationShader.cpp
 *
 * Copyright 2022 Rive
 */

#define MAX_PARAMETRIC_SEGMENTS_LOG2 10 // Max 1024 segments.

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, float4, @a_p0p1_); // End in '_' because D3D interprets the '1' as a semantic index.
ATTR(1, float4, @a_p2p3_);
ATTR(2, float4, @a_joinTan_and_ys); // [joinTangent, y, reflectionY]
ATTR(3, uint4, @a_args);            // [x0x1, reflectionX0X1, segmentCounts, contourIDWithFlags]
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float4, v_p0p1);
NO_PERSPECTIVE VARYING(1, float4, v_p2p3);
NO_PERSPECTIVE VARYING(2, float4, v_args);     // [vertexIdx, totalVertexCount, joinSegmentCount,
                                               //  parametricSegmentCount, radsPerPolarSegment]
NO_PERSPECTIVE VARYING(3, float3, v_joinArgs); // [joinTangent, radsPerJoinSegment]
FLAT VARYING(4, uint, v_contourIDWithFlags);
VARYING_BLOCK_END

// Tangent of the curve at T=0 and T=1.
INLINE float2x2 find_tangents(float2 p0, float2 p1, float2 p2, float2 p3)
{
    float2x2 t;
    t[0] = (any(notEqual(p0, p1)) ? p1 : any(notEqual(p1, p2)) ? p2 : p3) - p0;
    t[1] = p3 - (any(notEqual(p3, p2)) ? p2 : any(notEqual(p2, p1)) ? p1 : p0);
    return t;
}

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x4(PATH_BUFFER_IDX, PathBuffer, @pathBuffer);
STORAGE_BUFFER_U32x4(CONTOUR_BUFFER_IDX, ContourBuffer, @contourBuffer);
VERTEX_STORAGE_BUFFER_BLOCK_END

float cosine_between_vectors(float2 a, float2 b)
{
    // FIXME(crbug.com/800804,skbug.com/11268): This can overflow if we don't normalize exponents.
    float ab_cosTheta = dot(a, b);
    float ab_pow2 = dot(a, a) * dot(b, b);
    return (ab_pow2 == .0) ? 1. : clamp(ab_cosTheta * inversesqrt(ab_pow2), -1., 1.);
}

VERTEX_MAIN(@tessellateVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    // Each instance repeats twice. Once for normal patch(es) and once for reflection(s).
    ATTR_UNPACK(_instanceID, attrs, @a_p0p1_, float4);
    ATTR_UNPACK(_instanceID, attrs, @a_p2p3_, float4);
    ATTR_UNPACK(_instanceID, attrs, @a_joinTan_and_ys, float4);
    ATTR_UNPACK(_instanceID, attrs, @a_args, uint4);

    VARYING_INIT(v_p0p1, float4);
    VARYING_INIT(v_p2p3, float4);
    VARYING_INIT(v_args, float4);
    VARYING_INIT(v_joinArgs, float3);
    VARYING_INIT(v_contourIDWithFlags, uint);

    float2 p0 = @a_p0p1_.xy;
    float2 p1 = @a_p0p1_.zw;
    float2 p2 = @a_p2p3_.xy;
    float2 p3 = @a_p2p3_.zw;
    // Each instance has two spans, potentially for both a forward copy and and reflection.
    // (If the second span isn't needed, the client will have placed it offscreen.)
    bool isFirstSpan = _vertexID < 4;
    float y = isFirstSpan ? @a_joinTan_and_ys.z : @a_joinTan_and_ys.w;
    int x0x1 = int(isFirstSpan ? @a_args.x : @a_args.y);
#ifdef GLSL
    int x1up = x0x1 << 16;
    if (@a_args.z == 0xffffffffu)
    {
        // Pixel 8 with ARM Mali-G715 throws away "x0x1 << 16 >> 16". We need this in order to
        // sign-extend the bottom 16 bits of x0x1.
        // Create a branch that we know won't be taken, in order to convince the compiler not to
        // throw this operation away.
        // NOTE: we could use bitfieldExtract(), but it isn't available on ES 3.0.
        --x1up;
    }
    float x0 = float(x1up >> 16);
#else
    float x0 = float(x0x1 << 16 >> 16);
#endif
    float x1 = float(x0x1 >> 16);
    float2 coord = float2((_vertexID & 1) == 0 ? x0 : x1, (_vertexID & 2) == 0 ? y + 1. : y);

    uint parametricSegmentCount = @a_args.z & 0x3ffu;
    uint polarSegmentCount = (@a_args.z >> 10) & 0x3ffu;
    uint joinSegmentCount = @a_args.z >> 20;
    uint contourIDWithFlags = @a_args.w;
    if (x1 < x0) // Reflections are drawn right to left.
    {
        contourIDWithFlags |= MIRRORED_CONTOUR_CONTOUR_FLAG;
    }
    if ((x1 - x0) * uniforms.tessInverseViewportY < .0)
    {
        // Make sure we always emit clockwise triangles. Swap the top and bottom vertices.
        coord.y = 2. * y + 1. - coord.y;
    }
    if ((contourIDWithFlags & CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG) != 0u)
    {
        // This span may have more tessellation vertices allocated to it than necessary (e.g.,
        // outerCurve patches all have a fixed patch size, regardless of how many segments the curve
        // actually needs). Re-run Wang's formula to figure out how many segments we actually need,
        // and make any excess segments degenerate by co-locating their vertices at T=0.
        uint pathIDBits =
            STORAGE_BUFFER_LOAD4(@contourBuffer, contour_data_idx(contourIDWithFlags)).z;
        float2x2 mat =
            make_float2x2(uintBitsToFloat(STORAGE_BUFFER_LOAD4(@pathBuffer, pathIDBits * 2u)));
        float2 d0 = MUL(mat, -2. * p1 + p2 + p0);

        float2 d1 = MUL(mat, -2. * p2 + p3 + p1);
        float m = max(dot(d0, d0), dot(d1, d1));
        float n = max(ceil(sqrt(.75 * 4. * sqrt(m))), 1.);
        parametricSegmentCount = min(uint(n), parametricSegmentCount);
    }
    // Polar and parametric segments share the same beginning and ending vertices, so the merged
    // *vertex* count is equal to the sum of polar and parametric *segment* counts.
    uint totalVertexCount = parametricSegmentCount + polarSegmentCount + joinSegmentCount - 1u;

    float2x2 tangents = find_tangents(p0, p1, p2, p3);
    float theta = acos(cosine_between_vectors(tangents[0], tangents[1]));
    float radsPerPolarSegment = theta / float(polarSegmentCount);
    // Adjust sign of radsPerPolarSegment to match the direction the curve turns.
    // NOTE: Since the curve is not allowed to inflect, we can just check F'(.5) x F''(.5).
    // NOTE: F'(.5) x F''(.5) has the same sign as (p2 - p0) x (p3 - p1).
    float turn = determinant(float2x2(p2 - p0, p3 - p1));
    if (turn == .0) // This is the case for joins and cusps where points are co-located.
        turn = determinant(tangents);
    if (turn < .0)
        radsPerPolarSegment = -radsPerPolarSegment;

    v_p0p1 = float4(p0, p1);
    v_p2p3 = float4(p2, p3);
    v_args = float4(float(totalVertexCount) - abs(x1 - coord.x), // vertexIdx
                    float(totalVertexCount),                     // totalVertexCount
                    (joinSegmentCount << 10) | parametricSegmentCount,
                    radsPerPolarSegment);
    if (joinSegmentCount > 1u)
    {
        float2x2 joinTangents = float2x2(tangents[1], @a_joinTan_and_ys.xy);
        float joinTheta = acos(cosine_between_vectors(joinTangents[0], joinTangents[1]));
        float joinSpan = float(joinSegmentCount);
        if ((contourIDWithFlags & (JOIN_TYPE_MASK | EMULATED_STROKE_CAP_CONTOUR_FLAG)) ==
            EMULATED_STROKE_CAP_CONTOUR_FLAG)
        {
            // Round caps emulated as joins need to emit vertices at T=0 and T=1, unlike normal
            // round joins. The fragment shader will handle most of this, but here we need to adjust
            // radsPerJoinSegment to account for the fact that this join will be rotating around two
            // more segments.
            joinSpan -= 2.;
        }
        float radsPerJoinSegment = joinTheta / joinSpan;
        if (determinant(joinTangents) < .0)
            radsPerJoinSegment = -radsPerJoinSegment;
        v_joinArgs.xy = @a_joinTan_and_ys.xy;
        v_joinArgs.z = radsPerJoinSegment;
    }
    v_contourIDWithFlags = contourIDWithFlags;

    float4 pos;
    pos.x = coord.x * (2. / TESS_TEXTURE_WIDTH) - 1.;
    pos.y = coord.y * uniforms.tessInverseViewportY - sign(uniforms.tessInverseViewportY);
    pos.zw = float2(0, 1);

    VARYING_PACK(v_p0p1);
    VARYING_PACK(v_p2p3);
    VARYING_PACK(v_args);
    VARYING_PACK(v_joinArgs);
    VARYING_PACK(v_contourIDWithFlags);
    EMIT_VERTEX(pos);
}
#endif

#ifdef @FRAGMENT
FRAG_DATA_MAIN(uint4, @tessellateFragmentMain)
{
    VARYING_UNPACK(v_p0p1, float4);
    VARYING_UNPACK(v_p2p3, float4);
    VARYING_UNPACK(v_args, float4);
    VARYING_UNPACK(v_joinArgs, float3);
    VARYING_UNPACK(v_contourIDWithFlags, uint);

    float2 p0 = v_p0p1.xy;
    float2 p1 = v_p0p1.zw;
    float2 p2 = v_p2p3.xy;
    float2 p3 = v_p2p3.zw;
    float2x2 tangents = find_tangents(p0, p1, p2, p3);
    // Colocate any padding vertices at T=0.
    float vertexIdx = max(floor(v_args.x), .0);
    float totalVertexCount = v_args.y;
    uint joinSegmentCount_and_parametricSegmentCount = uint(v_args.z);
    float parametricSegmentCount = float(joinSegmentCount_and_parametricSegmentCount & 0x3ffu);
    float joinSegmentCount = float(joinSegmentCount_and_parametricSegmentCount >> 10);
    float radsPerPolarSegment = v_args.w;
    uint contourIDWithFlags = v_contourIDWithFlags;

    // mergedVertexID/mergedSegmentCount are relative to the sub-section of the instance this vertex
    // belongs to (either the curve section that consists of merged polar and parametric segments,
    // or the join section composed of just polar segments).
    //
    // Begin with the assumption that we belong to the curve section.
    float mergedSegmentCount = totalVertexCount - joinSegmentCount;
    float mergedVertexID = vertexIdx;
    if (mergedVertexID <= mergedSegmentCount)
    {
        // We do belong to the curve section. Clear out any stroke join flags.
        contourIDWithFlags &= ~JOIN_TYPE_MASK;
    }
    else
    {
        // We actually belong to the join section following the curve. Construct a point-cubic with
        // rotation.
        p0 = p1 = p2 = p3;
        tangents = float2x2(tangents[1], v_joinArgs.xy /*joinTangent*/);
        parametricSegmentCount = 1.;
        mergedVertexID -= mergedSegmentCount;
        mergedSegmentCount = joinSegmentCount;
        if ((contourIDWithFlags & JOIN_TYPE_MASK) != 0u)
        {
            // Miter or bevel join vertices snap to either tangents[0] or tangents[1], and get
            // adjusted in the shader that follows.
            if (mergedVertexID < 2.5) // With 5 join segments, this branch will see IDs: 1, 2, 3, 4.
                contourIDWithFlags |= JOIN_TANGENT_0_CONTOUR_FLAG;
            if (mergedVertexID > 1.5 && mergedVertexID < 3.5)
                contourIDWithFlags |= JOIN_TANGENT_INNER_CONTOUR_FLAG;
        }
        else if ((contourIDWithFlags & EMULATED_STROKE_CAP_CONTOUR_FLAG) != 0u)
        {
            // Round caps emulated as joins need to emit vertices at T=0 and T=1, unlike normal
            // round joins. Preserve the same number of vertices (the CPU should have given us two
            // extra, knowing that we are an emulated cap, and the vertex shader should have already
            // accounted for this in radsPerJoinSegment), but adjust our stepping parameters so we
            // begin at T=0 and end at T=1.
            mergedSegmentCount -= 2.;
            mergedVertexID--;
        }
        radsPerPolarSegment = v_joinArgs.z; // radsPerJoinSegment.
        contourIDWithFlags |=
            radsPerPolarSegment < .0 ? LEFT_JOIN_CONTOUR_FLAG : RIGHT_JOIN_CONTOUR_FLAG;
    }

    float2 tessCoord;
    float theta = .0;
    if (mergedVertexID == .0 || mergedVertexID == mergedSegmentCount ||
        (contourIDWithFlags & JOIN_TYPE_MASK) != 0u)
    {
        // Tessellated vertices at the beginning and end of the strip use exact endpoints and
        // tangents. This ensures crack-free seaming between instances.
        bool isTan0 = mergedVertexID < mergedSegmentCount * .5;
        tessCoord = isTan0 ? p0 : p3;
        theta = atan2(isTan0 ? tangents[0] : tangents[1]);
    }
    else if ((contourIDWithFlags & RETROFITTED_TRIANGLE_CONTOUR_FLAG) != 0u)
    {
        // This cubic should actually be drawn as the single, non-AA triangle: [p0, p1, p3].
        // This is used to squeeze in more rare triangles, like "grout" triangles from self
        // intersections on interior triangulation, where it wouldn't be worth it to put them in
        // their own dedicated draw call.
        tessCoord = p1;
    }
    else
    {
        float T, polarT;
        if (parametricSegmentCount == mergedSegmentCount)
        {
            // There are no polar vertices. This is (probably) a fill. Vertices are spaced evenly in
            // parametric space.
            T = mergedVertexID / parametricSegmentCount;
            polarT = .0; // Set polarT != T to ensure we calculate the parametric tangent later.
        }
        else
        {
            // Compute the location and tangent direction of the tessellated stroke vertex with the
            // integral id "mergedVertexID", where mergedVertexID is the sorted-order index of
            // parametric and polar vertices. Start by finding the tangent function's power basis
            // coefficients. These define a tangent direction (scaled by some uniform value) as:
            //
            //                                                 |T^2|
            //     Tangent_Direction(T) = dx,dy = |A  2B  C| * |T  |
            //                                    |.   .  .|   |1  |
            float2 A, B, C = p1 - p0;
            float2 D = p3 - p0;
            float2 E = p2 - p1;
            B = E - C;
            A = -3. * E + D;
            // FIXME(crbug.com/800804,skbug.com/11268): Consider normalizing the exponents in A,B,C
            // at this point in order to prevent fp32 overflow.

            // Now find the coefficients that give a tangent direction from a parametric vertex ID:
            //
            //                                                                |parametricVertexID^2|
            //  Tangent_Direction(parametricVertexID) = dx,dy = |A  B_  C_| * |parametricVertexID  |
            //                                                  |.   .   .|   |1                   |
            //
            float2 B_ = B * (parametricSegmentCount * 2.);
            float2 C_ = C * (parametricSegmentCount * parametricSegmentCount);

            // Run a binary search to determine the highest parametric vertex that is located on or
            // before the mergedVertexID. A merged ID is determined by the sum of complete
            // parametric and polar segments behind it. i.e., find the highest parametric vertex
            // where:
            //
            //    parametricVertexID + floor(numPolarSegmentsAtParametricT) <= mergedVertexID
            //
            float lastParametricVertexID = .0;
            float maxParametricVertexID = min(parametricSegmentCount - 1., mergedVertexID);
            // FIXME(crbug.com/800804,skbug.com/11268): This normalize() can overflow.
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
                    float maxRotation = testParametricID * negAbsRadsPerSegment + maxRotation0;
                    maxRotation = min(maxRotation, PI);
                    // Is rotation <= maxRotation? (i.e., is the number of complete polar segments
                    // behind testT, + testParametricID <= mergedVertexID?)
                    if (cosRotation >= cos(maxRotation))
                        lastParametricVertexID = testParametricID;
                }
            }

            // Find the T value of the parametric vertex at lastParametricVertexID.
            float parametricT = lastParametricVertexID / parametricSegmentCount;

            // Now that we've identified the highest parametric vertex on or before the
            // mergedVertexID, the highest polar vertex is easy:
            float lastPolarVertexID = mergedVertexID - lastParametricVertexID;

            // Find the angle of tan0, or the angle between tan0norm and the positive x axis.
            float theta0 = acos(clamp(tan0norm.x, -1., 1.));
            theta0 = tan0norm.y >= .0 ? theta0 : -theta0;

            // Find the tangent vector on the vertex at lastPolarVertexID.
            theta = lastPolarVertexID * radsPerPolarSegment + theta0;
            float2 norm = float2(sin(theta), -cos(theta));

            // Find the T value where the tangent is orthogonal to norm. This is a quadratic:
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

            // Roots are q/a and c/q. Since each curve section does not inflect or rotate more than
            // 180 degrees, there can only be one tangent orthogonal to "norm" inside 0..1. Pick the
            // root nearest .5.
            float _5qa = -.5 * q * a;
            float2 root = (abs(q * q + _5qa) < abs(a * c + _5qa)) ? float2(q, a) : float2(c, q);
            polarT = (root.t != .0) ? root.s / root.t : .0;
            polarT = clamp(polarT, .0, 1.);

            // The root finder above can become unstable when lastPolarVertexID == 0 (e.g., if there
            // are roots at exatly 0 and 1 both). polarT should always == 0 in this case.
            if (lastPolarVertexID == .0)
                polarT = .0;

            // Now that we've identified the T values of the last parametric and polar vertices, our
            // final T value for mergedVertexID is whichever is larger.
            T = max(parametricT, polarT);
        }

        // Evaluate the cubic at T. Use De Casteljau's for its accuracy and stability.
        float2 ab = unchecked_mix(p0, p1, T);
        float2 bc = unchecked_mix(p1, p2, T);
        float2 cd = unchecked_mix(p2, p3, T);
        float2 abc = unchecked_mix(ab, bc, T);
        float2 bcd = unchecked_mix(bc, cd, T);
        tessCoord = unchecked_mix(abc, bcd, T);

        // If we went with T=parametricT, then update theta. Otherwise leave it at the polar theta
        // found previously. (In the event that parametricT == polarT, we keep the polar theta.)
        if (T != polarT)
            theta = atan2(bcd - abc);
    }

    EMIT_FRAG_DATA(uint4(floatBitsToUint(float3(tessCoord, theta)), contourIDWithFlags));
}
#endif
