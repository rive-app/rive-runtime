#include "rive/generated/animation/elastic_interpolator_base.hpp"
#include "rive/animation/elastic_interpolator.hpp"

using namespace rive;

Core* ElasticInterpolatorBase::clone() const
{
    auto cloned = new ElasticInterpolator();
    cloned->copy(*this);
    return cloned;
}
