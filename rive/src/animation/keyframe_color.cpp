#include "animation/keyframe_color.hpp"
#include "generated/core_registry.hpp"
#include "shapes/paint/color.hpp"

using namespace rive;

void KeyFrameColor::apply(Core* object, int propertyKey, float mix)
{
	if (mix == 1)
	{
		CoreRegistry::setColor(object, propertyKey, value());
	}
	else
	{
		auto mixedColor =
		    colorLerp(CoreRegistry::getColor(object, propertyKey), value(), mix);
		CoreRegistry::setColor(object, propertyKey, mixedColor);
	}
}
void KeyFrameColor::applyInterpolation(Core* object,
                                       int propertyKey,
                                       float currentTime,
                                       const KeyFrame* nextFrame,
                                       float mix)
{
	auto kfc = nextFrame->as<KeyFrameColor>();
	const KeyFrameColor& nextColor = *kfc;
	float f = (currentTime - seconds()) / (nextColor.seconds() - seconds());

	if (CubicInterpolator* cubic = interpolator())
	{
		f = cubic->transform(f);
	}
	auto interpolatedValue = colorLerp(value(), nextColor.value(), f);
	if (mix == 1)
	{
		CoreRegistry::setColor(object, propertyKey, interpolatedValue);
	}
	else
	{
		auto mixedColor =
		    colorLerp(CoreRegistry::getColor(object, propertyKey),
		              interpolatedValue,
		              mix);
		CoreRegistry::setColor(object, propertyKey, mixedColor);
	}
}