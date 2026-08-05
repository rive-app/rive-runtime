#include "rive/generated/shapes/paint/color_channels_base.hpp"
#include "rive/core.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
using namespace rive;
ColorChannelsBase* ColorChannelsBase::from(Core* object)
{
    switch (object->coreType())
    {
        case SolidColorBase::typeKey:
            return object->as<SolidColorBase>();
        case GradientStopBase::typeKey:
            return object->as<GradientStopBase>();
    }
    return nullptr;
}
