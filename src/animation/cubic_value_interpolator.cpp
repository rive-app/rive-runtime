#include "rive/animation/cubic_value_interpolator.hpp"

using namespace rive;

CubicValueInterpolator::CubicValueInterpolator() : m_D(0.0f), m_ValueTo(0.0f)
{
    computeParameters();
}
void CubicValueInterpolator::computeParameters()
{
    float y1 = m_D;
    float y2 = CubicValueInterpolator::y1();
    float y3 = CubicValueInterpolator::y2();
    float y4 = m_ValueTo;

    m_A = y4 + 3 * (y2 - y3) - y1;
    m_B = 3 * (y3 - y2 * 2 + y1);
    m_C = 3 * (y2 - y1);
    // m_D = y1;
}

float CubicValueInterpolator::transformValue(float valueFrom, float valueTo, float factor)
{
    if (m_D != valueFrom || m_ValueTo != valueTo)
    {
        m_D = valueFrom;
        m_ValueTo = valueTo;
        computeParameters();
    }
    float t = m_solver.getT(factor);
    return ((m_A * t + m_B) * t + m_C) * t + m_D;
}

float CubicValueInterpolator::transform(float factor) const
{
    assert(false);
    return factor;
}

StatusCode CubicValueInterpolator::onAddedDirty(CoreContext* context)
{
    computeParameters();
    return Super::onAddedDirty(context);
}