#include "rive/text/text_style_axis.hpp"
#include "rive/text/text_style.hpp"
#include "rive/container_component.hpp"

using namespace rive;

StatusCode TextStyleAxis::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code == StatusCode::Ok)
    {
        if (!parent()->is<TextStyle>())
        {
            return StatusCode::InvalidObject;
        }
        auto style = parent()->as<TextStyle>();
        style->addVariation(this);
    }
    return code;
}

void TextStyleAxis::tagChanged() { parent()->addDirt(ComponentDirt::TextShape); }

void TextStyleAxis::axisValueChanged() { parent()->addDirt(ComponentDirt::TextShape); }