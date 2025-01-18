/*
 * Copyright 2023 Rive
 */

// Common functions shared by draw shaders.

// Feathered coverage values get shifted by "FEATHER_COVERAGE_BIAS" in order
// to classify the coverage as belonging to a feather.
#define FEATHER_COVERAGE_BIAS -2.

// Fragment shaders test if a coverage value is less than
// "FEATHER_COVERAGE_THRESHOLD" to test if the coverage belongs to a feather.
#define FEATHER_COVERAGE_THRESHOLD -1.5

#ifdef @VERTEX

VERTEX_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA32UI(PER_FLUSH_BINDINGS_SET,
                 TESS_VERTEX_TEXTURE_IDX,
                 @tessVertexTexture);
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x4(PATH_BUFFER_IDX, PathBuffer, @pathBuffer);
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
STORAGE_BUFFER_U32x4(CONTOUR_BUFFER_IDX, ContourBuffer, @contourBuffer);
VERTEX_STORAGE_BUFFER_BLOCK_END

#ifdef @DRAW_PATH
INLINE int2 tess_texel_coord(int texelIndex)
{
    return int2(texelIndex & ((1 << TESS_TEXTURE_WIDTH_LOG2) - 1),
                texelIndex >> TESS_TEXTURE_WIDTH_LOG2);
}

INLINE float manhattan_pixel_width(float2x2 M, float2 normalized)
{

    float2 v = MUL(M, normalized);
    return (abs(v.x) + abs(v.y)) * (1. / dot(v, v));
}

INLINE bool unpack_tessellated_path_vertex(float4 patchVertexData,
                                           float4 mirroredVertexData,
                                           int _instanceID,
                                           OUT(uint) outPathID,
                                           OUT(float2) outVertexPosition
#ifndef @RENDER_MODE_MSAA
                                           ,
                                           OUT(half2) outEdgeDistance
#else
                                           ,
                                           OUT(ushort) outPathZIndex
#endif
                                               VERTEX_CONTEXT_DECL)
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
    uint4 tessVertexData =
        TEXEL_FETCH(@tessVertexTexture, tess_texel_coord(tessVertexIdx));
    uint contourIDWithFlags = tessVertexData.w;

    // Fetch and unpack the contour referenced by the tessellation vertex.
    uint4 contourData =
        STORAGE_BUFFER_LOAD4(@contourBuffer,
                             contour_data_idx(contourIDWithFlags));
    float2 midpoint = uintBitsToFloat(contourData.xy);
    outPathID = contourData.z & 0xffffu;
    uint vertexIndex0 = contourData.w;

    // Fetch and unpack the path.
    float2x2 M = make_float2x2(
        uintBitsToFloat(STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u)));
    uint4 pathData = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 1u);
    float2 translate = uintBitsToFloat(pathData.xy);

    float strokeRadius = uintBitsToFloat(pathData.z);
    float featherRadius = uintBitsToFloat(pathData.w);

#ifdef @RENDER_MODE_MSAA
    // Unpack the rest of the path data.
    uint4 pathData2 = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 2u);
    outPathZIndex = cast_uint_to_ushort(pathData2.r);
#endif

    // Fix the tessellation vertex if we fetched the wrong one in order to
    // guarantee we got the correct contour ID and flags, or if we belong to a
    // mirrored contour and this vertex has an alternate position when mirrored.
    uint mirroredContourFlag =
        contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG;
    if (mirroredContourFlag != 0u)
    {
        localVertexID = int(mirroredVertexData.x);
        outset = mirroredVertexData.y;
        fillCoverage = mirroredVertexData.z;
    }
    if (localVertexID != vertexIDOnContour)
    {
        // This can peek one vertex before or after the contour, but the
        // tessellator guarantees there is always at least one padding vertex at
        // the beginning and end of the data.
        tessVertexIdx += localVertexID - vertexIDOnContour;
        uint4 replacementTessVertexData =
            TEXEL_FETCH(@tessVertexTexture, tess_texel_coord(tessVertexIdx));
        if ((replacementTessVertexData.w &
             (MIRRORED_CONTOUR_CONTOUR_FLAG | 0xffffu)) !=
            (contourIDWithFlags & (MIRRORED_CONTOUR_CONTOUR_FLAG | 0xffffu)))
        {
            // We crossed over into a new contour. Either wrap to the first
            // vertex in the contour or leave it clamped at the final vertex of
            // the contour.
            bool isClosed = strokeRadius == .0 || // filled
                            midpoint.x != .0;     // explicity closed stroke
            if (isClosed)
            {
                tessVertexData =
                    TEXEL_FETCH(@tessVertexTexture,
                                tess_texel_coord(int(vertexIndex0)));
            }
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

    // Finish unpacking tessVertexData.
    float theta = uintBitsToFloat(tessVertexData.z);
    float2 norm = float2(sin(theta), -cos(theta));
    float2 origin = uintBitsToFloat(tessVertexData.xy);
    float2 postTransformVertexOffset;

    if (featherRadius != .0)
    {
        // Never use a feather harder than 1.5 standard deviations across a
        // radius of 1/2px. This is the point where feathering just looks like
        // antialiasing, and any harder looks aliased.
        featherRadius =
            max(featherRadius,
                (FEATHER_TEXTURE_STDDEVS / 3.) / length(MUL(M, norm)));
    }

    if (strokeRadius != .0) // Is this a stroke?
    {
        // Ensure strokes always emit clockwise triangles.
        outset *= sign(determinant(M));

        // Joins only emanate from the outer side of the stroke.
        if ((contourIDWithFlags & LEFT_JOIN_CONTOUR_FLAG) != 0u)
            outset = min(outset, .0);
        if ((contourIDWithFlags & RIGHT_JOIN_CONTOUR_FLAG) != 0u)
            outset = max(outset, .0);

        float aaRadius = featherRadius != .0
                             ? featherRadius
                             : manhattan_pixel_width(M, norm) * AA_RADIUS;
        half globalCoverage = 1.;
        if (aaRadius > strokeRadius && featherRadius == .0)
        {
            // The stroke is narrower than the AA ramp. Instead of emitting
            // subpixel geometry, make the stroke as wide as the AA ramp and
            // apply a global coverage multiplier.
            globalCoverage =
                cast_float_to_half(strokeRadius) / cast_float_to_half(aaRadius);
            strokeRadius = aaRadius;
        }

        // Extend the vertex by half the width of the AA ramp.
        float2 vertexOffset =
            MUL(norm, strokeRadius + aaRadius); // Bloat stroke width for AA.

#ifndef @RENDER_MODE_MSAA
        // Calculate the AA distance to both the outset and inset edges of the
        // stroke. The fragment shader will use whichever is lesser.
        float x = outset * (strokeRadius + aaRadius);
        outEdgeDistance = cast_float2_to_half2(
            (1. / (aaRadius * 2.)) * (float2(x, -x) + strokeRadius) + .5);
#endif

        uint joinType = contourIDWithFlags & JOIN_TYPE_MASK;
        if (joinType > ROUND_JOIN_CONTOUR_FLAG)
        {
            // This vertex belongs to a miter or bevel join. Begin by finding
            // the bisector, which is the same as the miter line. The first two
            // vertices in the join peek forward to figure out the bisector, and
            // the final two peek backward.
            int peekDir = 2;
            if ((contourIDWithFlags & JOIN_TANGENT_0_CONTOUR_FLAG) == 0u)
                peekDir = -peekDir;
            if ((contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) != 0u)
                peekDir = -peekDir;
            int2 otherJoinTexelCoord =
                tess_texel_coord(tessVertexIdx + peekDir);
            uint4 otherJoinData =
                TEXEL_FETCH(@tessVertexTexture, otherJoinTexelCoord);
            float otherJoinTheta = uintBitsToFloat(otherJoinData.z);
            float joinAngle = abs(otherJoinTheta - theta);
            if (joinAngle > PI)
                joinAngle = 2. * PI - joinAngle;
            bool isTan0 =
                (contourIDWithFlags & JOIN_TANGENT_0_CONTOUR_FLAG) != 0u;
            bool isLeftJoin =
                (contourIDWithFlags & LEFT_JOIN_CONTOUR_FLAG) != 0u;
            float bisectTheta =
                joinAngle * (isTan0 == isLeftJoin ? -.5 : .5) + theta;
            float2 bisector = float2(sin(bisectTheta), -cos(bisectTheta));
            float bisectPixelWidth = manhattan_pixel_width(M, bisector);

            // Generalize everything to a "miter-clip", which is proposed in the
            // SVG-2 draft. Bevel joins are converted to miter-clip joins with a
            // miter limit of 1/2 pixel. They technically bleed out 1/2 pixel
            // when drawn this way, but they seem to look fine and there is not
            // an obvious solution to antialias them without an ink bleed.
            float miterRatio = cos(joinAngle * .5);
            float clipRadius;
            if ((joinType == MITER_CLIP_JOIN_CONTOUR_FLAG) ||
                (joinType == MITER_REVERT_JOIN_CONTOUR_FLAG &&
                 miterRatio >= .25))
            {
                // Miter! (Or square cap.)
                // We currently use hard coded miter limits:
                //   * 1 for square caps being emulated as miter-clip joins.
                //   * 4, which is the SVG default, for all other miter joins.
                float miterInverseLimit =
                    (contourIDWithFlags & EMULATED_STROKE_CAP_CONTOUR_FLAG) !=
                            0u
                        ? 1.
                        : .25;
                clipRadius =
                    strokeRadius * (1. / max(miterRatio, miterInverseLimit));
            }
            else
            {
                // Bevel! (Or butt cap.)
                clipRadius = strokeRadius * miterRatio +
                             /* 1/2px bleed! */ bisectPixelWidth * .5;
            }
            float clipAARadius = clipRadius + bisectPixelWidth * AA_RADIUS;
            if ((contourIDWithFlags & JOIN_TANGENT_INNER_CONTOUR_FLAG) != 0u)
            {
                // Reposition the inner join vertices at the miter-clip
                // positions. Leave the outer join vertices as duplicates on the
                // surrounding curve endpoints. We emit duplicate vertex
                // positions because we need a hard stop on the clip distance
                // (see below).
                //
                // Use aaRadius here because we're tracking AA on the mitered
                // edge, NOT the outer clip edge.
                float strokeAARaidus = strokeRadius + aaRadius;
                // clipAARadius must be 1/16 of an AA ramp (~1/16 pixel) longer
                // than the miter length before we start clipping, to ensure we
                // are solving for a numerically stable intersection.
                float slop = aaRadius * .125;
                if (strokeAARaidus <= clipAARadius * miterRatio + slop)
                {
                    // The miter point is before the clip line. Extend out to
                    // the miter point.
                    float miterAARadius = strokeAARaidus * (1. / miterRatio);
                    vertexOffset = bisector * miterAARadius;
                }
                else
                {
                    // The clip line is before the miter point. Find where the
                    // clip line and the mitered edge intersect.
                    float2 bisectAAOffset = bisector * clipAARadius;
                    float2 k = float2(dot(vertexOffset, vertexOffset),
                                      dot(bisectAAOffset, bisectAAOffset));
                    vertexOffset =
                        MUL(k, inverse(float2x2(vertexOffset, bisectAAOffset)));
                }
            }
            // The clip distance tells us how to antialias the outer clipped
            // edge. Since joins only emanate from the outset side of the
            // stroke, we can repurpose the inset distance as the clip distance.
            float2 pt = abs(outset) * vertexOffset;
            float clipDistance = (clipAARadius - dot(pt, bisector)) /
                                 (bisectPixelWidth * (AA_RADIUS * 2.));
#ifndef @RENDER_MODE_MSAA
            if ((contourIDWithFlags & LEFT_JOIN_CONTOUR_FLAG) != 0u)
                outEdgeDistance.y = cast_float_to_half(clipDistance);
            else
                outEdgeDistance.x = cast_float_to_half(clipDistance);
#endif
        }

#ifndef @RENDER_MODE_MSAA
        outEdgeDistance *= globalCoverage;

        // Bias outEdgeDistance.y slightly upwards in order to guarantee
        // outEdgeDistance.y is >= 0 at every pixel. "outEdgeDistance.y < 0" is
        // used to differentiate between strokes and fills.
        outEdgeDistance.y = max(outEdgeDistance.y, make_half(1e-4));

        if (featherRadius != .0)
        {
            // Bias x to tell the fragment shader that this is a feathered
            // stroke.
            outEdgeDistance.x = FEATHER_COVERAGE_BIAS - outEdgeDistance.x;
        }
#endif

        postTransformVertexOffset = MUL(M, outset * vertexOffset);

        // Throw away the fan triangles since we're a stroke.
        if (vertexType != STROKE_VERTEX)
            return false;
    }
    else // This is a fill.
    {
        if (bool(contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) !=
            bool(contourIDWithFlags & NEGATE_PATH_FILL_COVERAGE_FLAG))
        {
            fillCoverage = -fillCoverage;
        }

#ifndef @RENDER_MODE_MSAA
        // "outEdgeDistance.y < 0" indicates to the fragment shader that this is
        // a fill.
        outEdgeDistance = make_half2(fillCoverage, -1.);

        if (featherRadius != .0)
        {
            if (vertexType == STROKE_VERTEX)
            {
                // Bias y to tells the fragment shader that this is a feathered
                // fill.
                outEdgeDistance.y = FEATHER_COVERAGE_BIAS;
            }
            if ((contourIDWithFlags & JOIN_TYPE_MASK) ==
                FEATHER_JOIN_CONTOUR_FLAG)
            {
                // Feather joins need some dimming data, but there wasn't any
                // room left in the tessellation texture. Luckily, since feather
                // joins are all colocated on the same point, we were able to
                // slip in a sneaky backset to a different texel that has the
                // location, which freed up 32 bits for our dimming data.
                int backset = int(tessVertexData.x);
                if ((contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) != 0u)
                    backset = -backset;
                // Replace origin with the real one.
                origin = uintBitsToFloat(
                    TEXEL_FETCH(@tessVertexTexture,
                                tess_texel_coord(tessVertexIdx + backset))
                        .xy);
                if (vertexType == STROKE_VERTEX)
                {
                    // The dimming factor was placed where we typically would
                    // have found the origin.
                    half2 featherCorrection = unpackHalf2x16(tessVertexData.y);
                    // The dimming factor is "y^(x+1)" on the outer edge of the
                    // feather (y <= 1, x >= 0) and just "y^0" at the center.
                    //
                    // TODO: can we accomplish this by just making the local
                    // feather thinner instead?
                    half y = featherCorrection.y;
                    half x = fillCoverage == .0 ? featherCorrection.x : .0;
                    // If we change the base to 2, x becomes negative and this
                    // exponent can be expressed with one single scalar value:
                    //
                    //   y^(x+1) == 2^((x+1) * log2(y))
                    //
                    x = min((x + 1.) * log2(max(y, make_half(1e-5))), .0);
                    // Bias y to tells the fragment shader that this is a
                    // feather.
                    outEdgeDistance.y = x + FEATHER_COVERAGE_BIAS;
                }
                if ((contourIDWithFlags & ZERO_FEATHER_OUTSET_CONTOUR_FLAG) !=
                    0u)
                {
                    outset = .0;
                }
            }
            // Offset the vertex for feathering.
            postTransformVertexOffset = MUL(M, (outset * featherRadius) * norm);
        }
        else
        {
            // Offset the vertex for Manhattan AA.
            postTransformVertexOffset =
                sign(MUL(outset * norm, inverse(M))) * AA_RADIUS;
        }
#endif // !RENDER_MODE_MSAA

        // Place the fan point.
        if (vertexType == FAN_MIDPOINT_VERTEX)
            origin = midpoint;

        // If we're actually just drawing a triangle, throw away the entire
        // patch except a single fan triangle.
        if ((contourIDWithFlags & RETROFITTED_TRIANGLE_CONTOUR_FLAG) != 0u &&
            vertexType != FAN_VERTEX)
        {
            return false;
        }
    }

    outVertexPosition = MUL(M, origin) + postTransformVertexOffset + translate;
    return true;
}
#endif // @DRAW_PATH

#ifdef @DRAW_INTERIOR_TRIANGLES
INLINE float2
unpack_interior_triangle_vertex(float3 triangleVertex,
                                OUT(uint) outPathID,
                                OUT(half) outWindingWeight VERTEX_CONTEXT_DECL)
{
    outPathID = floatBitsToUint(triangleVertex.z) & 0xffffu;
    float2x2 M = make_float2x2(
        uintBitsToFloat(STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u)));
    uint4 pathData = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 1u);
    float2 translate = uintBitsToFloat(pathData.xy);
    outWindingWeight = cast_int_to_half(floatBitsToInt(triangleVertex.z) >> 16);
    return MUL(M, triangleVertex.xy) + translate;
}
#endif // @DRAW_INTERIOR_TRIANGLES

#endif // @VERTEX

#ifdef @FRAGMENT
INLINE bool is_stroke(half2 edgeDistance) { return edgeDistance.y >= .0; }

INLINE bool is_feathered_stroke(half2 edgeDistance)
{
    return edgeDistance.x < FEATHER_COVERAGE_THRESHOLD;
}

INLINE bool is_feathered_fill(half2 edgeDistance)
{
    return edgeDistance.y < FEATHER_COVERAGE_THRESHOLD;
}

INLINE half feathered_stroke_coverage(half2 edgeDistance,
                                      SAMPLED_R16F_REF(featherTextureRef,
                                                       featherSamplerRef))
{
    // Feathered stroke is:
    // 1 - feather(1 - leftCoverage) - feather(1 - rightCoverage)
    half coverage = 1.;

    // The portion OUTSIDE the coverage is "1 - coverage".
    // (edgeDistance.x is biased in order to classify this coverage as a
    // feather, so also remove the bias.)
    half leftOutsideCoverage = (1. - FEATHER_COVERAGE_BIAS) + edgeDistance.x;
    coverage -= TEXTURE_REF_SAMPLE_LOD(featherTextureRef,
                                       featherSamplerRef,
                                       float2(leftOutsideCoverage, .5),
                                       .0)
                    .r;

    half rightOutsideCoverage = 1. - edgeDistance.y;
    coverage -= TEXTURE_REF_SAMPLE_LOD(featherTextureRef,
                                       featherSamplerRef,
                                       float2(rightOutsideCoverage, .5),
                                       .0)
                    .r;

    return coverage;
}

INLINE half feathered_fill_coverage(half2 edgeDistance,
                                    SAMPLED_R16F_REF(featherTexture,
                                                     featherSampler))
{
    half fragCoverage = TEXTURE_REF_SAMPLE_LOD(featherTexture,
                                               featherSampler,
                                               float2(abs(edgeDistance.x), .5),
                                               .0)
                            .r *
                        sign(edgeDistance.x);
    // Apply nonlinear falloff to corners.
    // (edgeDistance.y is biased in order to classify this coverage as a
    // feather, so also remove the bias.)
    half x = min(edgeDistance.y - FEATHER_COVERAGE_BIAS, .0);
    fragCoverage *= exp2(x);
    return fragCoverage;
}
#endif // @FRAGMENT
