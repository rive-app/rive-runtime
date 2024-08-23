#include "rive/animation/cubic_ease_interpolator.hpp"

using namespace rive;

float CubicEaseInterpolator::transformValue(float valueFrom, float valueTo, float factor)
{
    return valueFrom + (valueTo - valueFrom) * transform(factor);
}

float CubicEaseInterpolator::transform(float factor) const
{
    return CubicInterpolatorSolver::calcBezier(m_solver.getT(factor), y1(), y2());
}