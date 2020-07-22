#ifndef _RIVE_FILL_HPP_
#define _RIVE_FILL_HPP_
#include "generated/shapes/paint/fill_base.hpp"
#include "shapes/path_space.hpp"
namespace rive
{
	class Fill : public FillBase
	{
	public:
		PathSpace pathSpace() const override;
	};
} // namespace rive

#endif