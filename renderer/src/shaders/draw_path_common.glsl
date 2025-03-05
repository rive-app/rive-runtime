/*
 * Copyright 2023 Rive
 */

// Common functions shared by draw shaders.

// Feathered coverage values get shifted by "FEATHER_COVERAGE_BIAS" in order
// to classify the coverage as belonging to a feather.
#define FEATHER_COVERAGE_BIAS -2.

// Fragment shaders test if a coverage value is less than
// "FEATHER_COVERAGE_THRESHOLD" to see if the coverage belongs to a feather.
#define FEATHER_COVERAGE_THRESHOLD -1.5

// Since the sign of x dictates the sign of coverage, and since x may be 0 when
// feathering, bias x slightly upward so we don't lose the sign when it's 0.
#define FEATHER_X_COORD_BIAS .25

// Magnitude of cotan(theta) at which we decide an angle is flat when processing
// feathers.
#define HORIZONTAL_COTANGENT_THRESHOLD 1e3

// Value to assign cotTheta to ensure it gets treated as flat.
#define HORIZONTAL_COTANGENT_VALUE                                             \
    (HORIZONTAL_COTANGENT_THRESHOLD * HORIZONTAL_COTANGENT_THRESHOLD)

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA32UI(PER_FLUSH_BINDINGS_SET,
                 TESS_VERTEX_TEXTURE_IDX,
                 @tessVertexTexture);
#ifdef @ENABLE_FEATHER
TEXTURE_R16F_1D_ARRAY(PER_FLUSH_BINDINGS_SET,
                      FEATHER_TEXTURE_IDX,
                      @featherTexture);
#endif
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x4(PATH_BUFFER_IDX, PathBuffer, @pathBuffer);
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, @paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, @paintAuxBuffer);
STORAGE_BUFFER_U32x4(CONTOUR_BUFFER_IDX, ContourBuffer, @contourBuffer);
VERTEX_STORAGE_BUFFER_BLOCK_END
#endif // @VERTEX

#if defined(@ENABLE_FEATHER) || defined(@ATLAS_BLIT)
SAMPLER_LINEAR(FEATHER_TEXTURE_IDX, featherSampler)
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, GRAD_TEXTURE_IDX, @gradTexture);
#if defined(@ENABLE_FEATHER) || defined(@ATLAS_BLIT)
TEXTURE_R16F_1D_ARRAY(PER_FLUSH_BINDINGS_SET,
                      FEATHER_TEXTURE_IDX,
                      @featherTexture);
#endif
#ifdef @ATLAS_BLIT
TEXTURE_R16F(PER_DRAW_BINDINGS_SET, ATLAS_TEXTURE_IDX, @atlasTexture);
#endif
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, @imageTexture);
#if defined(@RENDER_MODE_MSAA) && defined(@ENABLE_ADVANCED_BLEND)
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, DST_COLOR_TEXTURE_IDX, @dstColorTexture);
#endif
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
// Metal defines @VERTEX and @FRAGMENT at the same time, so yield to the vertex
// definition of featherSampler in this case.
#ifdef @ATLAS_BLIT
SAMPLER_LINEAR(ATLAS_TEXTURE_IDX, atlasSampler)
#endif
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)
#endif // @FRAGMENT

// We distinguish between strokes and fills by the sign of coverages.y,
// regardless of whether feathering is enabled (coverages are float4), or
// disabled (coverages are half2),
#ifdef @FRAGMENT
INLINE bool is_stroke(float4 coverages) { return coverages.y >= .0; }
INLINE bool is_stroke(half2 coverages) { return coverages.y >= .0; }
#endif // FRAGMENT

#if defined(@FRAGMENT) && defined(@ENABLE_FEATHER)
// We can also classify a fragments as feathered/not-feathered strokes/fills by
// looking at coverages.
INLINE bool is_feathered_stroke(float4 coverages)
{
    return coverages.x < FEATHER_COVERAGE_THRESHOLD;
}

INLINE bool is_feathered_fill(float4 coverages)
{
    return coverages.y < FEATHER_COVERAGE_THRESHOLD;
}
#endif // @FRAGMENT && @ENABLE_FEATHER

#ifdef @VERTEX
// Packs all the info to evaluate a feathered fill into 4 varying floats.
float4 pack_feathered_fill_coverages(float cornerTheta,
                                     float2 spokeNorm,
                                     float outset)
{
    // Find the corner's local coordinate within the feather convolution, where
    // the convolution matrix is centered on [.5, .5] and spans 0..1, and the
    // first edge of the corner runs horizontal and to the left.
    float2 cornerLocalCoord = (1. - spokeNorm * abs(outset)) * .5;

    // Calculate cotTheta and y0 for the fragment shader.
    // (See eval_feathered_fill() for details.)
    float cotTheta, y0;
    if (abs(cornerTheta - PI_OVER_2) < 1. / HORIZONTAL_COTANGENT_THRESHOLD)
    {
        cotTheta = .0;
        y0 = .0;
    }
    else
    {
        float tanTheta = tan(cornerTheta);
        cotTheta = sign(PI_OVER_2 - cornerTheta) /
                   max(abs(tanTheta), 1. / HORIZONTAL_COTANGENT_VALUE);
        y0 = cotTheta >= .0
                 ? cornerLocalCoord.y - (1. - cornerLocalCoord.x) * tanTheta
                 : cornerLocalCoord.y + cornerLocalCoord.x * tanTheta;
    }

    // Bias x & y for additional information:
    //
    //  * x will be negated later on if the triangle is back-facing. This tells
    //    the fragment shader what sign to give feathered coverage. Ensure it's
    //    greater than zero.
    //
    //  * y < FEATHER_COVERAGE_BIAS tells the fragment shader this is a feather.
    //
    float4 coverages;
    coverages.x = max(cornerLocalCoord.x, .0) + FEATHER_X_COORD_BIAS;
    coverages.y = -cornerLocalCoord.y + FEATHER_COVERAGE_BIAS;
    coverages.z = cotTheta;
    coverages.w = y0;
    return coverages;
}
#endif // @VERTEX

#ifdef @ENABLE_FEATHER
INLINE half eval_feathered_fill(float4 coverages TEXTURE_CONTEXT_DECL)
{
    // x and y are the relative coordinates of the corner vertex within the
    // feather convolution. They are oriented above center=[.5, .5], with the
    // first edge running horizontal and to the left.
    //
    // The second edge exits x,y at an angle of theta. "y0" is the location
    // where it intersects with either the left or right edge of the
    // convolution (left if cotTheta < 0, right if cotTheta > 0).
    //
    // y0 and cotTheta are both 0 when the corner angle is pi/2.
    half cotTheta = coverages.z;
    half y0 = max(coverages.w, .0); // Clamp y0 at the top of the convolution.

    // First compute the upper area of the convolution that is fully contained
    // within both edges. (i.e., the area above y0.)
    //
    // NOTE: If we aren't a corner, this will be the entire feather.
    half featherCoverage = cotTheta >= .0 ? FEATHER(y0) : .0;

    // If we're a (not flat) corner, the bottom area of the convolution needs to
    // account for both edges.
    //
    // Integrate the area contained within both edges, taking advantage of the
    // separability property of our convolution:
    //           _
    //    t=y   / \.
    //          |
    //          |  FEATHER(t * m + b) * FEATHER'(t) * dt
    //          |
    //   t=y0 \_/
    //
    // NOTE: The derivative FEATHER'(t) is the normal distribution with:
    //
    //   mu = 1/2
    //   sigma = 1 / (2 * FEATHER_TEXTURE_STDDEVS)
    //
    // We can evaluate this directly without a lookup table.
    //
    // For performance constraints, we only take 4 samples on the integral.
    if (abs(cotTheta) < HORIZONTAL_COTANGENT_THRESHOLD)
    {
        // Unpack x & y from the varying coverages.
        half x = abs(coverages.x) - FEATHER_X_COORD_BIAS;
        half y = -coverages.y + FEATHER_COVERAGE_BIAS;

        // Find the height of each sample, and pre-scale by 1/(sigma*sqrt(2pi))
        // from the normal distribution (to save on multiplies later).
        //
        //   dt = (y - y0) / 4 / (sigma * sqrt(2pi))
        //
        half dt = (y - y0) * 0.5984134206;

        // Subdivide into 4 bars of even height, and sample at the centers.
        // (Reuse dt so we don't have to recompute "y - y1" again.)
        //
        //   t = lerp(y0, y1, [1/8, 3/8, 5/8, 7/8])
        //
        half4 t = y0 + dt * make_half4(0.20888568955,
                                       0.62665706865,
                                       1.04442844776,
                                       1.46219982687);

        // Feather horizontally where each sample intersects the second edge.
        half4 u = t * -cotTheta + (y * cotTheta + x);
        half4 feathers = make_half4(FEATHER(u[0]),
                                    FEATHER(u[1]),
                                    FEATHER(u[2]),
                                    FEATHER(u[3]));

        // Evaluate the normal distribution at each vertical sample.
        // (Scale t_ by sqrt(log2(e)) to change the base of the function from
        // e^x to 2^x.)
        //
        //   t_ = 1/sqrt(2) * (x - mu) / sigma * sqrt(log2(e))
        //   normalDistro = 2^(-t_ * t_)
        //
        half4 t_ = t * 5.09593080173 + -2.54796540086;
        half4 ddtFeather = exp2(-t_ * t_);

        // Take the sum of "FEATHER(u) * FEATHER'(t) * dt" at all 4 samples.
        featherCoverage += dot(feathers, ddtFeather) * dt;
    }

    // Clockwise triangles add to the featherCoverage, counterclockwise
    // triangles subtract from it.
    return featherCoverage * sign(coverages.x);
}

INLINE half eval_feathered_stroke(float4 coverages TEXTURE_CONTEXT_DECL)
{
    // Feathered stroke is:
    // 1 - feather(1 - leftCoverage) - feather(1 - rightCoverage)
    float featherCoverage = 1.;

    // The portion OUTSIDE the featherCoverage is "1 - featherCoverage".
    // (coverages.x is biased in order to classify this featherCoverage as a
    // feather, so also remove the bias.)
    float leftOutsideCoverage = (1. - FEATHER_COVERAGE_BIAS) + coverages.x;
    featherCoverage -= FEATHER(leftOutsideCoverage);

    float rightOutsideCoverage = 1. - coverages.y;
    featherCoverage -= FEATHER(rightOutsideCoverage);

    return featherCoverage;
}
#endif // @ENABLE_FEATHER

#if defined(@FRAGMENT) && defined(@ATLAS_BLIT)
// Upscales a pre-rendered feather from the atlas, converting from gaussian
// space to linear before doing a bilerp.
INLINE half
filter_feather_atlas(float2 atlasCoord,
                     float2 atlasTextureInverseSize TEXTURE_CONTEXT_DECL)
{
    // Gather the quad of pixels we need to filter.
    // Gather from the exact center of the quad to make sure there are no
    // rounding differences between us and the texture unit.
    float2 atlasQuadCenter = round(atlasCoord);
    half4 coverages = TEXTURE_GATHER(@atlasTexture,
                                     atlasSampler,
                                     atlasQuadCenter,
                                     atlasTextureInverseSize);
    // Convert each pixel from gaussian space back to linear.
    coverages = make_half4(INVERSE_FEATHER(coverages.x),
                           INVERSE_FEATHER(coverages.y),
                           INVERSE_FEATHER(coverages.z),
                           INVERSE_FEATHER(coverages.w));
    // Bilerp in linear space.
    coverages.xw = mix(coverages.xw,
                       coverages.yz,
                       make_half(atlasCoord.x + .5 - atlasQuadCenter.x));
    coverages.x = mix(coverages.w,
                      coverages.x,
                      make_half(atlasCoord.y + .5 - atlasQuadCenter.y));
    // Go back to gaussian now that the bilerp is finished.
    return FEATHER(coverages.x);
}
#endif // @FRAGMENT && @ATLAS_BLIT

#if defined(@VERTEX) && defined(@DRAW_PATH)
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
                                           OUT(float4) outCoverages
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
        int replacementTessVertexIdx =
            tessVertexIdx + localVertexID - vertexIDOnContour;
        uint4 replacementTessVertexData =
            TEXEL_FETCH(@tessVertexTexture,
                        tess_texel_coord(replacementTessVertexIdx));
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
                tessVertexIdx = int(vertexIndex0);
                tessVertexData = TEXEL_FETCH(@tessVertexTexture,
                                             tess_texel_coord(tessVertexIdx));
            }
        }
        else
        {
            tessVertexIdx = replacementTessVertexIdx;
            tessVertexData = replacementTessVertexData;
        }
        // MIRRORED_CONTOUR_CONTOUR_FLAG is not preserved at vertexIndex0.
        // Preserve it here. By not preserving this flag, the normal and
        // mirrored contour can both share the same contour record.
        contourIDWithFlags =
            (tessVertexData.w & ~MIRRORED_CONTOUR_CONTOUR_FLAG) |
            mirroredContourFlag;
    }

    // Find the tangent angle of the curve at our vertex.
    float theta;
#ifdef @ENABLE_FEATHER
    float featherJoinEdge0Theta;
    float featherJoinCornerTheta;
    if ((contourIDWithFlags & JOIN_TYPE_MASK) == FEATHER_JOIN_CONTOUR_FLAG &&
        vertexType == STROKE_VERTEX)
    {
        // Feather joins work out their stepping here in the vertex shader.
        // Instead of emitting just the tangent angle, the tessellation shader
        // gave us the original tessellation parameters.
        float joinVertexID = float(tessVertexData.z & 0xffffu);
        float joinSegmentCount = float(tessVertexData.z >> 16);

        // Find the tessellation vertices immediately before and after the
        // feather join in order to work out the corner angles.
        int2 edgeVertexOffsets =
            int2(-joinVertexID - 1., joinSegmentCount - joinVertexID + 1.);
        if ((contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) != 0u)
            edgeVertexOffsets = -edgeVertexOffsets;
        uint4 tessDataBeforeJoin =
            TEXEL_FETCH(@tessVertexTexture,
                        tess_texel_coord(tessVertexIdx + edgeVertexOffsets.x));
        uint4 tesDataAfterJoin =
            TEXEL_FETCH(@tessVertexTexture,
                        tess_texel_coord(tessVertexIdx + edgeVertexOffsets.y));
        if ((tesDataAfterJoin.w & (MIRRORED_CONTOUR_CONTOUR_FLAG | 0xffffu)) !=
            (tessDataBeforeJoin.w & (MIRRORED_CONTOUR_CONTOUR_FLAG | 0xffffu)))
        {
            // We reached over into a new contour. The edge immediately after
            // this feather join is actually the first vertex in the countour.
            tesDataAfterJoin = TEXEL_FETCH(@tessVertexTexture,
                                           tess_texel_coord(int(vertexIndex0)));
        }

        featherJoinEdge0Theta = uintBitsToFloat(tessDataBeforeJoin.z);
        float featherJoinEdge1Theta = uintBitsToFloat(tesDataAfterJoin.z);
        featherJoinCornerTheta = featherJoinEdge1Theta - featherJoinEdge0Theta;
        if (abs(featherJoinCornerTheta) > PI)
            featherJoinCornerTheta -= _2PI * sign(featherJoinCornerTheta);

        // Feather joins draw backwards segments across the angle outside the
        // join, in order to erase some of the coverage that got written. Divide
        // the forward and backward segments proportionally to their respective
        // angles.
        float nonHelperSegmentCount =
            joinSegmentCount + 1. - float(FEATHER_JOIN_HELPER_VERTEX_COUNT);
        float forwardSegmentCount = clamp(
            round(abs(featherJoinCornerTheta) / PI * nonHelperSegmentCount),
            1.,
            nonHelperSegmentCount - 1.);
        float backwardSegmentCount =
            nonHelperSegmentCount - forwardSegmentCount;
        if (joinVertexID <= backwardSegmentCount)
        {
            // We're a backwards segment of the feather join.
            featherJoinCornerTheta =
                -(PI * sign(featherJoinCornerTheta) - featherJoinCornerTheta);
            joinSegmentCount = backwardSegmentCount;
            // On the final backward vertex, negate outset (later we will use
            // theta=featherJoinEdge1Theta instead of
            // featherJoinEdge1Theta - PI). This creates a crack-free
            // tessellation with the edge we're joining.
            if (joinVertexID == backwardSegmentCount)
                outset = -outset;
        }
        else if (joinVertexID == backwardSegmentCount + 1.)
        {
            // There's a discontinuous jump between the backward and forward
            // segments. This is a throwaway vertex to disconnect them.
            joinVertexID = .0;
            joinSegmentCount = .0;
            outset = .0;
        }
        else
        {
            // We're a forward segment of the feather join.
            joinVertexID -= backwardSegmentCount + 2.;
            joinSegmentCount = forwardSegmentCount;
        }

        if (joinVertexID == joinSegmentCount)
        {
            // Emit "featherJoinEdge1Theta" precisely (instead of the
            // approximate lerp below) to create crack-free tessellation with
            // the edges we're joining.
            theta = featherJoinEdge1Theta;
        }
        else
        {
            theta = featherJoinEdge0Theta +
                    featherJoinCornerTheta * (joinVertexID / joinSegmentCount);
        }
    }
    else
#endif // @ENABLE_FEATHER
    {
        theta = uintBitsToFloat(tessVertexData.z);
    }
    float2 norm = float2(sin(theta), -cos(theta));
    float2 origin = uintBitsToFloat(tessVertexData.xy);
    float2 postTransformVertexOffset = float2(0, 0);

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
            norm * (strokeRadius + aaRadius); // Bloat stroke width for AA.

#ifndef @RENDER_MODE_MSAA
        // Calculate the AA distance to both the outset and inset edges of the
        // stroke. The fragment shader will use whichever is lesser.
        float x = outset * (strokeRadius + aaRadius);
        outCoverages.xy =
            (1. / (aaRadius * 2.)) * (float2(x, -x) + strokeRadius) + .5;
        outCoverages.zw = make_float2(.0);
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
                joinAngle = _2PI - joinAngle;
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
                outCoverages.y = clipDistance;
            else
                outCoverages.x = clipDistance;
#endif
        }

#ifndef @RENDER_MODE_MSAA
        outCoverages.xy *= globalCoverage;

        // Bias outCoverages.y slightly upwards in order to guarantee
        // outCoverages.y is >= 0 at every pixel. "outCoverages.y < 0" is
        // used to differentiate between strokes and fills.
        outCoverages.y = max(outCoverages.y, 1e-4);

        if (featherRadius != .0)
        {
            // Bias x to tell the fragment shader that this is a feathered
            // stroke.
            outCoverages.x = FEATHER_COVERAGE_BIAS - outCoverages.x;
        }
#endif

        postTransformVertexOffset = MUL(M, outset * vertexOffset);

        // Throw away the fan triangles since we're a stroke.
        if (vertexType != STROKE_VERTEX)
            return false;
    }
    else // This is a fill.
    {
#ifndef @RENDER_MODE_MSAA
        // "outCoverages.y < 0" indicates to the fragment shader that this is
        // a fill, as opposed to a stroke.
        outCoverages = float4(fillCoverage, -1., .0, .0);

#ifdef @ENABLE_FEATHER
        if (featherRadius != .0)
        {
            // Bias y to tell the fragment shader that this is a feathered edge.
            outCoverages.y = FEATHER_COVERAGE_BIAS;

            // "outCoverages.z = HORIZONTAL_COTANGENT_VALUE" initializes us
            // in a default state of feathering a flat edge (as opposed to a
            // corner).
            outCoverages.z = HORIZONTAL_COTANGENT_VALUE;

            // eval_feathered_fill() just feathers outCoverages.w=y0 when
            // we're a flat edge, so initialize it with fillCoverage.
            outCoverages.w = fillCoverage;

            if ((contourIDWithFlags & JOIN_TYPE_MASK) ==
                    FEATHER_JOIN_CONTOUR_FLAG &&
                vertexType == STROKE_VERTEX)
            {
                // Feathered corners are symmetric; swap the first and second
                // edge if needed so the corner angle is always positive.
                if (featherJoinCornerTheta < .0)
                {
                    featherJoinEdge0Theta += featherJoinCornerTheta;
                    featherJoinCornerTheta = -featherJoinCornerTheta;
                }

                // Find the angle and local outset direction of our specific
                // spoke in the feather join, relative to the first edge. Take
                // advantage of the fact that feathered corners are symmetric
                // again, and limit spokeTheta to the first half of the join
                // angle.
                float spokeTheta = theta - featherJoinEdge0Theta;
                spokeTheta = mod(spokeTheta + PI_OVER_2, _2PI) - PI_OVER_2;
                spokeTheta = clamp(spokeTheta, .0, featherJoinCornerTheta);
                if (spokeTheta > featherJoinCornerTheta * .5)
                {
                    spokeTheta = featherJoinCornerTheta - spokeTheta;
                }
                float2 spokeNorm = float2(sin(spokeTheta), cos(spokeTheta));

                // TODO: This contraction logic generates cracks in geometry. It
                // needs more investigation.
#if 0
                // When coners have stong curvature, their feather diminishes
                // faster than it does for flat edges. In this scenario we can
                // contract the tessellation a little to save on performance
                // without losing visual fidelity.
                //
                // This code attempts to be somewhat methodical, but it's just
                // hackery. The idea is to measure actual feather coverage at an
                // outset of N standard deviations, compare that to what
                // coverage would have been for a flat edge, and contract
                // accordingly. By observation, a logarithmic function of
                // featherJoinCornerTheta gives values for N with a good balance
                // of perf and quality.
                float N =
                    1. + .33 * log2(PI_OVER_2 /
                            (PI - min(featherJoinCornerTheta, PI - PI / 16.)));
                float4 coveragesAtNStddevOutset =
                    pack_feathered_fill_coverages(featherJoinCornerTheta,
                            spokeNorm,
                            .5 * (N / 3.));
                float featherAtNStddevOutset = eval_feathered_fill(
                        coveragesAtNStddevOutset TEXTURE_CONTEXT_FORWARD);
                float inverseFeather =
                    INVERSE_FEATHER(featherAtNStddevOutset);
                float stddevsAwayFromCenter =
                    (.5 - inverseFeather) * (FEATHER_TEXTURE_STDDEVS * 2.);
                float contraction = N / max(stddevsAwayFromCenter, N);
                outset *= contraction;
#endif

                // Emit coverage values for the fragment shader.
                outCoverages =
                    pack_feathered_fill_coverages(featherJoinCornerTheta,
                                                  spokeNorm,
                                                  outset);
            }
            // Offset the vertex for feathering.
            postTransformVertexOffset = MUL(M, (outset * featherRadius) * norm);
        }
        else
#endif // @ENABLE_FEATHER
        {
            // Offset the vertex for Manhattan AA.
            postTransformVertexOffset =
                sign(MUL(outset * norm, inverse(M))) * AA_RADIUS;
        }

        if (bool(contourIDWithFlags & MIRRORED_CONTOUR_CONTOUR_FLAG) !=
            bool(contourIDWithFlags & NEGATE_PATH_FILL_COVERAGE_FLAG))
        {
            outCoverages.x = -outCoverages.x;
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

#ifdef @RENDER_MODE_MSAA
    uint4 pathData2 = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 2u);
    outPathZIndex = cast_uint_to_ushort(pathData2.r);
#else
    // Force coverage to solid when wireframe is enabled so we can see the
    // triangles.
    outCoverages.xy = mix(outCoverages.xy,
                          float2(1., -1.),
                          make_bool2(uniforms.wireframeEnabled != 0u));
#endif

    return true;
}
#endif // @VERTEX && @DRAW_PATH

#if defined(@VERTEX) && defined(@DRAW_INTERIOR_TRIANGLES)
INLINE float2 unpack_interior_triangle_vertex(float3 triangleVertex,
                                              OUT(uint) outPathID
#ifdef @RENDER_MODE_MSAA
                                              ,
                                              OUT(ushort) outPathZIndex
#else
                                              ,
                                              OUT(half) outWindingWeight
#endif
                                                  VERTEX_CONTEXT_DECL)
{
    outPathID = floatBitsToUint(triangleVertex.z) & 0xffffu;
#ifdef @RENDER_MODE_MSAA
    uint4 pathData2 = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 2u);
    outPathZIndex = cast_uint_to_ushort(pathData2.x);
#else
    outWindingWeight = cast_int_to_half(floatBitsToInt(triangleVertex.z) >> 16);
#endif
    float2 vertexPos = triangleVertex.xy;
    // ATLAS_BLIT draws vertices in screen space.
    float2x2 M = make_float2x2(
        uintBitsToFloat(STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u)));
    uint4 pathData = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 1u);
    float2 translate = uintBitsToFloat(pathData.xy);
    vertexPos = MUL(M, vertexPos) + translate;
    return vertexPos;
}
#endif // @VERTEX && @DRAW_INTERIOR_TRIANGLES

#if defined(@VERTEX) && defined(@ATLAS_BLIT)
INLINE float2
unpack_atlas_coverage_vertex(float3 triangleVertex,
                             OUT(uint) outPathID,
#ifdef @RENDER_MODE_MSAA
                             OUT(ushort) outPathZIndex,
#endif
                             OUT(float2) outAtlasCoord VERTEX_CONTEXT_DECL)
{
    outPathID = floatBitsToUint(triangleVertex.z) & 0xffffu;
    uint4 pathData2 = STORAGE_BUFFER_LOAD4(@pathBuffer, outPathID * 4u + 2u);
#ifdef @RENDER_MODE_MSAA
    outPathZIndex = cast_uint_to_ushort(pathData2.x);
#endif
    float2 vertexPos = triangleVertex.xy;
    // outAtlasCoord tells the fragment shader where to fetch coverage from the
    // atlas, when using atlas coverage.
    float3 atlasTransform = uintBitsToFloat(pathData2.yzw);
    outAtlasCoord = vertexPos * atlasTransform.x + atlasTransform.yz;
    return vertexPos;
}
#endif // @VERTEX && @ATLAS_BLIT
