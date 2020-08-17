#ifndef _RIVE_ANIMATION_HPP_
#define _RIVE_ANIMATION_HPP_
#include "generated/animation/animation_base.hpp"
namespace rive
{
	class Animation : public AnimationBase
	{
	public:
		void onAddedDirty(CoreContext* context) override {}
		void onAddedClean(CoreContext* context) override {}
	};
} // namespace rive

#endif