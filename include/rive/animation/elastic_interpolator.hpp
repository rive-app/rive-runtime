#ifndef _RIVE_ELASTIC_INTERPOLATOR_HPP_
#define _RIVE_ELASTIC_INTERPOLATOR_HPP_
#include "rive/generated/animation/elastic_interpolator_base.hpp"
#include "rive/animation/elastic_ease.hpp"
#include "rive/animation/easing.hpp"

namespace rive
{
class ElasticInterpolator : public ElasticInterpolatorBase
{
public:
    ElasticInterpolator();
    StatusCode onAddedDirty(CoreContext* context) override;
    float transformValue(float valueFrom, float valueTo, float factor) override;
    float transform(float factor) const override;

    Easing easing() const { return (Easing)easingValue(); }

private:
    ElasticEase m_elastic;
};
} // namespace rive

#endif