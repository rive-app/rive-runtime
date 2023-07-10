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
#endif
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING(float4, v_paint);
#ifdef @DRAW_INTERIOR_TRIANGLES
@OPTIONALLY_FLAT VARYING(half, v_windingWeight);
#else
NO_PERSPECTIVE VARYING(half2, v_edgeDistance);
#endif
@OPTIONALLY_FLAT VARYING(half, v_pathID);
#ifdef @ENABLE_PATH_CLIPPING
@OPTIONALLY_FLAT VARYING(half, v_clipID);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING(half, v_blendMode);
#endif
VARYING_BLOCK_END(_pos)

#ifdef @VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
TEXTURE_RGBA32UI(0, @tessVertexTexture);
TEXTURE_RGBA32UI(1, @pathTexture);
TEXTURE_RGBA32UI(2, @contourTexture);
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
#endif

    VARYING_INIT(varyings, v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(varyings, v_windingWeight, half);
#else
    VARYING_INIT(varyings, v_edgeDistance, half2);
#endif
    VARYING_INIT(varyings, v_pathID, half);
#ifdef @ENABLE_PATH_CLIPPING
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

    // Fetch the tessellation vertex we belong to.
    int tessVertexIdx = _instanceID * patchSegmentSpan + localVertexID;
#ifdef @ENABLE_BASE_INSTANCE_POLYFILL
    tessVertexIdx += @baseInstancePolyfill * patchSegmentSpan;
#endif
    int2 tessVertexTexelCoord = tessTexelCoord(tessVertexIdx);
    uint4 tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, tessVertexTexelCoord);
    uint contourIDWithFlags = tessVertexData.w;
    bool isClosingVertexOfContour = localVertexID == patchSegmentSpan &&
                                    (contourIDWithFlags & FIRST_VERTEX_OF_CONTOUR_FLAG) != 0u;
    if (isClosingVertexOfContour)
    {
        // The right vertex crossed over into a new contour. Fetch the previous vertex, which will
        // be the final vertex of the contour we're trying to draw.
        tessVertexTexelCoord = tessTexelCoord(tessVertexIdx - 1);
        tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, tessVertexTexelCoord);
        contourIDWithFlags = tessVertexData.w;
    }

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

    // Finish unpacking tessVertexData.
    if (isClosingVertexOfContour)
    {
        bool isClosed = strokeRadius == .0 || // filled
                        midpoint.x != .0;     // explicity closed stroke
        if (isClosed)
        {
            // The contour is closed and we are the closing vertex. Wrap back around full circle and
            // re-emit the first tessellation vertex.
            int2 vertexTexelCoord0 = tessTexelCoord(int(vertexIndex0));
            tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, vertexTexelCoord0);
            contourIDWithFlags = tessVertexData.w;
        }
    }
    float theta = uintBitsToFloat(tessVertexData.z);
    float2 norm = float2(sin(theta), -cos(theta));
    float2 origin = uintBitsToFloat(tessVertexData.xy);
    float2 postTransformVertexOffset;

    if (strokeRadius != .0) // Is this a stroke?
    {
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
            int peekDir = (contourIDWithFlags & JOIN_TANGENT_0_FLAG) != 0u ? 2 : -2;
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

        // Strokes identify themselves by emitting a negative edgeDistance.
        v_edgeDistance *= -globalCoverage;

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
        v_edgeDistance = make_half2(fillCoverage, 1);

        // If we're actually just drawing a triangle, throw away the entire patch except a single
        // fan triangle.
        if ((contourIDWithFlags & RETROFITTED_TRIANGLE_FLAG) != 0u && vertexType != FAN_VERTEX)
            shouldDiscardVertex = true;
    }
    float2 vertexPosition = MUL(mat, origin) + postTransformVertexOffset + translate;
#endif

    // Encode the integral pathID as a "half" that we know the hardware will see as a unique value
    // in the fragment shader.
    v_pathID = unpackHalf2x16((pathIDBits + MAX_DENORM_F16) * uniforms.pathIDGranularity).r;

    // Indicate even-odd fill rule by making pathID negative.
    if ((pathParams & EVEN_ODD_FLAG) != 0u)
        v_pathID = -v_pathID;

    uint paintType = (pathParams >> 20) & 7u;
#ifdef @ENABLE_PATH_CLIPPING
    uint clipIDBits = (pathParams >> 4) & 0xffffu;
    v_clipID = clipIDBits == 0u
                   ? .0
                   : unpackHalf2x16((clipIDBits + MAX_DENORM_F16) * uniforms.pathIDGranularity).r;
    // Negative clipID means to repalce the clip with this clipID.
    if (paintType == CLIP_REPLACE_PAINT_TYPE)
        v_clipID = -v_clipID;
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    v_blendMode = float(pathParams & 0xfu);
#endif

    // Unpack the paint once we have a position.
    uint4 paintData = TEXEL_FETCH(textures, @pathTexture, pathTexelCoord + int2(2, 0));
    float4 paint;
    if (paintType != LINEAR_GRADIENT_PAINT_TYPE && paintType != RADIAL_GRADIENT_PAINT_TYPE)
    {
        // The paint is a solid color or clip.
        paint = uintBitsToFloat(paintData);
    }
    else
    {
        // The paint is a gradient.
        uint span = paintData.x;
        float row = float(span >> 20);
        float x1 = float((span >> 10) & 0x3ffu);
        float x0 = float(span & 0x3ffu);
        // paint.a contains "-row" of the gradient ramp at texel center, in normalized space.
        paint.a = (row + .5) * -uniforms.gradTextureInverseHeight;
        // paint.b contains x0 of the gradient ramp, and whether the ramp is two texels or an entire
        // row. Specifically:
        //   - fract(paint.b) = normalized coordinate of x0, at texel center
        //   - paint.b < 1 => two-texel ramp
        //   - paint.b > 1 => the ramp spans an entire row
        paint.b = x0 * (1. / GRAD_TEXTURE_WIDTH) + (.5 / GRAD_TEXTURE_WIDTH);
        if (x1 > x0 + 1.)
            ++paint.b; // x1 must equal GRAD_TEXTURE_WIDTH - 1. Increment paint.b to convey this.
        float2 localCoord = MUL(inverse(mat), vertexPosition - translate);
        float3 gradCoeffs = uintBitsToFloat(paintData.yzw);
        if (paintType == LINEAR_GRADIENT_PAINT_TYPE)
        {
            // The paint is a linear gradient.
            paint.g = .0;
            paint.r = dot(localCoord, gradCoeffs.xy) + gradCoeffs.z;
        }
        else
        {
            // The paint is a radial gradient. Mark paint.b negative to indicate this to the
            // fragment shader. (paint.b can't be zero because the gradient ramp is aligned on pixel
            // centers, so negating it will always produce a negative number.)
            paint.b = -paint.b;
            paint.rg = (localCoord - gradCoeffs.xy) / gradCoeffs.z;
        }
    }
    v_paint = paint;

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
#ifdef @ENABLE_PATH_CLIPPING
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
TEXTURE_RGBA8(3, @gradTexture);
FRAG_TEXTURE_BLOCK_END

GRADIENT_SAMPLER_DECL(0, gradSampler)

PLS_BLOCK_BEGIN
PLS_DECL4F(0, framebuffer);
PLS_DECL2F(1, coverageCountBuffer);
PLS_DECL4F(2, originalDstColorBuffer);
PLS_DECL2F(3, clipBuffer);
PLS_BLOCK_END

PLS_MAIN(@drawFragmentMain, Varyings, varyings, FragmentTextures, textures, _pos, _clockwise)
{
    VARYING_UNPACK(varyings, v_paint, float4);
#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_UNPACK(varyings, v_windingWeight, half);
#else
    VARYING_UNPACK(varyings, v_edgeDistance, half2);
#endif
    VARYING_UNPACK(varyings, v_pathID, half);
#ifdef @ENABLE_PATH_CLIPPING
    VARYING_UNPACK(varyings, v_clipID, half);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    VARYING_UNPACK(varyings, v_blendMode, half);
#endif

    float4 paint = v_paint;
    half4 color;
    if (paint.a >= .0)
    {
        // The paint is a solid color or clip.
        color = make_half4(paint);
    }
    else
    {
        // The paint is a gradient (linear or radial).
        float t = paint.b > .0 ? paint.r /* linear */ : length(paint.rg) /* radial */;
        t = clamp(t, .0, 1.);
        float span = abs(paint.b);
        float x0 = fract(span);
        float x1 =
            span > 1.
                ? ((GRAD_TEXTURE_WIDTH - .5) / GRAD_TEXTURE_WIDTH) // The ramp spans an entire row.
                : x0 + (1. / GRAD_TEXTURE_WIDTH);                  // The ramp spans 2 texels.
        float row = -paint.a;
        float2 gradCoord = float2(mix(x0, x1, t), row);
        color = make_half4(TEXTURE_SAMPLE(textures, @gradTexture, gradSampler, gradCoord));
    }

#ifndef @DRAW_INTERIOR_TRIANGLES
    // Interior triangles don't overlap, so don't need raster ordering.
    PLS_INTERLOCK_BEGIN;
#endif

    half2 coverageData = PLS_LOAD2F(coverageCountBuffer);
    half localPathID = coverageData.r;
    half coverageCount = coverageData.g;

    half4 dstColor;
    if (localPathID != v_pathID)
    {
        // This is the first fragment from pathID to touch this pixel.
        coverageCount = .0;
        dstColor = PLS_LOAD4F(framebuffer);
#ifndef @DRAW_INTERIOR_TRIANGLES
        // We don't need to store coverage when drawing interior triangles because they always go
        // last and don't overlap, so every fragment is the final one in the path.
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

#ifdef @DRAW_INTERIOR_TRIANGLES
    coverageCount += v_windingWeight;
#else
    // TODO: We may need to just send actual flags instead of using sign(edgeDistance) to identify
    // strokes. Since edgeDistance is interpolated, it can sometimes cross signs.
    half d = v_edgeDistance.x;
    if (d < -1e-4 /*stroke with an intentionally negative edgeDistance*/)
        coverageCount = min(max(d, v_edgeDistance.y), coverageCount);
    else if (_clockwise /*clockwise fill*/)
        coverageCount += d;
    else /*counterclockwise fill*/
        coverageCount -= d;

    // Save the updated coverage.
    PLS_STORE2F(coverageCountBuffer, v_pathID, coverageCount);
#endif

    // Convert coverageCount to coverage. (Which is min(-edgeDistance) right now for strokes.)
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if (v_pathID < .0 /*even-odd*/)
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
#endif
    coverage = min(coverage, make_half(1.)); // This also caps stroke coverage, which can be >1.

#ifdef @ENABLE_PATH_CLIPPING
    if (v_clipID < .0 /*replace clip*/)
    {
        PLS_STORE2F(clipBuffer, -v_clipID, coverage);
        PLS_PRESERVE_VALUE(framebuffer);
    }
    else
#endif
    {
#ifdef @ENABLE_PATH_CLIPPING
        // Apply the clip.
        if (v_clipID > .0)
        {
            half2 clipData = PLS_LOAD2F(clipBuffer);
            coverage = v_clipID == clipData.r ? min(coverage, clipData.g) : .0;
        }
#endif
        PLS_PRESERVE_VALUE(clipBuffer);

        // Blend with the framebuffer color.
        color.a *= coverage;
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
