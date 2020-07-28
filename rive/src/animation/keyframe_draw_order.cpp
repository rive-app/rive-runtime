#include "animation/keyframe_draw_order.hpp"

using namespace rive;
void KeyFrameDrawOrder::addValue(KeyFrameDrawOrderValue* value)
{
	m_Values.push_back(value);
}

void KeyFrameDrawOrder::apply(Core* object, int propertyKey, float mix) {}
void KeyFrameDrawOrder::applyInterpolation(
    Core* object, int propertyKey, float seconds, const KeyFrame* nextFrame, float mix)
{
}