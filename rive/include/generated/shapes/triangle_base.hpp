#ifndef _RIVE_TRIANGLE_BASE_HPP_
#define _RIVE_TRIANGLE_BASE_HPP_
#include "shapes/parametric_path.hpp"
namespace rive
{
	class TriangleBase : public ParametricPath
	{
	public:
		static const int typeKey = 8;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif