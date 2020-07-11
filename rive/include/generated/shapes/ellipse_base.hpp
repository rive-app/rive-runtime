#ifndef _RIVE_ELLIPSE_BASE_HPP_
#define _RIVE_ELLIPSE_BASE_HPP_
#include "shapes/parametric_path.hpp"
namespace rive
{
	class EllipseBase : public ParametricPath
	{
	public:
		static const int typeKey = 4;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif