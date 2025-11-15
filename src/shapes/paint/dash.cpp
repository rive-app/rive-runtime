#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/dash_path.hpp"

using namespace rive;

Dash::Dash() {}

Dash::Dash(float value, bool percentage)
{
    length(value);
    lengthIsPercentage(percentage);
}

float Dash::normalizedLength(float contourLength) const
{
    float right = lengthIsPercentage() ? 1.0f : contourLength;
    if (right == 0.0f)
    {
        return 0.0f;
    }

    float p = fmodf(length(), right);
    if (p < 0.0f)
    {
        p += right;
    }
    return lengthIsPercentage() ? p * contourLength : p;
}

StatusCode Dash::onAddedClean(CoreContext* context)
{
    if (!parent()->is<DashPath>())
    {
        return StatusCode::InvalidObject;
    }

    return StatusCode::Ok;
}

void Dash::lengthChanged()
{
    if (parent() == nullptr || !parent()->is<DashPath>())
    {
        return;
    }
    parent()->as<DashPath>()->invalidateDash();
}

void Dash::lengthIsPercentageChanged() { lengthChanged(); }
