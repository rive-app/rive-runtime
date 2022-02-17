#ifndef _RIVE_CUBIC_INTERPOLATOR_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_HPP_
#include "rive/generated/animation/cubic_interpolator_base.hpp"
namespace rive {
    class CubicInterpolator : public CubicInterpolatorBase {
    private:
        static constexpr int SplineTableSize = 11;
        static constexpr float SampleStepSize = 1.0f / (SplineTableSize - 1.0f);
        float m_Values[SplineTableSize];

        float getT(float x) const;

    public:
        StatusCode onAddedDirty(CoreContext* context) override;

        /// Convert a linear interpolation factor to an eased one.
        float transform(float value) const;

        StatusCode import(ImportStack& importStack) override;
    };
} // namespace rive

#endif