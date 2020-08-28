#ifndef _RIVE_POINTS_PATH_HPP_
#define _RIVE_POINTS_PATH_HPP_
#include "bones/skinnable.hpp"
#include "generated/shapes/points_path_base.hpp"
namespace rive
{
	class PointsPath : public PointsPathBase, public Skinnable
	{
	public:
		bool isPathClosed() const override { return isClosed(); }
		void buildDependencies() override;
		void update(ComponentDirt value) override;
		void markPathDirty() override;
		void markSkinDirty() override;
	};
} // namespace rive

#endif