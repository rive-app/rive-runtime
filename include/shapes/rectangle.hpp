#ifndef _RIVE_RECTANGLE_HPP_
#define _RIVE_RECTANGLE_HPP_
#include "generated/shapes/rectangle_base.hpp"
#include "shapes/straight_vertex.hpp"

namespace rive
{
	class Rectangle : public RectangleBase
	{
		StraightVertex m_Vertex1, m_Vertex2, m_Vertex3, m_Vertex4;

	public:
		Rectangle();
		void update(ComponentDirt value) override;

	protected:
		void cornerRadiusChanged() override;
	};
} // namespace rive

#endif