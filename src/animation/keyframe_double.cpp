#include "animation/keyframe_double.hpp"
#include "generated/core_registry.hpp"

using namespace rive;

void KeyFrameDouble::apply(Core* object, int propertyKey, float mix)
{
	CoreRegistry::setDouble(object, propertyKey, value() * mix);
}

void KeyFrameDouble::applyInterpolation(Core* object,
                                        int propertyKey,
                                        float currentTime,
                                        const KeyFrame* nextFrame,
                                        float mix)
{
    auto kfd = nextFrame->as<KeyFrameDouble>();
    const KeyFrameDouble& nextDouble = *kfd;
	float f = (currentTime - seconds()) / (nextDouble.seconds() - seconds());

	if (CubicInterpolator* cubic = interpolator())
	{
		f = cubic->transform(f);
	}

	float interpolatedValue = value() + (nextDouble.value() - value()) * f;

	if (mix == 1.0f)
	{
		CoreRegistry::setDouble(object, propertyKey, interpolatedValue);
	}
	else
	{
		float mixi = 1.0 - mix;
		CoreRegistry::setDouble(object,
		                        propertyKey,
		                        CoreRegistry::getDouble(object, propertyKey) *
		                                mixi +
		                            interpolatedValue * mix);
	}
}