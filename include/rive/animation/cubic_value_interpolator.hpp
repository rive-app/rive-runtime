#ifndef _RIVE_CUBIC_VALUE_INTERPOLATOR_HPP_
#define _RIVE_CUBIC_VALUE_INTERPOLATOR_HPP_
#include "rive/generated/animation/cubic_value_interpolator_base.hpp"
#include <stdio.h>
namespace rive
{
class CubicValueInterpolator : public CubicValueInterpolatorBase
{
private:
    float m_A;
    float m_B;
    float m_C;
    float m_D;
    float m_ValueTo;

    void computeParameters();

public:
    CubicValueInterpolator();
    float transformValue(float valueFrom, float valueTo, float factor) override;
    float transform(float factor) const override;
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif