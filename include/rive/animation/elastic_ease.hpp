#ifndef _RIVE_ELASTIC_EASE_HPP_
#define _RIVE_ELASTIC_EASE_HPP_

namespace rive
{
class ElasticEase
{
public:
    ElasticEase(float amplitude, float period);
    float easeOut(float factor) const;
    float easeIn(float factor) const;
    float easeInOut(float factor) const;

private:
    float computeActualAmplitude(float time) const;
    float m_amplitude;
    float m_period;

    // Computed phase shift for starting the sin function at 0 at factor 0.
    float m_s;
};
} // namespace rive

#endif