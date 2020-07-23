#ifndef _RIVE_POINTS_PATH_HPP_
#define _RIVE_POINTS_PATH_HPP_
#include "generated/shapes/points_path_base.hpp"
namespace rive
{
	class PointsPath : public PointsPathBase
	{
	public:
		bool isPathClosed() const override { return isClosed(); }
	};
} // namespace rive

#endif