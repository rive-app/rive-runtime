/*
 * Copyright 2022 Rive
 */

#define AA_RADIUS .5

#define STROKE_VERTEX 0
#define FAN_VERTEX 1
#define FAN_MIDPOINT_VERTEX 2

#ifdef @VERTEX
ATTR_BLOCK_BEGIN(Attrs)
#ifdef @DRAW_INTERIOR_TRIANGLES
ATTR(0, packed_float3, @a_triangleVertex);
#else
ATTR(0, float4, @a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, @a_mirroredVertexData);
#endif
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(0, float4, v_paint);
#ifdef @DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(1, half, v_windingWeight);
#else
NO_PERSPECTIVE VARYING(2, half2, v_edgeDistance);
#endif
@OPTIONALLY_FLAT VARYING(3, half, v_pathID);
#ifdef @ENABLE_CLIPPING
@OPTIONALLY_FLAT VARYING(4, half, v_clipID);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(5, half, v_blendMode);
#endif
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
TEXTURE_RGBA32UI(TESS_VERTEX_TEXTURE_IDX, @tessVertexTexture);
TEXTURE_RGBA32UI(PATH_TEXTURE_IDX, @pathTexture);
TEXTURE_RGBA32UI(CONTOUR_TEXTURE_IDX, @contourTexture);
VERTEX_TEXTURE_BLOCK_END

int2 tessTexelCoord(int texelIndex)
{
    return int2(texelIndex & ((1 << TESS_TEXTURE_WIDTH_LOG2) - 1),
                texelIndex >> TESS_TEXTURE_WIDTH_LOG2);
}

float calc_aa_radius(float2x2 mat, float2 normalized)
{

    float2 v = MUL(mat, normalized);
    return (abs(v.x) + abs(v.y)) * (1. / dot(v, v)) * AA_RADIUS;
}

half id_bits_to_f16(uint idBits, uint pathIDGranularity)
{
    return idBits == 0u ? .0 : unpackHalf2x16((idBits + MAX_DENORM_F16) * pathIDGranularity).r;
}

#ifdef @ENABLE_BASE_INSTANCE_POLYFILL
// Define a uniform that will supply the base instance if we're on a platform that doesn't provide
// this as a built-in.
BASE_INSTANCE_POLYFILL_DECL(@baseInstancePolyfill);
#endif

VERTEX_MAIN(@drawVertexMain,
            @Uniforms,
            uniforms,
            Attrs,
            attrs,
            Varyings,
            varyings,
            VertexTextures,
            textures,
            _vertexID,
            _instanceID,
            _pos)
{
#ifdef @DRAW_INTERIOR_TRIANGLES
    ATTR_UNPACK(_vertexID, attrs, @a_triangleVertex, float3);
#else
    ATTR_UNPACK(_vertexID, attrs, @a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, @a_mirroredVertexData, float4);
#endif

    VARYING_INIT(varyings, v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(varyings, v_windingWeight, half);
#else
    VARYING_INIT(varyings, v_edgeDistance, half2);
#endif
    VARYING_INIT(varyings, v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_INIT(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_INIT(varyings, v_blendMode, half);
#endif

    bool shouldDiscardVertex = false;
#ifdef @DRAW_INTERIOR_TRIANGLES
    uint pathIDBits = floatBitsToUint(@a_triangleVertex.z) & 0xffffu;
#else
    // Unpack patchVertexData.
    int localVertexID = int(@a_patchVertexData.x);
    float outset = @a_patchVertexData.y;
    float fillCoverage = @a_patchVertexData.z;
    int patchSegmentSpan = floatBitsToInt(@a_patchVertexData.w) >> 2;
    int vertexType = floatBitsToInt(@a_patchVertexData.w) & 3;

    // Fetch a vertex that definitely belongs to the contour we're drawing.
    int vertexIDOnContour = min(localVertexID, patchSegmentSpan - 1);
    int tessVertexIdx = _instanceID * patchSegmentSpan + vertexIDOnContour;
#ifdef @ENABLE_BASE_INSTANCE_POLYFILL
    tessVertexIdx += @baseInstancePolyfill * patchSegmentSpan;
#endif
    uint4 tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, tessTexelCoord(tessVertexIdx));
    uint contourIDWithFlags = tessVertexData.w;

    // Fetch and unpack the contour referenced by the tessellation vertex.
    uint4 contourData =
        TEXEL_FETCH(textures, @contourTexture, contour_texel_coord(contourIDWithFlags));
    float2 midpoint = uintBitsToFloat(contourData.xy);
    uint pathIDBits = contourData.z;
    uint vertexIndex0 = contourData.w;
#endif

    // Fetch and unpack the path.
    int2 pathTexelCoord = path_texel_coord(pathIDBits);
    float2x2 mat =
        make_float2x2(uintBitsToFloat(TEXEL_FETCH(textures, @pathTexture, pathTexelCoord)));
    uint4 pathData = TEXEL_FETCH(textures, @pathTexture, pathTexelCoord + int2(1, 0));
    float2 translate = uintBitsToFloat(pathData.xy);
    uint pathParams = pathData.w;

#ifdef @DRAW_INTERIOR_TRIANGLES
    // The vertex position is encoded directly in vertex data when drawing triangles.
    float2 vertexPosition = MUL(mat, @a_triangleVertex.xy) + translate;
    // When we belong to a non-overlapping interior triangulation, the winding sign and weight are
    // also encoded directly in vertex data.
    v_windingWeight = float(floatBitsToInt(@a_triangleVertex.z) >> 16) * sign(determinant(mat));
#else
    float strokeRadius = uintBitsToFloat(pathData.z);

    // Fix the tessellation vertex if we fetched the wrong one in order to guarantee we got the
    // correct contour ID and flags, or if we belong to a mirrored contour and this vertex has an
    // alternate position when mirrored.
    uint mirroredContourFlag = contourIDWithFlags & MIRRORED_CONTOUR_FLAG;
    if (mirroredContourFlag != 0u)
    {
        localVertexID = int(@a_mirroredVertexData.x);
        outset = @a_mirroredVertexData.y;
        fillCoverage = @a_mirroredVertexData.z;
    }
    if (localVertexID != vertexIDOnContour)
    {
        // This can peek one vertex before or after the contour, but the tessellator guarantees
        // there is always at least one padding vertex at the beginning and end of the data.
        tessVertexIdx += localVertexID - vertexIDOnContour;
        uint4 replacementTessVertexData =
            TEXEL_FETCH(textures, @tessVertexTexture, tessTexelCoord(tessVertexIdx));
        if ((replacementTessVertexData.w & 0xffffu) != (contourIDWithFlags & 0xffffu))
        {
            // We crossed over into a new contour. Either wrap to the first vertex in the contour or
            // leave it clamped at the final vertex of the contour.
            bool isClosed = strokeRadius == .0 || // filled
                            midpoint.x != .0;     // explicity closed stroke
            if (isClosed)
            {
                tessVertexData =
                    TEXEL_FETCH(textures, @tessVertexTexture, tessTexelCoord(int(vertexIndex0)));
            }
        }
        else
        {
            tessVertexData = replacementTessVertexData;
        }
        // MIRRORED_CONTOUR_FLAG is not preserved at vertexIndex0. Preserve it here. By not
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
        outset *= sign(determinant(mat));

        // Joins only emanate from the outer side of the stroke.
        if ((contourIDWithFlags & LEFT_JOIN_FLAG) != 0u)
            outset = min(outset, .0);
        if ((contourIDWithFlags & RIGHT_JOIN_FLAG) != 0u)
            outset = max(outset, .0);

        float aaRadius = calc_aa_radius(mat, norm);
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
        v_edgeDistance = make_half2((1. / (aaRadius * 2.)) * (float2(x, -x) + strokeRadius) + .5);

        uint joinType = contourIDWithFlags & JOIN_TYPE_MASK;
        if (joinType != 0u)
        {
            // This vertex belongs to a miter or bevel join. Begin by finding the bisector, which is
            // the same as the miter line. The first two vertices in the join peek forward to figure
            // out the bisector, and the final two peek backward.
            int peekDir = 2;
            if ((contourIDWithFlags & JOIN_TANGENT_0_FLAG) == 0u)
                peekDir = -peekDir;
            if ((contourIDWithFlags & MIRRORED_CONTOUR_FLAG) != 0u)
                peekDir = -peekDir;
            int2 otherJoinTexelCoord = tessTexelCoord(tessVertexIdx + peekDir);
            uint4 otherJoinData = TEXEL_FETCH(textures, @tessVertexTexture, otherJoinTexelCoord);
            float otherJoinTheta = uintBitsToFloat(otherJoinData.z);
            float joinAngle = abs(otherJoinTheta - theta);
            if (joinAngle > PI)
                joinAngle = 2. * PI - joinAngle;
            bool isTan0 = (contourIDWithFlags & JOIN_TANGENT_0_FLAG) != 0u;
            bool isLeftJoin = (contourIDWithFlags & LEFT_JOIN_FLAG) != 0u;
            float bisectTheta = joinAngle * (isTan0 == isLeftJoin ? -.5 : .5) + theta;
            float2 bisector = float2(sin(bisectTheta), -cos(bisectTheta));
            float bisectAARadius = calc_aa_radius(mat, bisector);

            // Generalize everything to a "miter-clip", which is proposed in the SVG-2 draft. Bevel
            // joins are converted to miter-clip joins with a miter limit of 1/2 pixel. They
            // technically bleed out 1/2 pixel when drawn this way, but they seem to look fine and
            // there is not an obvious solution to antialias them without an ink bleed.
            float miterRatio = cos(joinAngle * .5);
            float clipRadius;
            if ((joinType == MITER_CLIP_JOIN) ||
                (joinType == MITER_REVERT_JOIN && miterRatio >= .25))
            {
                // Miter!
                // We currently use hard coded miter limits:
                //   * 1 for square caps being emulated as miter-clip joins.
                //   * 4, which is the SVG default, for all other miter joins.
                float miterInverseLimit =
                    (contourIDWithFlags & EMULATED_STROKE_CAP_FLAG) != 0u ? 1. : .25;
                clipRadius = strokeRadius * (1. / max(miterRatio, miterInverseLimit));
            }
            else
            {
                // Bevel!
                clipRadius = strokeRadius * miterRatio + /* 1/2px bleed! */ bisectAARadius;
            }
            float clipAARadius = clipRadius + bisectAARadius;
            if ((contourIDWithFlags & JOIN_TANGENT_INNER_FLAG) != 0u)
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
            if ((contourIDWithFlags & LEFT_JOIN_FLAG) != 0u)
                v_edgeDistance.y = make_half(clipDistance);
            else
                v_edgeDistance.x = make_half(clipDistance);
        }

        v_edgeDistance *= globalCoverage;

        // Bias v_edgeDistance.y slightly upwards in order to guarantee v_edgeDistance.y is >= 0 at
        // every pixel. "v_edgeDistance.y < 0" is used to differentiate between strokes and fills.
        v_edgeDistance.y = max(v_edgeDistance.y, make_half(1e-4));

        postTransformVertexOffset = MUL(mat, outset * vertexOffset);

        // Throw away the fan triangles since we're a stroke.
        if (vertexType != STROKE_VERTEX)
            shouldDiscardVertex = true;
    }
    else // This is a fill.
    {
        // Place the fan point.
        if (vertexType == FAN_MIDPOINT_VERTEX)
            origin = midpoint;

        // Offset the vertex for Manhattan AA.
        postTransformVertexOffset = sign(MUL(mat, outset * norm)) * AA_RADIUS;

        if ((contourIDWithFlags & MIRRORED_CONTOUR_FLAG) != 0u)
            fillCoverage = -fillCoverage;

        // "v_edgeDistance.y < 0" indicates to the fragment shader that this is a fill.
        v_edgeDistance = make_half2(fillCoverage, -1);

        // If we're actually just drawing a triangle, throw away the entire patch except a single
        // fan triangle.
        if ((contourIDWithFlags & RETROFITTED_TRIANGLE_FLAG) != 0u && vertexType != FAN_VERTEX)
            shouldDiscardVertex = true;
    }
    float2 vertexPosition = MUL(mat, origin) + postTransformVertexOffset + translate;
#endif

    // Encode the integral pathID as a "half" that we know the hardware will see as a unique value
    // in the fragment shader.
    v_pathID = id_bits_to_f16(pathIDBits, uniforms.pathIDGranularity);

    // Indicate even-odd fill rule by making pathID negative.
    if ((pathParams & EVEN_ODD_FLAG) != 0u)
        v_pathID = -v_pathID;

    uint paintType = (pathParams >> 20) & 7u;
#ifdef @ENABLE_CLIPPING
    uint clipIDBits = (pathParams >> 4) & 0xffffu;
    v_clipID = id_bits_to_f16(clipIDBits, uniforms.pathIDGranularity);
    // Negative clipID means to update the clip buffer instead of the framebuffer.
    if (paintType == CLIP_UPDATE_PAINT_TYPE)
        v_clipID = -v_clipID;
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    v_blendMode = float(pathParams & 0xfu);
#endif

    // Unpack the paint once we have a position.
    uint4 paintData = TEXEL_FETCH(textures, @pathTexture, pathTexelCoord + int2(2, 0));
    if (paintType == SOLID_COLOR_PAINT_TYPE)
    {
        float4 color = uintBitsToFloat(paintData);
        v_paint = color;
    }
#ifdef @ENABLE_CLIPPING
    else if (paintType == CLIP_UPDATE_PAINT_TYPE)
    {
        half outerClipID = id_bits_to_f16(paintData.r, uniforms.pathIDGranularity);
        v_paint = float4(outerClipID, 0, 0, 0);
    }
#endif
    else
    {
        float2 localCoord = MUL(inverse(mat), vertexPosition - translate);
        if (paintType == LINEAR_GRADIENT_PAINT_TYPE || paintType == RADIAL_GRADIENT_PAINT_TYPE)
        {
            uint span = paintData.x;
            float row = float(span >> 20);
            float x1 = float((span >> 10) & 0x3ffu);
            float x0 = float(span & 0x3ffu);
            // v_paint.a contains "-row" of the gradient ramp at texel center, in normalized space.
            v_paint.a = (row + .5) * -uniforms.gradTextureInverseHeight;
            // abs(v_paint.b) contains either:
            //   - 2 if the gradient ramp spans an entire row.
            //   - x0 of the gradient ramp in normalized space, if it's a simple 2-texel ramp.
            if (x1 > x0 + 1.)
                v_paint.b = 2.; // This ramp spans an entire row. Set it to 2 to convey this.
            else
                v_paint.b = x0 * (1. / GRAD_TEXTURE_WIDTH) + (.5 / GRAD_TEXTURE_WIDTH);
            float3 gradCoeffs = uintBitsToFloat(paintData.yzw);
            if (paintType == LINEAR_GRADIENT_PAINT_TYPE)
            {
                // The paint is a linear gradient.
                v_paint.g = .0;
                v_paint.r = dot(localCoord, gradCoeffs.xy) + gradCoeffs.z;
            }
            else
            {
                // The paint is a radial gradient. Mark v_paint.b negative to indicate this to the
                // fragment shader. (v_paint.b can't be zero because the gradient ramp is aligned on
                // pixel centers, so negating it will always produce a negative number.)
                v_paint.b = -v_paint.b;
                v_paint.rg = (localCoord - gradCoeffs.xy) / gradCoeffs.z;
            }
        }
        else // IMAGE_PAINT_TYPE
        {
            // v_paint.a <= -1. signals that the paint is an image.
            // v_paint.b is the image opacity.
            // v_paint.rg is the normalized image texture coordinate. (For now, we just
            // pre-transform paths before rendering, such that localCoord == textureCoord.)
            float opacity = uintBitsToFloat(paintData.x);
            v_paint = float4(localCoord.x, localCoord.y, opacity, -2.);
        }
    }

    _pos.xy = vertexPosition * float2(uniforms.renderTargetInverseViewportX,
                                      -uniforms.renderTargetInverseViewportY) +
              float2(-1, 1);
    _pos.zw = float2(0, 1);
    if (shouldDiscardVertex)
    {
        _pos = float4(uniforms.vertexDiscardValue,
                      uniforms.vertexDiscardValue,
                      uniforms.vertexDiscardValue,
                      uniforms.vertexDiscardValue);
    }

    VARYING_PACK(varyings, v_paint);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_PACK(varyings, v_windingWeight);
#else
    VARYING_PACK(varyings, v_edgeDistance);
#endif
    VARYING_PACK(varyings, v_pathID);
#ifdef @ENABLE_CLIPPING
    VARYING_PACK(varyings, v_clipID);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_PACK(varyings, v_blendMode);
#endif
    EMIT_VERTEX(varyings, _pos);
}
#endif

#ifdef @FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN(FragmentTextures)
TEXTURE_RGBA8(GRAD_TEXTURE_IDX, @gradTexture);
TEXTURE_RGBA8(IMAGE_TEXTURE_IDX, @imageTexture);
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)

PLS_BLOCK_BEGIN
PLS_DECL4F(0, framebuffer);
PLS_DECLUI(1, coverageCountBuffer);
PLS_DECL4F(2, originalDstColorBuffer);
PLS_DECLUI(3, clipBuffer);
PLS_BLOCK_END

PLS_MAIN(@drawFragmentMain, Varyings, varyings, FragmentTextures, textures, _pos)
{
    VARYING_UNPACK(varyings, v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(varyings, v_windingWeight, half);
#else
    VARYING_UNPACK(varyings, v_edgeDistance, half2);
#endif
    VARYING_UNPACK(varyings, v_pathID, half);
#ifdef @ENABLE_CLIPPING
    VARYING_UNPACK(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(varyings, v_blendMode, half);
#endif

#ifndef @DRAW_INTERIOR_TRIANGLES
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_BEGIN;
#endif

#if 0
    // Since path IDs are drawn in monotonically increasing order, we can reset coverageCount back
    // to zero on a new path by using max().
    // TODO: This is slower on Apple silicon. Does it perform better on other platforms?
    // uint coverageData = PLS_LOADUI(coverageCountBuffer);
    // uint coverageZero = packHalf2x16(make_half2(0, v_pathID));
    // half coverageCount = unpackHalf2x16(max(coverageData, coverageZero)).r;
#endif

    half2 coverageData = unpackHalf2x16(PLS_LOADUI(coverageCountBuffer));
    half coverageBufferID = coverageData.g;
    half coverageCount = coverageBufferID == v_pathID ? coverageData.r : make_half(0);

#ifdef @DRAW_INTERIOR_TRIANGLES
    coverageCount += v_windingWeight;
#else
    if (v_edgeDistance.y >= .0) // Stroke.
        coverageCount = max(min(v_edgeDistance.x, v_edgeDistance.y), coverageCount);
    else // Fill. (Back-face culling ensures v_edgeDistance.x is appropriately signed.)
        coverageCount += v_edgeDistance.x;

    // Save the updated coverage.
    PLS_STOREUI(coverageCountBuffer, packHalf2x16(make_half2(coverageCount, v_pathID)));
#endif

    // Convert coverageCount to coverage.
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if (v_pathID < .0 /*even-odd*/)
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
#endif
    coverage = min(coverage, make_half(1.)); // This also caps stroke coverage, which can be >1.

#ifdef @ENABLE_CLIPPING
    if (v_clipID < .0) // Update the clip buffer.
    {
        half clipID = -v_clipID;
#ifdef @ENABLE_NESTED_CLIPPING
        half outerClipID = v_paint.r;
        if (outerClipID != .0)
        {
            // This is a nested clip. Intersect coverage with the enclosing clip (outerClipID).
            half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
            half clipContentID = clipData.g;
            half outerClipCoverage = .0;
            if (clipContentID == outerClipID)
            {
                // First hit: clipBuffer contains outerClipCoverage.
                outerClipCoverage = clipData.r;
#ifndef @DRAW_INTERIOR_TRIANGLES
                // Stash outerClipCoverage before overwriting clipBuffer, in case we hit this pixel
                // again and need it. (Not necessary when drawing interior triangles because they
                // always go last and don't overlap.)
                PLS_STORE4F(originalDstColorBuffer, make_half4(outerClipCoverage, 0, 0, 0));
#endif
            }
            else if (clipContentID == clipID)
            {
                // Subsequent hit: outerClipCoverage is stashed in originalDstColorBuffer.
                outerClipCoverage = PLS_LOAD4F(originalDstColorBuffer).r;
#ifndef @DRAW_INTERIOR_TRIANGLES
                // Since interior triangles are always last, there's no need to preserve this value.
                PLS_PRESERVE_VALUE(originalDstColorBuffer);
#endif
            }
            coverage = min(coverage, outerClipCoverage);
        }
#endif // @ENABLE_NESTED_CLIPPING
        PLS_STOREUI(clipBuffer, packHalf2x16(make_half2(coverage, clipID)));
        PLS_PRESERVE_VALUE(framebuffer);
    }
    else // Render to the main framebuffer.
#endif   // @ENABLE_CLIPPING
    {
#ifdef @ENABLE_CLIPPING
        // Apply the clip.
        if (v_clipID != .0)
        {
            // Clip IDs are not necessarily drawn in monotonically increasing order, so always check
            // exact equality of the clipID.
            half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
            half clipContentID = clipData.g;
            half clipCoverage = clipContentID == v_clipID ? clipData.r : make_half(0);
            coverage = min(coverage, clipCoverage);
        }
#endif
        PLS_PRESERVE_VALUE(clipBuffer);

        half4 color;
        if (v_paint.a >= .0) // Is the paint a solid color?
        {
            color = make_half4(v_paint);
        }
        else if (v_paint.a > -1.) // Is paint is a gradient (linear or radial)?
        {
            float t = v_paint.b > .0 ? /*linear*/ v_paint.r : /*radial*/ length(v_paint.rg);
            t = clamp(t, .0, 1.);
            float span = abs(v_paint.b);
            float x = span > 1. ? /*entire row*/ (1. - 1. / GRAD_TEXTURE_WIDTH) * t +
                                      (.5 / GRAD_TEXTURE_WIDTH)
                                : /*two texels*/ (1. / GRAD_TEXTURE_WIDTH) * t + span;
            float row = -v_paint.a;
            color = make_half4(TEXTURE_SAMPLE(textures, @gradTexture, gradSampler, float2(x, row)));
        }
        else // The paint is an image.
        {
            color = TEXTURE_SAMPLE(textures, @imageTexture, imageSampler, v_paint.rg);
            color.a *= v_paint.b; // paint.b holds the opacity of the image.
        }
        color.a *= coverage;

        half4 dstColor;
        if (coverageBufferID != v_pathID)
        {
            // This is the first fragment from pathID to touch this pixel.
            dstColor = PLS_LOAD4F(framebuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // We don't need to store coverage when drawing interior triangles because they always
            // go last and don't overlap, so every fragment is the final one in the path.
            PLS_STORE4F(originalDstColorBuffer, dstColor);
#endif
        }
        else
        {
            dstColor = PLS_LOAD4F(originalDstColorBuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
            // Since interior triangles are always last, there's no need to preserve this value.
            PLS_PRESERVE_VALUE(originalDstColorBuffer);
#endif
        }

        // Blend with the framebuffer color.
#ifdef @ENABLE_ADVANCED_BLEND
        if (v_blendMode != .0 /*srcOver*/)
        {
#ifdef @ENABLE_HSL_BLEND_MODES
            color = advanced_hsl_blend(
#else
            color = advanced_blend(
#endif
                color,
                unmultiply(dstColor),
                make_ushort(v_blendMode));
        }
        else
#endif
        {
            color.rgb *= color.a;
            color = color + dstColor * (1. - color.a);
        }

        PLS_STORE4F(framebuffer, color);
    }

#ifndef @DRAW_INTERIOR_TRIANGLES
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_END;
#endif

    EMIT_PLS;
}
#endif
