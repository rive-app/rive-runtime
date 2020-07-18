#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_
#include "generated/shapes/shape_base.hpp"
namespace rive
{
	class Shape : public ShapeBase
	{
	public:
		void onAddedClean(CoreContext* context) {}
	};
} // namespace rive

#endif