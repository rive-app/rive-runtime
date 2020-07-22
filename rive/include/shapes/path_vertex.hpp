#ifndef _RIVE_PATH_VERTEX_HPP_
#define _RIVE_PATH_VERTEX_HPP_
#include "generated/shapes/path_vertex_base.hpp"
namespace rive
{
	class PathVertex : public PathVertexBase
	{
	public:
		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override {}
	};
} // namespace rive

#endif