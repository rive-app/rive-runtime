#ifndef _RIVE_ANIMATION_HPP_
#define _RIVE_ANIMATION_HPP_
#include "generated/animation/animation_base.hpp"
namespace rive
{
	class Animation : public AnimationBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override
		{
			return StatusCode::Ok;
		}
		StatusCode onAddedClean(CoreContext* context) override
		{
			return StatusCode::Ok;
		}
	};
} // namespace rive

#endif