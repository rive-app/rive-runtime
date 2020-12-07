#ifndef _RIVE_POLYGON_HPP_
#define _RIVE_POLYGON_HPP_
#include "generated/shapes/polygon_base.hpp"
#include <stdio.h>
namespace rive
{
	class Polygon : public PolygonBase
	{
	public:
		void update(ComponentDirt value) override;

	protected:
		void cornerRadiusChanged() override;
		void pointsChanged() override;
		void clearVertices();
	};
} // namespace rive

#endif