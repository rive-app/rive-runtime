#ifndef _RIVE_TO_SKIA_HPP_
#define _RIVE_TO_SKIA_HPP_

#include "SkPaint.h"
#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
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
			SkMatrix skMatrix;
			skMatrix.set9((SkScalar[9])
			              // Skia Matrix is row major
			              {// Row 1
			               m[0],
			               m[2],
			               m[4],
			               // Row 2
			               m[1],
			               m[3],
			               m[5],
			               // Row 3
			               0.0,
			               0.0,
			               1.0});
			return skMatrix;
		}

		static SkPoint convert(const rive::Vec2D& point)
		{
			return SkPoint::Make(point[0], point[1]);
		}

		static SkPaint::Cap convert(rive::StrokeCap cap)
		{
			switch (cap)
			{
				case rive::StrokeCap::butt:
					return SkPaint::Cap::kButt_Cap;
				case rive::StrokeCap::round:
					return SkPaint::Cap::kRound_Cap;
				case rive::StrokeCap::square:
					return SkPaint::Cap::kSquare_Cap;
			}
			return SkPaint::Cap::kButt_Cap;
		}

		static SkPaint::Join convert(rive::StrokeJoin join)
		{
			switch (join)
			{
				case rive::StrokeJoin::bevel:
					return SkPaint::Join::kBevel_Join;
				case rive::StrokeJoin::round:
					return SkPaint::Join::kRound_Join;
				case rive::StrokeJoin::miter:
					return SkPaint::Join::kMiter_Join;
			}
			return SkPaint::Join::kMiter_Join;
		}

		static SkBlendMode convert(rive::BlendMode blendMode)
		{
			switch (blendMode)
			{
				case rive::BlendMode::srcOver:
					return SkBlendMode::kSrcOver;
				case rive::BlendMode::screen:
					return SkBlendMode::kScreen;
				case rive::BlendMode::overlay:
					return SkBlendMode::kOverlay;
				case rive::BlendMode::darken:
					return SkBlendMode::kDarken;
				case rive::BlendMode::lighten:
					return SkBlendMode::kLighten;
				case rive::BlendMode::colorDodge:
					return SkBlendMode::kColorDodge;
				case rive::BlendMode::colorBurn:
					return SkBlendMode::kColorBurn;
				case rive::BlendMode::hardLight:
					return SkBlendMode::kHardLight;
				case rive::BlendMode::softLight:
					return SkBlendMode::kSoftLight;
				case rive::BlendMode::difference:
					return SkBlendMode::kDifference;
				case rive::BlendMode::exclusion:
					return SkBlendMode::kExclusion;
				case rive::BlendMode::multiply:
					return SkBlendMode::kMultiply;
				case rive::BlendMode::hue:
					return SkBlendMode::kHue;
				case rive::BlendMode::saturation:
					return SkBlendMode::kSaturation;
				case rive::BlendMode::color:
					return SkBlendMode::kColor;
				case rive::BlendMode::luminosity:
					return SkBlendMode::kLuminosity;
			}
			return SkBlendMode::kSrcOver;
		}
	};
} // namespace rive
#endif