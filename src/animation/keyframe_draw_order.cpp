#include "animation/keyframe_draw_order.hpp"
#include "animation/keyframe_draw_order_value.hpp"
#include "artboard.hpp"
#include "drawable.hpp"

using namespace rive;
void KeyFrameDrawOrder::addValue(KeyFrameDrawOrderValue* value)
{
	m_Values.push_back(value);
}

void KeyFrameDrawOrder::apply(Core* object, int propertyKey, float mix)
{
	for (auto value : m_Values)
	{
		auto artboard = object->as<Artboard>();
		auto object = artboard->resolve(value->drawableId());
		if (object == nullptr)
		{
			continue;
		}
		auto drawable = object->as<Drawable>();
		drawable->drawOrder(value->value());
	}
}

void KeyFrameDrawOrder::applyInterpolation(Core* object,
                                           int propertyKey,
                                           float seconds,
                                           const KeyFrame* nextFrame,
                                           float mix)
{
	apply(object, propertyKey, mix);
}