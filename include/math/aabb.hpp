#ifndef _RIVE_AABB_HPP_
#define _RIVE_AABB_HPP_

#include "mat2d.hpp"
#include "vec2d.hpp"
#include <cstddef>

namespace rive
{
	class AABB
	{
	public:
		union
		{
			float buffer[4];
			struct
			{
				Vec2D min, max;
			};
			struct
			{
				float minX, minY, maxX, maxY;
			};
		};

	public:
		AABB();
		AABB(const AABB& copy);
		AABB(float minX, float minY, float maxX, float maxY);

		inline const float* values() const { return buffer; }

		float& operator[](std::size_t idx) { return buffer[idx]; }
		const float& operator[](std::size_t idx) const { return buffer[idx]; }

		static void center(Vec2D& out, const AABB& a);
		static void size(Vec2D& out, const AABB& a);
		static void extents(Vec2D& out, const AABB& a);
		static void combine(AABB& out, const AABB& a, const AABB& b);
		static bool contains(const AABB& a, const AABB& b);
		static bool isValid(const AABB& a);
		static bool testOverlap(const AABB& a, const AABB& b);
		static bool areIdentical(const AABB& a, const AABB& b);
		static void transform(AABB& out, const AABB& a, const Mat2D& matrix);

		float width() const;
		float height() const;
		float perimeter() const;
	};
} // namespace rive
#endif