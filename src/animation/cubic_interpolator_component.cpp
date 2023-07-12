#include "rive/animation/cubic_interpolator_component.hpp"

using namespace rive;

StatusCode CubicInterpolatorComponent::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    m_solver.build(x1(), x2());
    return StatusCode::Ok;
}

float CubicInterpolatorComponent::transform(float factor) const
{
    return CubicInterpolatorSolver::calcBezier(m_solver.getT(factor), y1(), y2());
}