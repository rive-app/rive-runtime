/*
 * Copyright 2023 Rive
 */

// Common functions shared by draw shaders.

#ifdef @VERTEX

#ifdef @DRAW_PATH
INLINE int2 tess_texel_coord(int texelIndex)
{
    return int2(texelIndex & ((1 << TESS_TEXTURE_WIDTH_LOG2) - 1),
                texelIndex >> TESS_TEXTURE_WIDTH_LOG2);
}

INLINE float calc_aa_radius(float2x2 M, float2 normalized)
{

    float2 v = MUL(M, normalized);
    return (abs(v.x) + abs(v.y)) * (1. / dot(v, v)) * AA_RADIUS;
}

INLINE bool unpack_tessellated_path_vertex(float4 patchVertexData,
                                           float4 mirroredVertexData,
                                           int _instanceID,
                                           TEX32UIREF tessVertexTexture,
                                           TEX32UIREF pathTexture,
                                           TEX32UIREF contourTexture,
                                           OUT(ushort) o_pathIDBits,
                                           OUT(int2) o_pathTexelCoord,
                                           OUT(float2x2) o_M,
                                           OUT(float2) o_translate,
                                           OUT(uint) o_pathParams,
                                           OUT(half2) o_edgeDistance,
                                           OUT(float2) o_vertexPosition)
{
    // Unpack patchVertexData.
    int localVertexID = int(patchVertexData.x);
    float outset = patchVertexData.y;
    float fillCoverage = patchVertexData.z;
    int patchSegmentSpan = floatBitsToInt(patchVertexData.w) >> 2;
    int vertexType = floatBitsToInt(patchVertexData.w) & 3;

    // Fetch a vertex that definitely belongs to the contour we're drawing.
    int vertexIDOnContour = min(localVertexID, patchSegmentSpan - 1);
    int tessVertexIdx = _instanceID * patchSegmentSpan + vertexIDOnContour;
    uint4 tessVertexData = TEXEL_FETCH(tessVertexTexture, tess_texel_coord(tessVertexIdx));
    uint contourIDWithFlags = tessVertexData.w;

    // Fetch and unpack the contour referenced by the tessellation vertex.
    uint4 contourData = TEXEL_FETCH(contourTexture, contour_texel_coord(contourIDWithFlags));
    float2 midpoint = uintBitsToFloat(contourData.xy);
    o_pathIDBits = make_ushort(contourData.z);
    uint vertexIndex0 = contourData.w;

    // Fetch and unpack the path.
    o_pathTexelCoord = path_texel_coord(o_pathIDBits);
    o_M = make_float2x2(uintBitsToFloat(TEXEL_FETCH(pathTexture, o_pathTexelCoord)));
    uint4 pathData = TEXEL_FETCH(pathTexture, o_pathTexelCoord + int2(1, 0));
    o_translate = uintBitsToFloat(pathData.xy);
    o_pathParams = pathData.w;

    float strokeRadius = uintBitsToFloat(pathData.z);

    // Fix the tessellation vertex if we fetched the wrong one in order to guarantee we got the
    // correct contour ID and flags, or if we belong to a mirrored contour and this vertex has an
    // alternate position when mirrored.
    uint mirroredContourFlag = contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG;
    if (mirroredContourFlag != 0u)
    {
        localVertexID = int(mirroredVertexData.x);
        outset = mirroredVertexData.y;
        fillCoverage = mirroredVertexData.z;
    }
    if (localVertexID != vertexIDOnContour)
    {
        // This can peek one vertex before or after the contour, but the tessellator guarantees
        // there is always at least one padding vertex at the beginning and end of the data.
        tessVertexIdx += localVertexID - vertexIDOnContour;
        uint4 replacementTessVertexData =
            TEXEL_FETCH(tessVertexTexture, tess_texel_coord(tessVertexIdx));
        if ((replacementTessVertexData.w & 0xffffu) != (contourIDWithFlags & 0xffffu))
        {
            // We crossed over into a new contour. Either wrap to the first vertex in the contour or
            // leave it clamped at the final vertex of the contour.
            bool isClosed = strokeRadius == .0 || // filled
                            midpoint.x != .0;     // explicity closed stroke
            if (isClosed)
            {
                tessVertexData =
                    TEXEL_FETCH(tessVertexTexture, tess_texel_coord(int(vertexIndex0)));
            }
        }
        else
        {
            tessVertexData = replacementTessVertexData;
        }
        // MIRRORED_CONTOUR_CONTOUR_FLAG is not preserved at vertexIndex0. Preserve it here. By not
        // preserving this flag, the normal and mirrored contour can both share the same contour
        // record.
        contourIDWithFlags = tessVertexData.w | mirroredContourFlag;
    }

    // Finish unpacking tessVertexData.
    float theta = uintBitsToFloat(tessVertexData.z);
    float2 norm = float2(sin(theta), -cos(theta));
    float2 origin = uintBitsToFloat(tessVertexData.xy);
    float2 postTransformVertexOffset;

    if (strokeRadius != .0) // Is this a stroke?
    {
        // Ensure strokes always emit clockwise triangles.
        outset *= sign(determinant(o_M));

        // Joins only emanate from the outer side of the stroke.
        if ((contourIDWithFlags & LEFT_JOIN_CONTOUR_FLAG) != 0u)
            outset = min(outset, .0);
        if ((contourIDWithFlags & RIGHT_JOIN_CONTOUR_FLAG) != 0u)
            outset = max(outset, .0);

        float aaRadius = calc_aa_radius(o_M, norm);
        half globalCoverage = 1.;
        if (aaRadius > strokeRadius)
        {
            // The stroke is narrower than the AA ramp. Instead of emitting subpixel geometry, make
            // the stroke as wide as the AA ramp and apply a global coverage multiplier.
            globalCoverage = make_half(strokeRadius) / make_half(aaRadius);
            strokeRadius = aaRadius;
        }

        // Extend the vertex by half the width of the AA ramp.
        float2 vertexOffset = MUL(norm, strokeRadius + aaRadius); // Bloat stroke width for AA.

        // Calculate the AA distance to both the outset and inset edges of the stroke. The fragment
        // shader will use whichever is lesser.
        float x = outset * (strokeRadius + aaRadius);
        o_edgeDistance = make_half2((1. / (aaRadius * 2.)) * (float2(x, -x) + strokeRadius) + .5);

        uint joinType = contourIDWithFlags & JOIN_TYPE_MASK;
        if (joinType != 0u)
        {
            // This vertex belongs to a miter or bevel join. Begin by finding the bisector, which is
            // the same as the miter line. The first two vertices in the join peek forward to figure
            // out the bisector, and the final two peek backward.
            int peekDir = 2;
            if ((contourIDWithFlags & JOIN_TANGENT_0_CONTOUR_FLAG) == 0u)
                peekDir = -peekDir;
            if ((contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) != 0u)
                peekDir = -peekDir;
            int2 otherJoinTexelCoord = tess_texel_coord(tessVertexIdx + peekDir);
            uint4 otherJoinData = TEXEL_FETCH(tessVertexTexture, otherJoinTexelCoord);
            float otherJoinTheta = uintBitsToFloat(otherJoinData.z);
            float joinAngle = abs(otherJoinTheta - theta);
            if (joinAngle > PI)
                joinAngle = 2. * PI - joinAngle;
            bool isTan0 = (contourIDWithFlags & JOIN_TANGENT_0_CONTOUR_FLAG) != 0u;
            bool isLeftJoin = (contourIDWithFlags & LEFT_JOIN_CONTOUR_FLAG) != 0u;
            float bisectTheta = joinAngle * (isTan0 == isLeftJoin ? -.5 : .5) + theta;
            float2 bisector = float2(sin(bisectTheta), -cos(bisectTheta));
            float bisectAARadius = calc_aa_radius(o_M, bisector);

            // Generalize everything to a "miter-clip", which is proposed in the SVG-2 draft. Bevel
            // joins are converted to miter-clip joins with a miter limit of 1/2 pixel. They
            // technically bleed out 1/2 pixel when drawn this way, but they seem to look fine and
            // there is not an obvious solution to antialias them without an ink bleed.
            float miterRatio = cos(joinAngle * .5);
            float clipRadius;
            if ((joinType == MITER_CLIP_JOIN_CONTOUR_FLAG) ||
                (joinType == MITER_REVERT_JOIN_CONTOUR_FLAG && miterRatio >= .25))
            {
                // Miter!
                // We currently use hard coded miter limits:
                //   * 1 for square caps being emulated as miter-clip joins.
                //   * 4, which is the SVG default, for all other miter joins.
                float miterInverseLimit =
                    (contourIDWithFlags & EMULATED_STROKE_CAP_CONTOUR_FLAG) != 0u ? 1. : .25;
                clipRadius = strokeRadius * (1. / max(miterRatio, miterInverseLimit));
            }
            else
            {
                // Bevel!
                clipRadius = strokeRadius * miterRatio + /* 1/2px bleed! */ bisectAARadius;
            }
            float clipAARadius = clipRadius + bisectAARadius;
            if ((contourIDWithFlags & JOIN_TANGENT_INNER_CONTOUR_FLAG) != 0u)
            {
                // Reposition the inner join vertices at the miter-clip positions. Leave the outer
                // join vertices as duplicates on the surrounding curve endpoints. We emit duplicate
                // vertex positions because we need a hard stop on the clip distance (see below).
                //
                // Use aaRadius here because we're tracking AA on the mitered edge, NOT the outer
                // clip edge.
                float strokeAARaidus = strokeRadius + aaRadius;
                // clipAARadius must be 1/16 of an AA ramp (~1/16 pixel) longer than the miter
                // length before we start clipping, to ensure we are solving for a numerically
                // stable intersection.
                float slop = aaRadius * .125;
                if (strokeAARaidus <= clipAARadius * miterRatio + slop)
                {
                    // The miter point is before the clip line. Extend out to the miter point.
                    float miterAARadius = strokeAARaidus * (1. / miterRatio);
                    vertexOffset = bisector * miterAARadius;
                }
                else
                {
                    // The clip line is before the miter point. Find where the clip line and the
                    // mitered edge intersect.
                    float2 bisectAAOffset = bisector * clipAARadius;
                    float2 k = float2(dot(vertexOffset, vertexOffset),
                                      dot(bisectAAOffset, bisectAAOffset));
                    vertexOffset = MUL(k, inverse(float2x2(vertexOffset, bisectAAOffset)));
                }
            }
            // The clip distance tells us how to antialias the outer clipped edge. Since joins only
            // emanate from the outset side of the stroke, we can repurpose the inset distance as
            // the clip distance.
            float2 pt = abs(outset) * vertexOffset;
            float clipDistance = (clipAARadius - dot(pt, bisector)) / (bisectAARadius * 2.);
            if ((contourIDWithFlags & LEFT_JOIN_CONTOUR_FLAG) != 0u)
                o_edgeDistance.y = make_half(clipDistance);
            else
                o_edgeDistance.x = make_half(clipDistance);
        }

        o_edgeDistance *= globalCoverage;

        // Bias o_edgeDistance.y slightly upwards in order to guarantee o_edgeDistance.y is >= 0 at
        // every pixel. "o_edgeDistance.y < 0" is used to differentiate between strokes and fills.
        o_edgeDistance.y = max(o_edgeDistance.y, make_half(1e-4));

        postTransformVertexOffset = MUL(o_M, outset * vertexOffset);

        // Throw away the fan triangles since we're a stroke.
        if (vertexType != STROKE_VERTEX)
            return false;
    }
    else // This is a fill.
    {
        // Place the fan point.
        if (vertexType == FAN_MIDPOINT_VERTEX)
            origin = midpoint;

        // Offset the vertex for Manhattan AA.
        postTransformVertexOffset = sign(MUL(o_M, outset * norm)) * AA_RADIUS;

        if ((contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) != 0u)
            fillCoverage = -fillCoverage;

        // "o_edgeDistance.y < 0" indicates to the fragment shader that this is a fill.
        o_edgeDistance = make_half2(fillCoverage, -1);

        // If we're actually just drawing a triangle, throw away the entire patch except a single
        // fan triangle.
        if ((contourIDWithFlags & RETROFITTED_TRIANGLE_CONTOUR_FLAG) != 0u &&
            vertexType != FAN_VERTEX)
            return false;
    }

    o_vertexPosition = MUL(o_M, origin) + postTransformVertexOffset + o_translate;
    return true;
}
#endif // @DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
INLINE float2 unpack_interior_triangle_vertex(float3 triangleVertex,
                                              TEX32UIREF pathTexture,
                                              OUT(ushort) o_pathIDBits,
                                              OUT(int2) o_pathTexelCoord,
                                              OUT(float2x2) o_M,
                                              OUT(float2) o_translate,
                                              OUT(uint) o_pathParams,
                                              OUT(half) o_windingWeight)
{
    o_pathIDBits = make_ushort(floatBitsToUint(triangleVertex.z) & 0xffffu);
    o_pathTexelCoord = path_texel_coord(o_pathIDBits);
    o_M = make_float2x2(uintBitsToFloat(TEXEL_FETCH(pathTexture, o_pathTexelCoord)));
    uint4 pathData = TEXEL_FETCH(pathTexture, o_pathTexelCoord + int2(1, 0));
    o_translate = uintBitsToFloat(pathData.xy);
    o_pathParams = pathData.w;
    o_windingWeight = float(floatBitsToInt(triangleVertex.z) >> 16) * sign(determinant(o_M));
    return MUL(o_M, triangleVertex.xy) + o_translate;
}
#endif // @DRAW_INTERIOR_TRIANGLES

#endif // @VERTEX
