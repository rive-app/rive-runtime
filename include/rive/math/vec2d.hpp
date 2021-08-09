#ifndef _RIVE_VEC2D_HPP_
#define _RIVE_VEC2D_HPP_

#include <cstddef>

namespace rive
{
	class Mat2D;
	class Vec2D
	{
	private:
		float m_Buffer[2];

	public:
		Vec2D();
		Vec2D(const Vec2D& copy);
		Vec2D(float x, float y);

		inline const float* values() const { return m_Buffer; }

		float& operator[](std::size_t idx) { return m_Buffer[idx]; }
		const float& operator[](std::size_t idx) const { return m_Buffer[idx]; }

		static void transform(Vec2D& result, const Vec2D& a, const Mat2D& m);
		static void transformDir(Vec2D& result, const Vec2D& a, const Mat2D& m);
		static void subtract(Vec2D& result, const Vec2D& a, const Vec2D& b);
		static void add(Vec2D& result, const Vec2D& a, const Vec2D& b);
		static float length(const Vec2D& a);
		static float lengthSquared(const Vec2D& a);
		static float distance(const Vec2D& a, const Vec2D& b);
		static float distanceSquared(const Vec2D& a, const Vec2D& b);
		static void copy(Vec2D& result, const Vec2D& a);
		static void normalize(Vec2D& result, const Vec2D& a);
		static float dot(const Vec2D& a, const Vec2D& b);
		static void lerp(Vec2D& o, const Vec2D& a, const Vec2D& b, float f);
		static void scale(Vec2D& o, const Vec2D& a, float scale);
		static void scaleAndAdd(Vec2D& o, const Vec2D& a, const Vec2D& b, float scale);
		static void negate(Vec2D& o, const Vec2D& a);
	};

	inline Vec2D operator*(const Mat2D& a, const Vec2D& b)
	{
		Vec2D result;
		Vec2D::transform(result, b, a);
		return result;
	}

	inline Vec2D operator-(const Vec2D& a, const Vec2D& b)
	{
		Vec2D result;
		Vec2D::subtract(result, a, b);
		return result;
	}

	inline Vec2D operator+(const Vec2D& a, const Vec2D& b)
	{
		Vec2D result;
		Vec2D::add(result, a, b);
		return result;
	}

	inline bool operator==(const Vec2D& a, const Vec2D& b) { return a[0] == b[0] && a[1] == b[1]; }
	inline bool operator!=(const Vec2D& a, const Vec2D& b) { return a[0] != b[0] || a[1] != b[1]; }
} // namespace rive
#endif