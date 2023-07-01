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
ATTR(0) float4 attr_p0p1;
ATTR(1) float4 attr_p2p3;
ATTR(2) float4 attr_joinTangent;
ATTR(3) uint4 attr_args; // [x0, x1, y, polarSegmentCount, contourIDWithFlags]
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING float4 p0p1;
NO_PERSPECTIVE VARYING float4 p2p3;
NO_PERSPECTIVE VARYING float4 args;     // [vertexIdx, totalVertexCount, joinSegmentCount,
                                        // parametricSegmentCount, radsPerPolarSegment]
NO_PERSPECTIVE VARYING float3 joinArgs; // [joinTangent, radsPerJoinSegment]
FLAT VARYING uint contourIDWithFlags;
VARYING_BLOCK_END

// Tangent of the curve at T=0 and T=1.
float2x2 find_tangents(float4x2 p)
{
    float2x2 t;
    t[0] = (any(notEqual(p[0], p[1])) ? p[1] : any(notEqual(p[1], p[2])) ? p[2] : p[3]) - p[0];
    t[1] = p[3] - (any(notEqual(p[3], p[2])) ? p[2] : any(notEqual(p[2], p[1])) ? p[1] : p[0]);
    return t;
}

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
TEXTURE_RGBA32UI(1) @pathTexture;
TEXTURE_RGBA32UI(2) @contourTexture;
VERTEX_TEXTURE_BLOCK_END

float cosine_between_vectors(float2 a, float2 b)
{
    // FIXME(crbug.com/800804,skbug.com/11268): This can overflow if we don't normalize exponents.
    float ab_cosTheta = dot(a, b);
    float ab_pow2 = dot(a, a) * dot(b, b);
    return (ab_pow2 == .0) ? 1. : clamp(ab_cosTheta * inversesqrt(ab_pow2), -1., 1.);
}

VERTEX_MAIN(
#ifdef METAL
    @tessellateVertexMain,
    Varyings,
    varyings,
    uint VERTEX_ID [[vertex_id]],
    uint INSTANCE_ID [[instance_id]],
    constant @Uniforms& uniforms [[buffer(0)]],
    constant Attrs* attrs [[buffer(1)]],
    VertexTextures textures
#endif
)
{
    ATTR_LOAD(float4, attrs, attr_p0p1, INSTANCE_ID);
    ATTR_LOAD(float4, attrs, attr_p2p3, INSTANCE_ID);
    ATTR_LOAD(uint4, attrs, attr_args, INSTANCE_ID);
    ATTR_LOAD(float4, attrs, attr_joinTangent, INSTANCE_ID);

    float4x2 p = float4x2(attr_p0p1.xy, attr_p0p1.zw, attr_p2p3.xy, attr_p2p3.zw);
    float x0 = float(int(attr_args.x << 16) >> 16);
    float x1 = float(int(attr_args.x) >> 16);
    float y = float(attr_args.y);
    float2 coord = float2((VERTEX_ID & 1) == 0 ? x0 : x1, (VERTEX_ID & 2) == 0 ? y : y + 1.);

    uint parametricSegmentCount = attr_args.z & 0x3ffu;
    uint polarSegmentCount = (attr_args.z >> 10) & 0x3ffu;
    uint joinSegmentCount = attr_args.z >> 20;
    uint outContourIDWithFlags = attr_args.w;
    if ((outContourIDWithFlags & CULL_EXCESS_TESSELLATION_SEGMENTS_FLAG) != 0u)
    {
        // This span may have more tessellation vertices allocated to it than necessary (e.g.,
        // outerCurve patches all have a fixed patch size, regardless of how many segments the curve
        // actually needs). Re-run Wang's formula to figure out how many segments we actually need,
        // and make any excess segments degenerate by co-locating their vertices at T=0.
        uint pathIDBits =
            TEXEL_FETCH(textures, @contourTexture, contour_texel_coord(outContourIDWithFlags), 0).z;
        float2x2 matrix = make_float2x2(
            uintBitsToFloat(TEXEL_FETCH(textures, @pathTexture, path_texel_coord(pathIDBits), 0)));
        float2 d0 = matrix * (-2. * p[1] + p[2] + p[0]);
        float2 d1 = matrix * (-2. * p[2] + p[3] + p[1]);
        float m = max(dot(d0, d0), dot(d1, d1));
        float n = max(ceil(sqrt(.75 * 4. * sqrt(m))), 1.);
        parametricSegmentCount = min(uint(n), parametricSegmentCount);
    }
    // Polar and parametric segments share the same beginning and ending vertices, so the merged
    // *vertex* count is equal to the sum of polar and parametric *segment* counts.
    uint totalVertexCount = parametricSegmentCount + polarSegmentCount + joinSegmentCount - 1u;

    float2x2 tangents = find_tangents(p);
    float theta = acos(cosine_between_vectors(tangents[0], tangents[1]));
    float radsPerPolarSegment = theta / float(polarSegmentCount);
    // Adjust sign of radsPerPolarSegment to match the direction the curve turns.
    // NOTE: Since the curve is not allowed to inflect, we can just check F'(.5) x F''(.5).
    // NOTE: F'(.5) x F''(.5) has the same sign as (p[2] - p[0]) x (p[3] - p[1]).
    float turn = determinant(float2x2(p[2] - p[0], p[3] - p[1]));
    if (turn == .0) // This is the case for joins and cusps where points are co-located.
        turn = determinant(tangents);
    if (turn < .0)
        radsPerPolarSegment = -radsPerPolarSegment;

    FLD(varyings, p0p1) = float4(p[0], p[1]);
    FLD(varyings, p2p3) = float4(p[2], p[3]);
    FLD(varyings, args) = float4(float(totalVertexCount) + coord.x - x1, // vertexIdx
                                 float(totalVertexCount),                // totalVertexCount
                                 (joinSegmentCount << 10) | parametricSegmentCount,
                                 radsPerPolarSegment);
    if (joinSegmentCount > 1u)
    {
        float2x2 joinTangents = float2x2(tangents[1], attr_joinTangent.xy);
        float joinTheta = acos(cosine_between_vectors(joinTangents[0], joinTangents[1]));
        float joinSpan = float(joinSegmentCount);
        if ((outContourIDWithFlags & (JOIN_TYPE_MASK | EMULATED_STROKE_CAP_FLAG)) ==
            EMULATED_STROKE_CAP_FLAG)
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
        FLD(varyings, joinArgs).xy = attr_joinTangent.xy;
        FLD(varyings, joinArgs).z = radsPerJoinSegment;
    }
    FLD(varyings, contourIDWithFlags) = outContourIDWithFlags;

    POSITION.xy = coord * float2(2. / TESS_TEXTURE_WIDTH, uniforms.tessInverseViewportY) - 1.;
    POSITION.zw = float2(0, 1);
    EMIT_OFFSCREEN_VERTEX(varyings);
}
#endif

#ifdef @FRAGMENT
FRAG_DATA_TYPE(uint4)
FRAG_DATA_MAIN(
#ifdef METAL
    @tessellateFragmentMain,
    Varyings varyings [[stage_in]]
#endif
)
{
    float4x2 p = make_float4x2(FLD(varyings, p0p1), FLD(varyings, p2p3));
    float2x2 tangents = find_tangents(p);
    // Colocate any padding vertices at T=0.
    float vertexIdx = max(floor(FLD(varyings, args).x), .0);
    float totalVertexCount = FLD(varyings, args).y;
    uint joinSegmentCount_and_parametricSegmentCount = uint(FLD(varyings, args).z);
    float parametricSegmentCount = float(joinSegmentCount_and_parametricSegmentCount & 0x3ffu);
    float joinSegmentCount = float(joinSegmentCount_and_parametricSegmentCount >> 10);
    float radsPerPolarSegment = FLD(varyings, args).w;
    uint outContourIDWithFlags = FLD(varyings, contourIDWithFlags);
    // PLSRenderer sends down the first curve of each contour with the
    // "FIRST_VERTEX_OF_CONTOUR_FLAG" flag. But from here we need to only output this flag for the
    // first *vertex* of that first curve.
    if (vertexIdx != .0)
        outContourIDWithFlags &= ~FIRST_VERTEX_OF_CONTOUR_FLAG;

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
        outContourIDWithFlags &= ~JOIN_TYPE_MASK;
    }
    else
    {
        // We actually belong to the join section following the curve. Construct a point-cubic with
        // rotation.
        p = float4x2(p[3], p[3], p[3], p[3]);
        tangents = float2x2(tangents[1], FLD(varyings, joinArgs).xy /*joinTangent*/);
        parametricSegmentCount = 1.;
        mergedVertexID -= mergedSegmentCount;
        mergedSegmentCount = joinSegmentCount;
        if ((outContourIDWithFlags & JOIN_TYPE_MASK) != 0u)
        {
            // Miter or bevel join vertices snap to either tangents[0] or tangents[1], and get
            // adjusted in the shader that follows.
            if (mergedVertexID < 2.5) // With 5 join segments, this branch will see IDs: 1, 2, 3, 4.
                outContourIDWithFlags |= JOIN_TANGENT_0_FLAG;
            if (mergedVertexID > 1.5 && mergedVertexID < 3.5)
                outContourIDWithFlags |= JOIN_TANGENT_INNER_FLAG;
        }
        else if ((outContourIDWithFlags & EMULATED_STROKE_CAP_FLAG) != 0u)
        {
            // Round caps emulated as joins need to emit vertices at T=0 and T=1, unlike normal
            // round joins. Preserve the same number of vertices (the CPU should have given us two
            // extra, knowing that we are an emulated cap, and the vertex shader should have already
            // accounted for this in radsPerJoinSegment), but adjust our stepping parameters so we
            // begin at T=0 and end at T=1.
            mergedSegmentCount -= 2.;
            mergedVertexID--;
        }
        radsPerPolarSegment = FLD(varyings, joinArgs).z; // radsPerJoinSegment.
        outContourIDWithFlags |= radsPerPolarSegment < .0 ? LEFT_JOIN_FLAG : RIGHT_JOIN_FLAG;
    }

    float2 tessCoord;
    float theta;
    if (mergedVertexID == .0 || mergedVertexID == mergedSegmentCount ||
        (outContourIDWithFlags & JOIN_TYPE_MASK) != 0u)
    {
        // Tessellated vertices at the beginning and end of the strip use exact endpoints and
        // tangents. This ensures crack-free seaming between instances.
        bool isTan0 = mergedVertexID < mergedSegmentCount * .5;
        tessCoord = isTan0 ? p[0] : p[3];
        theta = atan2(isTan0 ? tangents[0] : tangents[1]);
    }
    else if ((outContourIDWithFlags & RETROFITTED_TRIANGLE_FLAG) != 0u)
    {
        // This cubic should actually be drawn as the single, non-AA triangle: [p0, p1, p3].
        // This is used to squeeze in more rare triangles, like "breadcrumb" triangles from
        // self-intersections on interior triangulation, where it wouldn't be worth it to put them
        // in their own dedicated draw call.
        tessCoord = p[1];
        theta = .0;
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
            float2 A, B, C = p[1] - p[0];
            float2 D = p[3] - p[0];
            float2 E = p[2] - p[1];
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
        float2 ab = unchecked_mix(p[0], p[1], T);
        float2 bc = unchecked_mix(p[1], p[2], T);
        float2 cd = unchecked_mix(p[2], p[3], T);
        float2 abc = unchecked_mix(ab, bc, T);
        float2 bcd = unchecked_mix(bc, cd, T);
        tessCoord = unchecked_mix(abc, bcd, T);

        // If we went with T=parametricT, then update theta. Otherwise leave it at the polar theta
        // found previously. (In the event that parametricT == polarT, we keep the polar theta.)
        if (T != polarT)
            theta = atan2(bcd - abc);
    }

    EMIT_FRAG_DATA(uint4(floatBitsToUint(float3(tessCoord, theta)), outContourIDWithFlags));
}
#endif
