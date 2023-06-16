/*
 * Copyright 2022 Rive
 */

#define AA_RADIUS .5

#define STROKE_VERTEX 0
#define FAN_VERTEX 1
#define FAN_MIDPOINT_VERTEX 2

#define FIRST_VERTEX_OF_CONTOUR_FLAG (1u << 31)
#define JOIN_TYPE_MASK (3u << 29)
#define MITER_CLIP_JOIN (3u << 29)
#define MITER_REVERT_JOIN (2u << 29)
#define BEVEL_JOIN (1u << 29)
#define EMULATED_STROKE_CAP_FLAG (1u << 28)
#define JOIN_TANGENT_0_FLAG (1u << 27)
#define JOIN_TANGENT_INNER_FLAG (1u << 26)
#define LEFT_JOIN_FLAG (1u << 25)
#define RIGHT_JOIN_FLAG (1u << 24)
#define CONTOUR_ID_MASK 0xffffu

#define GRAD_TEXTURE_WIDTH 512.

#define TESS_TEXTURE_WIDTH_LOG2 11

#define EVEN_ODD_FLAG (1u << 31)
#define SOLID_COLOR_PAINT_TYPE 0u
#define LINEAR_GRADIENT_PAINT_TYPE 1u
#define RADIAL_GRADIENT_PAINT_TYPE 2u
#define CLIP_REPLACE_PAINT_TYPE 3u

// acos(1/4), because the miter limit is always 4.
#define MITER_ANGLE_LIMIT 1.318116071652817965746

// Raw bit representation of the largest denormalized fp16 value. We offset all (1-based) path IDs
// by this value in order to avoid denorms, which have been empirically unreliable on Android as ID
// values.
#define MAX_DENORM_F16 1023u

VARYING_BLOCK_BEGIN(Varyings)
NO_PERSPECTIVE VARYING half2 edgeDistance;
NO_PERSPECTIVE VARYING float4 varying_paint;
@OPTIONALLY_FLAT VARYING half pathID;
#ifdef @ENABLE_PATH_CLIPPING
@OPTIONALLY_FLAT VARYING half clipID;
#endif
#ifdef @ENABLE_ADVANCED_BLEND
@OPTIONALLY_FLAT VARYING half blendMode;
#endif
VARYING_BLOCK_END

#ifdef @VERTEX

UNIFORM_BLOCK_BEGIN(@Uniforms)
float2 viewportSize;
float gradTextureInverseHeight;
uint pathIDGranularity;
float vertexDiscardValue;
uint pad0;
uint pad1;
uint pad2;
UNIFORM_BLOCK_END(uniforms)

VERTEX_TEXTURE_BLOCK_BEGIN(VertexTextures)
TEXTURE_RGBA32UI(0) @tessVertexTexture;
TEXTURE_RGBA32UI(1) @pathTexture;
TEXTURE_RGBA32UI(2) @contourTexture;
VERTEX_TEXTURE_BLOCK_END

ATTR_BLOCK_BEGIN(Attrs)
ATTR(0) float4 wedgeVertexData; // [localVertexID, outset, fillCoverage, vertexType]
ATTR_BLOCK_END

int2 tessTexelCoord(int texelIndex)
{
    return int2(texelIndex & ((1 << TESS_TEXTURE_WIDTH_LOG2) - 1),
                texelIndex >> TESS_TEXTURE_WIDTH_LOG2);
}

float calc_aa_radius(float2x2 matrix, float2 normalized)
{

    float2 v = matrix * normalized;
    return (abs(v.x) + abs(v.y)) * (1. / dot(v, v)) * AA_RADIUS;
}

VERTEX_MAIN(
#ifdef METAL
    @drawVertexMain,
    Varyings,
    varyings,
    uint GLSL_VERTEX_ID [[vertex_id]],
    uint GLSL_INSTANCE_ID [[instance_id]],
    constant @Uniforms& uniforms [[buffer(0)]],
    constant Attrs* attrs [[buffer(1)]],
    VertexTextures textures
#endif
)
{
    // Unpack wedgeVertexData.
    ATTR_LOAD(float4, attrs, wedgeVertexData, GLSL_VERTEX_ID);
    int localVertexID = int(wedgeVertexData.x);
    float outset = wedgeVertexData.y;
    float fillCoverage = wedgeVertexData.z;
    int wedgeSize = floatBitsToInt(wedgeVertexData.w) >> 2;
    int vertexType = floatBitsToInt(wedgeVertexData.w) & 3;

    // Fetch the tessellation vertex we belong to.
    int vertexIdx = GLSL_INSTANCE_ID * wedgeSize + localVertexID;
    int2 tessVertexTexelCoord = tessTexelCoord(vertexIdx);
    uint4 tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, tessVertexTexelCoord, 0);
    uint contourIDWithFlags = tessVertexData.w;
    bool isClosingVertexOfContour =
        localVertexID == wedgeSize && (contourIDWithFlags & FIRST_VERTEX_OF_CONTOUR_FLAG) != 0u;
    if (isClosingVertexOfContour)
    {
        // The right vertex crossed over into a new contour. Fetch the previous vertex, which will
        // be the final vertex of the contour we're trying to draw.
        tessVertexTexelCoord = tessTexelCoord(vertexIdx - 1);
        tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, tessVertexTexelCoord, 0);
        contourIDWithFlags = tessVertexData.w;
    }

    // Fetch and unpack the contour referenced by the tessellation vertex.
    uint contourTexelIdx = (contourIDWithFlags & CONTOUR_ID_MASK) - 1u;
    int2 contourTexelCoord = int2(contourTexelIdx & 0xffu, contourTexelIdx >> 8);
    uint4 contourData = TEXEL_FETCH(textures, @contourTexture, contourTexelCoord, 0);
    float2 midpoint = uintBitsToFloat(contourData.xy);
    uint pathIDBits = contourData.z;
    uint vertexIndex0 = contourData.w;

    // Fetch and unpack the path referenced by the contour.
    uint pathIdx = pathIDBits - 1u;
    int2 pathTexelCoord = int2((pathIdx & 0x7fu) * 3u, pathIdx >> 7);
    float2x2 matrix =
        make_float2x2(uintBitsToFloat(TEXEL_FETCH(textures, @pathTexture, pathTexelCoord, 0)));
    uint4 pathData = TEXEL_FETCH(textures, @pathTexture, pathTexelCoord + int2(1, 0), 0);
    float2 translate = uintBitsToFloat(pathData.xy);
    float strokeRadius = uintBitsToFloat(pathData.z);
    uint pathParams = pathData.w;

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
            tessVertexData = TEXEL_FETCH(textures, @tessVertexTexture, vertexTexelCoord0, 0);
            contourIDWithFlags = tessVertexData.w;
        }
    }
    FLD(varyings, pathID) =
        unpackHalf2x16((pathIDBits + MAX_DENORM_F16) * uniforms.pathIDGranularity).r;
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

        float aaRadius = calc_aa_radius(matrix, norm);
        float globalCoverage = 1.;
        if (aaRadius > strokeRadius)
        {
            // The stroke is narrower than the AA ramp. Instead of emitting subpixel geometry,
            // make the stroke as wide as the AA ramp and apply a global coverage multiplier.
            globalCoverage = strokeRadius / aaRadius;
            strokeRadius = aaRadius;
        }

        // Extend the vertex by half the width of the AA ramp.
        float2 vertexOffset = norm * (strokeRadius + aaRadius); // Bloat stroke width for AA.

        // Calculate the AA distance to both the outset and inset edges of the stroke. The fragment
        // shader will use whichever is lesser.
        float x = outset * (strokeRadius + aaRadius);
        FLD(varyings, edgeDistance) =
            make_half2((1. / (aaRadius * 2.)) * (float2(x, -x) + strokeRadius) + .5);

        uint joinType = contourIDWithFlags & JOIN_TYPE_MASK;
        if (joinType != 0u)
        {
            // This vertex belongs to a miter or bevel join. Begin by finding the bisector, which is
            // the same as the miter line. The first two vertices in the join peek forward to figure
            // out the bisector, and the final two peek backward.
            int peekDir = (contourIDWithFlags & JOIN_TANGENT_0_FLAG) != 0u ? 2 : -2;
            int2 otherJoinTexelCoord = tessTexelCoord(vertexIdx + peekDir);
            uint4 otherJoinData = TEXEL_FETCH(textures, @tessVertexTexture, otherJoinTexelCoord, 0);
            float otherJoinTheta = uintBitsToFloat(otherJoinData.z);
            float joinAngle = abs(otherJoinTheta - theta);
            if (joinAngle > PI)
                joinAngle = 2. * PI - joinAngle;
            bool isTan0 = (contourIDWithFlags & JOIN_TANGENT_0_FLAG) != 0u;
            bool isLeftJoin = (contourIDWithFlags & LEFT_JOIN_FLAG) != 0u;
            float bisectTheta = joinAngle * (isTan0 == isLeftJoin ? -.5 : .5) + theta;
            float2 bisector = float2(sin(bisectTheta), -cos(bisectTheta));
            float bisectAARadius = calc_aa_radius(matrix, bisector);

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
                    vertexOffset = k * inverse(float2x2(vertexOffset, bisectAAOffset));
                }
            }
            // The clip distance tells us how to antialias the outer clipped edge. Since joins only
            // emanate from the outset side of the stroke, we can repurpose the inset distance as
            // the clip distance.
            float2 pt = abs(outset) * vertexOffset;
            float clipDistance = (clipAARadius - dot(pt, bisector)) / (bisectAARadius * 2.);
            if ((contourIDWithFlags & LEFT_JOIN_FLAG) != 0u)
                FLD(varyings, edgeDistance).y = clipDistance;
            else
                FLD(varyings, edgeDistance).x = clipDistance;
        }

        // Strokes identify themselves by emitting a negative edgeDistance.
        FLD(varyings, edgeDistance) *= -globalCoverage;

        // Throw away the fan triangles since we're a stroke.
        if (vertexType != STROKE_VERTEX)
            outset = uniforms.vertexDiscardValue;

        postTransformVertexOffset = matrix * (outset * vertexOffset);
    }
    else // This is a fill.
    {
        // Place the fan point.
        if (vertexType == FAN_MIDPOINT_VERTEX)
            origin = midpoint;

        // Indicate even-odd fill rule by making pathID negative.
        if ((pathParams & EVEN_ODD_FLAG) != 0u)
            FLD(varyings, pathID) = -FLD(varyings, pathID);

        // Offset the vertex for Manhattan AA.
        postTransformVertexOffset = sign(matrix * (outset * norm)) * AA_RADIUS;
        FLD(varyings, edgeDistance) = make_half2(fillCoverage, 1);
    }
    float2 vertexPosition = matrix * origin + postTransformVertexOffset + translate;

    uint paintType = (pathParams >> 20) & 7u;
#ifdef @ENABLE_PATH_CLIPPING
    uint clipIDBits = (pathParams >> 4) & 0xffffu;
    FLD(varyings, clipID) =
        clipIDBits == 0u
            ? .0
            : unpackHalf2x16((clipIDBits + MAX_DENORM_F16) * uniforms.pathIDGranularity).r;
    // Negative clipID means to repalce the clip with this clipID.
    if (paintType == CLIP_REPLACE_PAINT_TYPE)
        FLD(varyings, clipID) = -FLD(varyings, clipID);
#endif
#ifdef @ENABLE_ADVANCED_BLEND
    FLD(varyings, blendMode) = float(pathParams & 0xfu);
#endif

    // Unpack the paint once we have a position.
    uint4 paintData = TEXEL_FETCH(textures, @pathTexture, pathTexelCoord + int2(2, 0), 0);
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
        float2 localCoord = inverse(matrix) * (vertexPosition - translate);
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
    FLD(varyings, varying_paint) = paint;

    GLSL_POSITION.xy = vertexPosition * (float2(2, -2) / uniforms.viewportSize) + float2(-1, 1);
    GLSL_POSITION.zw = float2(0, 1);
    EMIT_VERTEX(varyings);
}
#endif

#ifdef @FRAGMENT

FRAG_TEXTURE_BLOCK_BEGIN(FragmentTextures)
TEXTURE_RGBA8(3) @gradTexture;
FRAG_TEXTURE_BLOCK_END

PLS_BLOCK_BEGIN
PLS_DECL4F(0) framebuffer;
PLS_DECL2F(1) coverageCountBuffer;
PLS_DECL4F(2) originalDstColorBuffer;
#ifdef @ENABLE_PATH_CLIPPING
PLS_DECL2F(3) clipBuffer;
#endif
PLS_BLOCK_END

PLS_MAIN(
#ifdef METAL
    @drawFragmentMain,
    Varyings varyings [[stage_in]],
    FragmentTextures textures,
    bool GLSL_FRONT_FACING [[front_facing]]
#endif
)
{
    float4 paint = FLD(varyings, varying_paint);
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
        color = TEXTURE_SAMPLE(textures, @gradTexture, float2(mix(x0, x1, t), row));
    }

    PLS_INTERLOCK_BEGIN;

    half2 coverageData = PLS_LOAD2F(coverageCountBuffer);
    half localPathID = coverageData.r;
    half coverageCount = coverageData.g;

    half4 dstColor;
    if (localPathID != FLD(varyings, pathID))
    {
        // This is the first fragment from pathID to touch this pixel.
        coverageCount = .0;
        dstColor = PLS_LOAD4F(framebuffer);
        PLS_STORE4F(originalDstColorBuffer, dstColor);
    }
    else
    {
        dstColor = PLS_LOAD4F(originalDstColorBuffer);
        PLS_PRESERVE_VALUE(originalDstColorBuffer);
    }

    // TODO: We may need to just send actual flags instead of using sign(edgeDistance) to identify
    // strokes. Since edgeDistance is interpolated, it can sometimes cross signs.
    half d = FLD(varyings, edgeDistance).x;
    if (d < -1e-4 /*stroke with an intentionally negative edgeDistance*/)
        coverageCount = min(max(d, FLD(varyings, edgeDistance).y), coverageCount);
    else if (GLSL_FRONT_FACING /*clockwise fill*/)
        coverageCount += d;
    else /*counterclockwise fill*/
        coverageCount -= d;

    // Save the updated coverage.
    PLS_STORE2F(coverageCountBuffer, FLD(varyings, pathID), coverageCount);

    // Convert coverageCount to coverage. (Which is min(-edgeDistance) right now for strokes.)
    half coverage = abs(coverageCount);
#ifdef @ENABLE_EVEN_ODD
    if (FLD(varyings, pathID) < .0 /*even-odd*/)
        coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
#endif
    coverage = min(coverage, make_half(1.)); // This also caps stroke coverage, which can be >1.

#ifdef @ENABLE_PATH_CLIPPING
    if (FLD(varyings, clipID) < .0 /*replace clip*/)
    {
        PLS_STORE2F(clipBuffer, -FLD(varyings, clipID), coverage);
        PLS_PRESERVE_VALUE(framebuffer);
    }
    else
#endif
    {
#ifdef @ENABLE_PATH_CLIPPING
        // Apply the clip.
        if (FLD(varyings, clipID) > .0)
        {
            half2 clipData = PLS_LOAD2F(clipBuffer);
            coverage = FLD(varyings, clipID) == clipData.r ? min(coverage, clipData.g) : .0;
        }
        PLS_PRESERVE_VALUE(clipBuffer);
#endif

        // Blend with the framebuffer color.
        color.a *= coverage;
#ifdef @ENABLE_ADVANCED_BLEND
        if (FLD(varyings, blendMode) != .0 /*srcOver*/)
        {
#ifdef @ENABLE_HSL_BLEND_MODES
            color = advanced_hsl_blend(
#else
            color = advanced_blend(
#endif
                color,
                unmultiply(dstColor),
                make_ushort(FLD(varyings, blendMode)));
        }
        else
#endif
        {
            color.rgb *= color.a;
            color = color + dstColor * (1. - color.a);
        }

        PLS_STORE4F(framebuffer, color);
    }

    PLS_INTERLOCK_END;

    EMIT_PLS;
}
#endif
