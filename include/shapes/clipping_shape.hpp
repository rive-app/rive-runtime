#ifndef _RIVE_CLIPPING_SHAPE_HPP_
#define _RIVE_CLIPPING_SHAPE_HPP_
#include "generated/shapes/clipping_shape_base.hpp"
#include <stdio.h>
namespace rive
{
	enum class ClipOp : unsigned char
	{
		intersection = 0,
		difference = 1
	};

	class Shape;
	class ClippingShape : public ClippingShapeBase
	{
	private:
		Shape* m_Shape;

	public:
		Shape* shape() const { return m_Shape; }
		void onAddedClean(CoreContext* context) override;
		void onAddedDirty(CoreContext* context) override;
	};
} // namespace rive

#endif