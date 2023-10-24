#include "rive/animation/elastic_interpolator.hpp"

using namespace rive;

ElasticInterpolator::ElasticInterpolator() : m_elastic(1.0f, 1.0f) {}

StatusCode ElasticInterpolator::onAddedDirty(CoreContext* context)
{
    m_elastic = ElasticEase(amplitude(), period());
    return StatusCode::Ok;
}

float ElasticInterpolator::transformValue(float valueFrom, float valueTo, float factor)
{
    return valueFrom + (valueTo - valueFrom) * transform(factor);
}

float ElasticInterpolator::transform(float factor) const
{
    switch (easing())
    {
        case Easing::easeIn:
            return m_elastic.easeIn(factor);
        case Easing::easeOut:
            return m_elastic.easeOut(factor);
        case Easing::easeInOut:
            return m_elastic.easeInOut(factor);
    }
    return factor;
}