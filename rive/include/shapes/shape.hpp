#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_
#include "generated/shapes/shape_base.hpp"
#include "shapes/shape_paint_container.hpp"
#include <vector>

namespace rive
{
	class Path;
	class PathComposer;
	class Shape : public ShapeBase, public ShapePaintContainer
	{
	private:
		PathComposer* m_PathComposer = nullptr;
		std::vector<Path*> m_Paths;

	public:
		void buildDependencies() override;
		void addPath(Path* path);
		std::vector<Path*>& paths() { return m_Paths; }

		bool wantLocalPath() const { return true; }
		bool wantWorldPath() const { return true; }

		void update(ComponentDirt value) override;
		void draw(Renderer* renderer) override;

		void pathComposer(PathComposer* value);
		void pathChanged();
	};
} // namespace rive

#endif