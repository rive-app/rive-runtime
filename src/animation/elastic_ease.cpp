#include "rive/animation/elastic_ease.hpp"
#include "rive/math/math_types.hpp"
#include "math.h"

using namespace rive;

ElasticEase::ElasticEase(float amplitude, float period) :
    m_amplitude(amplitude),
    m_period(period),
    m_s(amplitude < 1.0f ? period / 4.0f : period / (2.0f * math::PI) * asinf(1.0f / amplitude))
{}

float ElasticEase::computeActualAmplitude(float time) const
{
    if (m_amplitude < 1.0f)
    {
        /// We use this when the amplitude is less than 1.0 (amplitude is
        /// described as factor of change in value). We also precompute s which is
        /// the effective starting period we use to align the decaying sin with
        /// our keyframe.
        float t = abs(m_s);
        float absTime = abs(time);
        if (absTime < t)
        {
            float l = absTime / t;
            return (m_amplitude * l) + (1.0f - l);
        }
    }

    return m_amplitude;
}

float ElasticEase::easeOut(float factor) const
{
    float time = factor;
    float actualAmplitude = computeActualAmplitude(time);

    return (actualAmplitude * pow(2.0f, 10.0f * -time) *
            sinf((time - m_s) * (2.0f * math::PI) / m_period)) +
           1.0f;
}

float ElasticEase::easeIn(float factor) const
{
    float time = factor - 1.0f;

    float actualAmplitude = computeActualAmplitude(time);

    return -(actualAmplitude * pow(2.0f, 10.0f * time) *
             sinf((-time - m_s) * (2.0f * math::PI) / m_period));
}

float ElasticEase::easeInOut(float factor) const
{
    float time = factor * 2.0f - 1.0f;
    float actualAmplitude = computeActualAmplitude(time);
    if (time < 0.0f)
    {
        return -0.5f * actualAmplitude * pow(2.0f, 10.0f * time) *
               sinf((-time - m_s) * (2.0f * math::PI) / m_period);
    }
    else
    {
        return 0.5f * (actualAmplitude * pow(2.0f, 10.0f * -time) *
                       sinf((time - m_s) * (2.0f * math::PI) / m_period)) +
               1.0f;
    }
}
