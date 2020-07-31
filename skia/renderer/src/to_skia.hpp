#ifndef _RIVE_TO_SKIA_HPP_
#define _RIVE_TO_SKIA_HPP_
#include "SkPaint.h"
#include "math/mat2d.hpp"
#include "math/vec2d.hpp"
#include "shapes/paint/stroke_cap.hpp"
#include "shapes/paint/stroke_join.hpp"
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
	};
} // namespace rive
#endif