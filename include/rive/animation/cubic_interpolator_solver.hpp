#ifndef _RIVE_CUBIC_INTERPOLATOR_SOLVER_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_SOLVER_HPP_

namespace rive
{
// A helper for finding T based on X value.
class CubicInterpolatorSolver
{
public:
    void build(float x1, float x2);
    float getT(float x) const;
    static float calcBezier(float aT, float aA1, float aA2);

private:
    static constexpr int SplineTableSize = 11;
    static constexpr float SampleStepSize = 1.0f / (SplineTableSize - 1.0f);
    float m_values[SplineTableSize];
    float m_x1;
    float m_x2;
};
} // namespace rive

#endif