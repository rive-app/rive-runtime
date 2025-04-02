/*
 * Copyright 2022 Rive
 */

// From the KHR_blend_equation_advanced spec:
//
//    The advanced blend equations are those listed in tables X.1 and X.2.  When
//    using one of these equations, blending is performed according to the
//    following equations:
//
//      R = f(Rs',Rd')*p0(As,Ad) + Y*Rs'*p1(As,Ad) + Z*Rd'*p2(As,Ad)
//      G = f(Gs',Gd')*p0(As,Ad) + Y*Gs'*p1(As,Ad) + Z*Gd'*p2(As,Ad)
//      B = f(Bs',Bd')*p0(As,Ad) + Y*Bs'*p1(As,Ad) + Z*Bd'*p2(As,Ad)
//      A =          X*p0(As,Ad) +     Y*p1(As,Ad) +     Z*p2(As,Ad)
//
//    where the function f and terms X, Y, and Z are specified in the table.
//    The R, G, and B components of the source color used for blending are
//    considered to have been premultiplied by the A component prior to
//    blending.  The base source color (Rs',Gs',Bs') is obtained by dividing
//    through by the A component:
//
//      (Rs', Gs', Bs') =
//        (0, 0, 0),              if As == 0
//        (Rs/As, Gs/As, Bs/As),  otherwise
//
//    The destination color components are always considered to have been
//    premultiplied by the destination A component and the base destination
//    color (Rd', Gd', Bd') is obtained by dividing through by the A component:
//
//      (Rd', Gd', Bd') =
//        (0, 0, 0),               if Ad == 0
//        (Rd/Ad, Gd/Ad, Bd/Ad),   otherwise
//
//    When blending using advanced blend equations, we expect that the R, G, and
//    B components of premultiplied source and destination color inputs be
//    stored as the product of non-premultiplied R, G, and B components and the
//    A component of the color.  If any R, G, or B component of a premultiplied
//    input color is non-zero and the A component is zero, the color is
//    considered ill-formed, and the corresponding component of the blend result
//    will be undefined.
//
//    The weighting functions p0, p1, and p2 are defined as follows:
//
//      p0(As,Ad) = As*Ad
//      p1(As,Ad) = As*(1 - Ad)
//      p2(As,Ad) = Ad*(1 - As)
//
//    In these functions, the A components of the source and destination colors
//    are taken to indicate the portion of the pixel covered by the fragment
//    (source) and the fragments previously accumulated in the pixel
//    (destination).  The functions p0, p1, and p2 approximate the relative
//    portion of the pixel covered by the intersection of the source and
//    destination, covered only by the source, and covered only by the
//    destination, respectively.  The equations defined here assume that there
//    is no correlation between the source and destination coverage.
//

#ifdef @FRAGMENT

#ifdef @ENABLE_KHR_BLEND
layout(
#ifdef @ENABLE_HSL_BLEND_MODES
    blend_support_all_equations
#else
    blend_support_multiply,
    blend_support_screen,
    blend_support_overlay,
    blend_support_darken,
    blend_support_lighten,
    blend_support_colordodge,
    blend_support_colorburn,
    blend_support_hardlight,
    blend_support_softlight,
    blend_support_difference,
    blend_support_exclusion
#endif
    ) out;
#endif // ENABLE_KHR_BLEND

#ifdef @ENABLE_ADVANCED_BLEND
#ifdef @ENABLE_HSL_BLEND_MODES
// When using one of the HSL blend equations in table X.2 as the blend equation,
// the blend coefficients are effectively obtained by converting both the
// non-premultiplied source and destination colors to the HSL (hue, saturation,
// luminosity) color space, generating a new HSL color by selecting H, S, and L
// components from the source or destination according to the blend equation,
// and then converting the result back to RGB. The HSL blend equations are only
// well defined when the values of the input color components are in the range
// [0..1].
half minv3(half3 c) { return min(min(c.r, c.g), c.b); }
half maxv3(half3 c) { return max(max(c.r, c.g), c.b); }
half lumv3(half3 c) { return dot(c, make_half3(.30, .59, .11)); }
half satv3(half3 c) { return maxv3(c) - minv3(c); }

// If any color components are outside [0,1], adjust the color to get the
// components in range.
half3 clip_color(half3 color)
{
    half lum = lumv3(color);
    half mincol = minv3(color);
    half maxcol = maxv3(color);
    if (mincol < .0)
        color = lum + ((color - lum) * lum) / (lum - mincol);
    if (maxcol > 1.)
        color = lum + ((color - lum) * (1. - lum)) / (maxcol - lum);
    return color;
}

// Take the base RGB color <cbase> and override its luminosity with that of the
// RGB color <clum>.
half3 set_lum(half3 cbase, half3 clum)
{
    half lbase = lumv3(cbase);
    half llum = lumv3(clum);
    half ldiff = llum - lbase;
    half3 color = cbase + make_half3(ldiff);
    return clip_color(color);
}

// Take the base RGB color <cbase> and override its saturation with that of the
// RGB color <csat>. The override the luminosity of the result with that of the
// RGB color <clum>.
half3 set_lum_sat(half3 cbase, half3 csat, half3 clum)
{
    half minbase = minv3(cbase);
    half sbase = satv3(cbase);
    half ssat = satv3(csat);
    half3 color;
    if (sbase > .0)
    {
        // Equivalent (modulo rounding errors) to setting the smallest (R,G,B)
        // component to 0, the largest to <ssat>, and interpolating the "middle"
        // component based on its original value relative to the
        // smallest/largest.
        color = (cbase - minbase) * ssat / sbase;
    }
    else
    {
        color = make_half3(.0);
    }
    return set_lum(color, clum);
}
#endif // ENABLE_HSL_BLEND_MODES

INLINE half3 unmultiply_rgb(half4 premul)
{
    // We *could* return preciesly 1 when premul.rgb == premul.a, but we can
    // also be approximate here. The blend modes that depend on this exact level
    // of precision (colordodge and colorburn) account for it with dstPremul.
    return premul.rgb * (premul.a != .0 ? 1. / premul.a : .0);
}

// The advanced blend coefficients are generated from un-multiplied RGB values,
// and control the look of each blend mode.
half3 advanced_blend_coeffs(half3 src, half4 dstPremul, ushort mode)
{
    half3 dst = unmultiply_rgb(dstPremul);
    half3 coeffs;
    switch (mode)
    {
        case BLEND_MODE_MULTIPLY:
            coeffs = src.rgb * dst.rgb;
            break;
        case BLEND_MODE_SCREEN:
            coeffs = src.rgb + dst.rgb - src.rgb * dst.rgb;
            break;
        case BLEND_MODE_OVERLAY:
        {
            for (int i = 0; i < 3; ++i)
            {
                if (dst[i] <= .5)
                    coeffs[i] = 2. * src[i] * dst[i];
                else
                    coeffs[i] = 1. - 2. * (1. - src[i]) * (1. - dst[i]);
            }
            break;
        }
        case BLEND_MODE_DARKEN:
            coeffs = min(src.rgb, dst.rgb);
            break;
        case BLEND_MODE_LIGHTEN:
            coeffs = max(src.rgb, dst.rgb);
            break;
        case BLEND_MODE_COLORDODGE:
        {
            dstPremul.rgb = clamp(dstPremul.rgb, make_half3(.0), dstPremul.aaa);
            half3 denom =
                clamp(1. - src, make_half3(.0), make_half3(1.)) * dstPremul.a;
            coeffs = mix(min(make_half3(1.), dstPremul.rgb / denom),
                         sign(dstPremul.rgb),
                         equal(denom, make_half3(.0)));
            break;
        }
        case BLEND_MODE_COLORBURN:
        {
            src = clamp(src, make_half3(.0), make_half3(1.));
            dstPremul.rgb = clamp(dstPremul.rgb, make_half3(.0), dstPremul.aaa);
            if (dstPremul.a == .0)
                dstPremul.a = 1.;
            half3 numer = dstPremul.a - dstPremul.rgb;
            coeffs = 1. - mix(min(make_half3(1.), numer / (src * dstPremul.a)),
                              sign(numer),
                              equal(src, make_half3(.0)));
            break;
        }
        case BLEND_MODE_HARDLIGHT:
        {
            for (int i = 0; i < 3; ++i)
            {
                if (src[i] <= .5)
                    coeffs[i] = 2. * src[i] * dst[i];
                else
                    coeffs[i] = 1. - 2. * (1. - src[i]) * (1. - dst[i]);
            }
            break;
        }
        case BLEND_MODE_SOFTLIGHT:
        {
            for (int i = 0; i < 3; ++i)
            {
                if (src[i] <= 0.5)
                    coeffs[i] =
                        dst[i] - (1. - 2. * src[i]) * dst[i] * (1. - dst[i]);
                else if (dst[i] <= .25)
                    coeffs[i] =
                        dst[i] + (2. * src[i] - 1.) * dst[i] *
                                     ((16. * dst[i] - 12.) * dst[i] + 3.);
                else
                    coeffs[i] =
                        dst[i] + (2. * src[i] - 1.) * (sqrt(dst[i]) - dst[i]);
            }
            break;
        }
        case BLEND_MODE_DIFFERENCE:
            coeffs = abs(dst.rgb - src.rgb);
            break;
        case BLEND_MODE_EXCLUSION:
            coeffs = src.rgb + dst.rgb - 2. * src.rgb * dst.rgb;
            break;
#ifdef @ENABLE_HSL_BLEND_MODES
        // The HSL blend equations are only well defined when the values of the
        // input color components are in the range [0..1].
        case BLEND_MODE_HUE:
            if (@ENABLE_HSL_BLEND_MODES)
            {
                src.rgb = clamp(src.rgb, make_half3(.0), make_half3(1.));
                coeffs = set_lum_sat(src.rgb, dst.rgb, dst.rgb);
            }
            break;
        case BLEND_MODE_SATURATION:
            if (@ENABLE_HSL_BLEND_MODES)
            {
                src.rgb = clamp(src.rgb, make_half3(.0), make_half3(1.));
                coeffs = set_lum_sat(dst.rgb, src.rgb, dst.rgb);
            }
            break;
        case BLEND_MODE_COLOR:
            if (@ENABLE_HSL_BLEND_MODES)
            {
                src.rgb = clamp(src.rgb, make_half3(.0), make_half3(1.));
                coeffs = set_lum(src.rgb, dst.rgb);
            }
            break;
        case BLEND_MODE_LUMINOSITY:
            if (@ENABLE_HSL_BLEND_MODES)
            {
                src.rgb = clamp(src.rgb, make_half3(.0), make_half3(1.));
                coeffs = set_lum(dst.rgb, src.rgb);
            }
            break;
#endif
    }
    return coeffs;
}

// Returns an intermediate RGB color that won't produce the desired blend mode
// until *AFTER* being blended into the framebuffer with hardware coefficients
// of [SRC_ALPHA, ONE_MINUS_SRC_ALPHA].
//
// NOTE: Alpha follows standard src-over rules in all blend modes, so the shader
// can just output the paint's alpha value, unmodified, directly to the blend
// unit.
INLINE half3 advanced_color_blend_pre_src_over(half3 src,
                                               half4 dstPremul,
                                               ushort mode)
{
    // The weighting functions p0, p1, and p2 are defined as follows:
    //
    //     p0(As,Ad) = As*Ad
    //     p1(As,Ad) = As*(1 - Ad)
    //     p2(As,Ad) = Ad*(1 - As)
    //
    // Blending is performed according to the following equations:
    //
    //     R = coeffs(Rs',Rd')*p0(As,Ad) + Y*Rs'*p1(As,Ad) + Z*Rd'*p2(As,Ad)
    //     G = coeffs(Gs',Gd')*p0(As,Ad) + Y*Gs'*p1(As,Ad) + Z*Gd'*p2(As,Ad)
    //     B = coeffs(Bs',Bd')*p0(As,Ad) + Y*Bs'*p1(As,Ad) + Z*Bd'*p2(As,Ad)
    //     A =               X*p0(As,Ad) +     Y*p1(As,Ad) +     Z*p2(As,Ad)
    //
    // NOTE: (X,Y,Z) always == 1, so it is ignored in this implementation.
    //       Also, since (X,Y,Z) == 1, alpha simplifies to standard src-over
    //       rules: A = Ad * (1 - As) + As
    //
    // Since we are returning an intermediate value for use *BEFORE* src-over
    // blend, we make two modifications:
    //
    //     1) To cancel the effect of the upcoming src-over blend, we subtract
    //        "RGBd * (1 - As)" from the final RGB values. This cancels out p2
    //        entirely.
    //
    //     2) Since the caller is expected to premultiply alpha, don't multiply
    //        p0 or p1 by As.
    //
    half3 coeffs = advanced_blend_coeffs(src, dstPremul, mode);
    half2 p = make_half2(dstPremul.a, 1. - dstPremul.a); // p2 cancelled to 0.
    return MUL(make_half2x3(coeffs, src), p);
}

INLINE half4 advanced_blend(half4 src, half4 dstPremul, ushort mode)
{
    src.rgb = advanced_color_blend_pre_src_over(src.rgb, dstPremul, mode);
    src.rgb *= src.a;
    return src + dstPremul * (1. - src.a);
}
#endif // ENABLE_ADVANCED_BLEND

#endif // FRAGMENT
