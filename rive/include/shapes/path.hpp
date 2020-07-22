#ifndef _RIVE_PATH_HPP_
#define _RIVE_PATH_HPP_
#include "generated/shapes/path_base.hpp"
#include "math/mat2d.hpp"

namespace rive
{
	class Shape;
	class RenderPath;

	class Path : public PathBase
	{
	private:
		Shape* m_Shape = nullptr;
		RenderPath* m_RenderPath = nullptr;

	public:
		~Path();
		Shape* shape() const { return m_Shape; }
		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override;
		const Mat2D& pathTransform() const;
		RenderPath* renderPath() const { return m_RenderPath; }
		void update(ComponentDirt value) override;
	};
} // namespace rive

#endif