#include "rive/animation/cubic_interpolator.hpp"

using namespace rive;

StatusCode CubicInterpolator::onAddedDirty(CoreContext* context)
{
    m_solver.build(x1(), x2());
    return StatusCode::Ok;
}