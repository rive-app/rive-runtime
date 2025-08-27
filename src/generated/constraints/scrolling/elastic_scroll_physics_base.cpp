#include "rive/generated/constraints/scrolling/elastic_scroll_physics_base.hpp"
#include "rive/constraints/scrolling/elastic_scroll_physics.hpp"

using namespace rive;

Core* ElasticScrollPhysicsBase::clone() const
{
    auto cloned = new ElasticScrollPhysics();
    cloned->copy(*this);
    return cloned;
}
