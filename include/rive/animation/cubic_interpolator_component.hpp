#ifndef _RIVE_CUBIC_INTERPOLATOR_COMPONENT_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_COMPONENT_HPP_
#include "rive/generated/animation/cubic_interpolator_component_base.hpp"
#include "rive/animation/cubic_interpolator_solver.hpp"

namespace rive
{
class CubicInterpolatorComponent : public CubicInterpolatorComponentBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    float transform(float factor) const;

private:
    CubicInterpolatorSolver m_solver;
};
} // namespace rive

#endif