#include "generated/animation/keyframe_color_base.hpp"
#include "animation/keyframe_color.hpp"

using namespace rive;

Core* KeyFrameColorBase::clone() const
{
	auto cloned = new KeyFrameColor();
	cloned->copy(*this);
	return cloned;
}
