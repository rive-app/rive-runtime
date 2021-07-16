#include "generated/animation/blend_animation_direct_base.hpp"
#include "animation/blend_animation_direct.hpp"

using namespace rive;

Core* BlendAnimationDirectBase::clone() const
{
	auto cloned = new BlendAnimationDirect();
	cloned->copy(*this);
	return cloned;
}
