#include "rive/generated/text/text_style_feature_base.hpp"
#include "rive/text/text_style_feature.hpp"

using namespace rive;

Core* TextStyleFeatureBase::clone() const
{
    auto cloned = new TextStyleFeature();
    cloned->copy(*this);
    return cloned;
}
