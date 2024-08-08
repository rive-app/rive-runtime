/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_TO_SKIA_HPP_
#define _RIVE_TO_SKIA_HPP_

#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkTileMode.h"

#include "rive/math/math_types.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_join.hpp"
#include "rive/shapes/paint/blend_mode.hpp"

namespace rive
{
class ToSkia
{
public:
    static SkMatrix convert(const rive::Mat2D& m)
    {
        return SkMatrix::MakeAll(m[0], m[2], m[4], m[1], m[3], m[5], 0, 0, 1);
    }

    static SkPoint convert(rive::Vec2D point) { return SkPoint::Make(point.x, point.y); }

    // clang-format off
        static SkPathFillType convert(FillRule value) {
            switch (value) {
                case FillRule::evenOdd: return SkPathFillType::kEvenOdd;
                case FillRule::nonZero: return SkPathFillType::kWinding;
            }
            assert(false);
            return SkPathFillType::kWinding;
        }

        static SkPaint::Cap convert(rive::StrokeCap cap) {
            switch (cap) {
                case StrokeCap::butt:   return SkPaint::Cap::kButt_Cap;
                case StrokeCap::round:  return SkPaint::Cap::kRound_Cap;
                case StrokeCap::square: return SkPaint::Cap::kSquare_Cap;
            }
            assert(false);
            return SkPaint::Cap::kButt_Cap;
        }

        static SkPaint::Join convert(StrokeJoin join) {
            switch (join) {
                case StrokeJoin::bevel: return SkPaint::Join::kBevel_Join;
                case StrokeJoin::round: return SkPaint::Join::kRound_Join;
                case StrokeJoin::miter: return SkPaint::Join::kMiter_Join;
            }
            assert(false);
            return SkPaint::Join::kMiter_Join;
        }

        static SkBlendMode convert(BlendMode blendMode) {
            switch (blendMode) {
                case BlendMode::srcOver:    return SkBlendMode::kSrcOver;
                case BlendMode::screen:     return SkBlendMode::kScreen;
                case BlendMode::overlay:    return SkBlendMode::kOverlay;
                case BlendMode::darken:     return SkBlendMode::kDarken;
                case BlendMode::lighten:    return SkBlendMode::kLighten;
                case BlendMode::colorDodge: return SkBlendMode::kColorDodge;
                case BlendMode::colorBurn:  return SkBlendMode::kColorBurn;
                case BlendMode::hardLight:  return SkBlendMode::kHardLight;
                case BlendMode::softLight:  return SkBlendMode::kSoftLight;
                case BlendMode::difference: return SkBlendMode::kDifference;
                case BlendMode::exclusion:  return SkBlendMode::kExclusion;
                case BlendMode::multiply:   return SkBlendMode::kMultiply;
                case BlendMode::hue:        return SkBlendMode::kHue;
                case BlendMode::saturation: return SkBlendMode::kSaturation;
                case BlendMode::color:      return SkBlendMode::kColor;
                case BlendMode::luminosity: return SkBlendMode::kLuminosity;
            }
            assert(false);
            return SkBlendMode::kSrcOver;
        }

        static SkPath convert(const RawPath& rp) {
            const auto pts = rp.points();
            const auto vbs = rp.verbsU8();
            return SkPath::Make((const SkPoint*)pts.data(), pts.size(),
                                vbs.data(), math::lossless_numeric_cast<int>(vbs.size()),
                                nullptr, 0, SkPathFillType::kWinding);
        }
        // clang-format off
    };
} // namespace rive
#endif
