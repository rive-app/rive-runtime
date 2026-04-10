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
// Note: the following routines are mathematically equivalent to those in
// https://registry.khronos.org/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt
// but have been rearranged to be more efficient.

// When using one of the HSL blend equations in table X.2 as the blend equation,
// the blend coefficients are effectively obtained by converting both the
// non-premultiplied source and destination colors to the HSL (hue, saturation,
// luminosity) color space, generating a new HSL color by selecting H, S, and L
// components from the source or destination according to the blend equation,
// and then converting the result back to RGB. The HSL blend equations are only
// well defined when the values of the input color components are in the range
// [0..1].
half lum_from_rgb(half3 c) { return dot(c, make_half3(.30, .59, .11)); }

// Take the base RGB color and override its luminosity with that of another
// RGB color (lumColor).
half3 set_lum(half3 baseColor, half3 lumColor)
{
    half lumTarget = lum_from_rgb(lumColor);

    // Bias the color such that its original luminance is now the 0 point.
    half3 biased = baseColor - lum_from_rgb(baseColor);

    // Now we potentially need to rescale the color to get it to fit within
    // 0..1. Effectively to keep the relative luminance, we may need to squish
    // the rgb range so it still fits.

    //  Calculate the scale values necessary to push the min or max component
    //  back into range. One is for if the luminance pushes the rgb values
    //  negative (pushing min component back into range), the other for if they
    //  go above 1.0 (pushing max component).
    half2 scales =
        make_half2(lumTarget, 1.0 - lumTarget) /
        max(make_half2(EPSILON_FP16_NON_DENORM),
            make_half2(-min_component(biased), max_component(biased)));

    // Take the minimum scale of the above (but nothing larger than 1, since we
    // only ever want to scale *down* the range)
    half satScale = min(make_half(1.0), min(scales.x, scales.y));

    // Now we can apply the scale to get the rgb range right, then re-bias it
    // back into the correct place (at the new luminance)
    return biased * satScale + lumTarget;
}

// Take the hue from hueColor, the saturation from satColor, and the luminance
// from lumColor and combine them into one resulting color.
half3 set_lum_sat(half3 hueColor, half3 satColor, half3 lumColor)
{
    // The saturation of a color is the difference between its max and min
    // components.
    float satTarget = max_component(satColor) - min_component(satColor);

    // Bias the hue color so its min component is 0 (this is not *strictly*
    // required but it simplifies the saturation calculation and matches the
    // canonical math).
    hueColor -= min_component(hueColor);

    // Because of that bias, the minimum component is now 0, so the saturation
    // is just the max component.
    float satSource = max_component(hueColor);

    // Rescale hueColor to have the new saturation. If satSource == 0, then
    // hueColor == {0, 0, 0}, so do a max on the denominator to avoid a divide
    // by 0.
    float scale = satTarget / max(EPSILON_FP16_NON_DENORM, satSource);

    // Now apply the luminance from lumColor to the rescaled color.
    return set_lum(hueColor * scale, lumColor);
}
#endif // ENABLE_HSL_BLEND_MODES

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
            // This logic is equivalent to the following, but should be more
            // efficient, and works around a Vulkan Adreno 6-series Android 9/10
            // driver bug:
            //  f(Cs,Cd) = 2*Cs*Cd, if Cd <= 0.5
            //             1-2*(1-Cs)*(1-Cd), otherwise
            half3 sd = src * dst;
            coeffs = 2.0 * mix(sd,
                               src + dst - sd - 0.5,
                               greaterThan(dst, make_half3(0.5)));
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
            // This logic is equivalent to the following, but should be more
            // efficient, and works around a Vulkan Adreno 6-series Android 9/10
            // driver bug:
            //   f(Cs,Cd) = 2*Cs*Cd, if Cs <= 0.5
            //              1-2*(1-Cs)*(1-Cd), otherwise
            half3 sd = src * dst;
            coeffs = 2.0 * mix(sd,
                               src + dst - sd - 0.5,
                               greaterThan(src, make_half3(0.5)));
            break;
        }
        case BLEND_MODE_SOFTLIGHT:
        {
            // This logic is equivalent to the following, but should be more
            // efficient, and works around a Vulkan Adreno 6-series Android 9/10
            // driver bug:
            //   f(Cs,Cd) =
            //     Cd-(1-2*Cs)*Cd*(1-Cd),
            //       if Cs <= 0.5
            //     Cd+(2*Cs-1)*Cd*((16*Cd-12)*Cd+3),
            //       if Cs > 0.5 and Cd <= 0.25
            //     Cd+(2*Cs-1)*(sqrt(Cd)-Cd),
            //       if Cs > 0.5 and Cd > 0.25
            for (int i = 0; i < 3; ++i)
            {
                if (src[i] <= 0.5)
                    coeffs[i] = (1.0 - dst[i]);
                else if (dst[i] <= 0.25)
                    coeffs[i] = ((16.0 * dst[i] - 12.0) * dst[i] + 3.0);
                else
                    coeffs[i] = (inversesqrt(dst[i]) - 1.0);
            }

            coeffs = dst + dst * (2.0 * src - 1.0) * coeffs;
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

// Performs the given advanced blend operation with a solid RGB src color (no
// srcAlpha).
//
// NOTE: This method is sufficient for all blending because alpha in the src
// can be accounted for afterward using a standard src-over blend operation.
//
// e.g., dst = blend_src_over(
//           premultiply(advanced_color_blend(src.rgb, dstPremul)),
//           dstPremul)
INLINE half3 advanced_color_blend(half3 src, half4 dstPremul, ushort mode)
{
    // The weighting functions p0, p1, and p2 are defined as follows:
    //
    //     p0(As,Ad) = As*Ad
    //     p1(As,Ad) = As*(1 - Ad)
    //     p2(As,Ad) = Ad*(1 - As)
    //
    // Since srcAlpha (As) == 1, this simplifies to:
    //
    //     p0(As,Ad) = Ad
    //     p1(As,Ad) = (1 - Ad)
    //     p2(As,Ad) = 0
    //
    // Blending is performed according to the following equations:
    //
    //     R = coeffs(Rs',Rd')*p0 + Y*Rs'*p1 + Z*Rd'*p2
    //     G = coeffs(Gs',Gd')*p0 + Y*Gs'*p1 + Z*Gd'*p2
    //     B = coeffs(Bs',Bd')*p0 + Y*Bs'*p1 + Z*Bd'*p2
    //     A =               X*p0 +     Y*p1 +     Z*p2
    //
    // NOTE: (X,Y,Z) always == 1, so it is ignored in this implementation.
    //       Also, since (X,Y,Z) == 1, alpha simplifies to standard src-over
    //       rules: A = Ad * (1 - As) + As
    half3 coeffs = advanced_blend_coeffs(src, dstPremul, mode);

    // Because p0 is (Ad), p1 is (1 - Ad), and p2 is 0, this is equivalent to
    // that matrix multiply:
    return mix(src, coeffs, make_half3(dstPremul.a));
}
#endif // ENABLE_ADVANCED_BLEND

#endif // FRAGMENT
