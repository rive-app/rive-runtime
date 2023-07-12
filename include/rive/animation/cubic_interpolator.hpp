#ifndef _RIVE_CUBIC_INTERPOLATOR_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_HPP_
#include "rive/generated/animation/cubic_interpolator_base.hpp"
#include "rive/animation/cubic_interpolator_solver.hpp"

namespace rive
{
class CubicInterpolator : public CubicInterpolatorBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;

    /// Convert a linear interpolation value to an eased one.
    virtual float transformValue(float valueFrom, float valueTo, float factor) = 0;

    /// Convert a linear interpolation factor to an eased one.
    virtual float transform(float factor) const = 0;

    StatusCode import(ImportStack& importStack) override;

protected:
    CubicInterpolatorSolver m_solver;
};
} // namespace rive

#endif