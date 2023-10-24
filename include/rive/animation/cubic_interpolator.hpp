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

protected:
    CubicInterpolatorSolver m_solver;
};
} // namespace rive

#endif