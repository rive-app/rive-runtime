// We need to adjust the scaling of the packed `u` value in the gradient data by
// this amount, to leave enough bits for the gradient type.
#define GRADIENT_PACKING_U_SCALE 8.0

#ifdef @VERTEX

// We pack the following into the result:
//  xy = gradient coordinate
//  z  = gradient type, grad texture u and v, and "isComplex", packed:
//     - v is stored in the integral part (it's range [0, 2047])
//     - type is scaled by to be in [0..1), sitting at the top of the fractional
//       part.
//     - u is scaled to be in [0..0.125), sitting just below type. The
//       half-texel bias has not yet been applied.
//     - The value is negated for complex gradients.
//     Note that all of these components fit nicely within the mantissa of a
//     32-bit float.
//  w  = additiveness complement and coverage, packed, always negative.
//     - additiveness complement is in the whole part.
//     - coverage is halved and stored in the fractional part, then biased to
//       avoid a situation where the interpolated value can go slightly negative
//       from 0 (due to plane equation shenanigans)
INLINE float4 packGradientData(float2 fragCoord,
                               float2x2 mat,
                               float2 translate,
                               float2 texCoord,
                               uint type,
                               float additivenessComplement,
                               float coverage)
{
    float4 gradient;

    // xy are just the transformed coordinate, direct.
    gradient.xy = MUL(mat, fragCoord) + translate;

    // texCoord x is negative for complex gradients, and should be packed as 0
    // in that case.
    bool isComplex = (texCoord.x < 0.0);
    texCoord.x = max(texCoord.x, 0.0);

    // Pack the 3 values that go into gradient.z.
    gradient.z =
        texCoord.y + (float(type) / GRADIENT_PACKING_U_SCALE) +
        (texCoord.x * (GRAD_TEXTURE_INVERSE_WIDTH / GRADIENT_PACKING_U_SCALE));

    // Negate gradient.z to signify a complex gradient.
    gradient.z = (isComplex) ? -gradient.z : gradient.z;

    // Pack half (and biased) coverage with (whole) additiveness, all negative.
    gradient.w =
        (coverage * -0.5 - 0.25) - round(additivenessComplement * 255.0);
    return gradient;
}
#endif

#ifdef @FRAGMENT

INLINE float2 getGradientUVFromOriginUV(float type,
                                        float2 gradientCoord,
                                        float2 originUV,
                                        bool isComplex)
{
    // Calculate the gradient t value based on the type.
    float t = (type == float(LINEAR_GRADIENT_PAINT_TYPE))
                  ? /*linear*/ gradientCoord.x
                  : /*radial*/ length(gradientCoord.xy);
    t = clamp(t, 0.0, 1.0);

    // Complex gradients span from the first to last texel center, simple
    // gradients span a single texel.
    float span = isComplex ? (1.0 - GRAD_TEXTURE_INVERSE_WIDTH)
                           : GRAD_TEXTURE_INVERSE_WIDTH;
    float x = t * span + originUV.x;
    return float2(x, originUV.y);
}

// This is used by atomic_draw in the image rect case to get the proper uv
// coordinates for gradient lookup.
INLINE float2 getGradientUVFromOriginTexel(float type,
                                           float2 gradientCoord,
                                           float2 originTexel,
                                           bool isComplex,
                                           float yScale,
                                           float yBias)
{
    // The origin U coordinate needs to be biased by a half-texel
    const float OriginUBias = 0.5 * GRAD_TEXTURE_INVERSE_WIDTH;
    float2 originUV =
        float2(originTexel.x * GRAD_TEXTURE_INVERSE_WIDTH + OriginUBias,
               originTexel.y * yScale + yBias);
    return getGradientUVFromOriginUV(type, gradientCoord, originUV, isComplex);
}

INLINE float2 getGradientUV(float4 gradient, float yScale, float yBias)
{
    // The origin U coordinate needs to be biased by a half-texel
    const float OriginUBias = 0.5 * GRAD_TEXTURE_INVERSE_WIDTH;

    bool isComplex = gradient.z < 0.0;
    gradient.z = abs(gradient.z);
    float2 originUV;

    // v lives in the whole part of the number. We know this value is positive
    // so we can use floor instead of trunc, as floor is considerably faster on
    // some hardware.
    originUV.y = floor(gradient.z);

    // u (and type) lives in the fractional part. Scale the fractional part up
    // to get type into the integral component and u in the fractional
    // component. Also, in the same operation, bias u so that it's correctly
    // pointing at a texel center (note that this will not affect the whole
    // part, type, in any way)
    // (doing this as a subtraction instead of a fract because subtraction is
    // faster)
    originUV.x =
        (gradient.z - originUV.y) * GRADIENT_PACKING_U_SCALE + OriginUBias;

    // Scale and bias the v component to get it into proper uv space
    originUV.y = originUV.y * yScale + yBias;

    // The whole part of this, now, is the type. Note that we don't need to
    // remove this whole part from the u coordinate (and thus can save a
    // subtraction), because the gradient texture is set to wrap and so the
    // whole part modulos away.
    float type = floor(originUV.x);

    return getGradientUVFromOriginUV(type, gradient.xy, originUV, isComplex);
}

INLINE half getGradientCoverage(float4 gradient)
{
    // coverage was halved and the w value was negated, hence the -2.0 scale.
    // Note that fract gives a *positive* value, so our packed -0.25 .. -0.75
    // comes out of fract as 0.75..0.25, this is the correct operation to scale
    // it back to [0, 1].
    return fract(gradient.w) * -2.0 + 1.5;
}

INLINE half getGradientAdditivenessComplement(float4 gradient)
{
    const float Scale = -1.0 / 255.0;

    // gradient.w is always negative, so using `ceil` here as the stand-in for
    // `trunc`, since trunc is considerably slower on some hardware.
    return ceil(gradient.w) * Scale;
}
#endif