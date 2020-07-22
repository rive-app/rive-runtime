#ifndef _RIVE_CUBIC_VERTEX_HPP_
#define _RIVE_CUBIC_VERTEX_HPP_
#include "generated/shapes/cubic_vertex_base.hpp"
#include "math/vec2d.hpp"

namespace rive
{
	class Vec2D;
	class CubicVertex : public CubicVertexBase
	{
	protected:
		bool m_InValid = false;
		bool m_OutValid = false;
		Vec2D m_InPoint;
		Vec2D m_OutPoint;
		
		virtual void computeIn() = 0;
		virtual void computeOut() = 0;

	public:
		const Vec2D& outPoint();
		const Vec2D& inPoint();
	};
} // namespace rive

#endif