#include "rive/animation/cubic_interpolator_solver.hpp"
#include <cmath>

using namespace rive;

const int NewtonIterations = 4;
const float NewtonMinSlope = 0.001f;
const float SubdivisionPrecision = 0.0000001f;
const int SubdivisionMaxIterations = 10;

// Returns x(t) given t, x1, and x2, or y(t) given t, y1, and y2.
float CubicInterpolatorSolver::calcBezier(float aT, float aA1, float aA2)
{
    return (((1.0f - 3.0f * aA2 + 3.0f * aA1) * aT + (3.0f * aA2 - 6.0f * aA1)) * aT +
            (3.0f * aA1)) *
           aT;
}

// Returns dx/dt given t, x1, and x2, or dy/dt given t, y1, and y2.
static float getSlope(float aT, float aA1, float aA2)
{
    return 3.0f * (1.0f - 3.0f * aA2 + 3.0f * aA1) * aT * aT +
           2.0f * (3.0f * aA2 - 6.0f * aA1) * aT + (3.0f * aA1);
}

void CubicInterpolatorSolver::build(float x1, float x2)
{
    m_x1 = x1;
    m_x2 = x2;
    for (int i = 0; i < SplineTableSize; ++i)
    {
        m_values[i] = calcBezier(i * SampleStepSize, x1, x2);
    }
}

float CubicInterpolatorSolver::getT(float x) const
{
    float intervalStart = 0.0f;
    int currentSample = 1;
    int lastSample = SplineTableSize - 1;

    for (; currentSample != lastSample && m_values[currentSample] <= x; ++currentSample)
    {
        intervalStart += SampleStepSize;
    }
    --currentSample;

    // Interpolate to provide an initial guess for t
    float dist =
        (x - m_values[currentSample]) / (m_values[currentSample + 1] - m_values[currentSample]);
    float guessForT = intervalStart + dist * SampleStepSize;

    float initialSlope = getSlope(guessForT, m_x1, m_x2);
    if (initialSlope >= NewtonMinSlope)
    {
        for (int i = 0; i < NewtonIterations; ++i)
        {
            float currentSlope = getSlope(guessForT, m_x1, m_x2);
            if (currentSlope == 0.0f)
            {
                return guessForT;
            }
            float currentX = calcBezier(guessForT, m_x1, m_x2) - x;
            guessForT -= currentX / currentSlope;
        }
        return guessForT;
    }
    else if (initialSlope == 0.0f)
    {
        return guessForT;
    }
    else
    {
        float aB = intervalStart + SampleStepSize;
        float currentX, currentT;
        int i = 0;
        do
        {
            currentT = intervalStart + (aB - intervalStart) / 2.0f;
            currentX = calcBezier(currentT, m_x1, m_x2) - x;
            if (currentX > 0.0f)
            {
                aB = currentT;
            }
            else
            {
                intervalStart = currentT;
            }
        } while (std::abs(currentX) > SubdivisionPrecision && ++i < SubdivisionMaxIterations);
        return currentT;
    }
}