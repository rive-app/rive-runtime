#ifndef _RIVE_POLYGON_HPP_
#define _RIVE_POLYGON_HPP_
#include "generated/shapes/polygon_base.hpp"
#include "shapes/path_vertex.hpp"
#include <stdio.h>
namespace rive
{
	class Polygon : public PolygonBase
	{
	public:
		Polygon();
		~Polygon();
		void update(ComponentDirt value) override;

	protected:
		void cornerRadiusChanged() override;
		void pointsChanged() override;
		void clearVertices();
		void resizeVertices(int size);
		virtual int expectedSize();
		virtual void buildPolygon();
		void buildVertex(PathVertex* vertex, float h, float w, float angle);
	};
} // namespace rive

#endif