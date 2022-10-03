#ifndef _RIVE_KEY_FRAME_HPP_
#define _RIVE_KEY_FRAME_HPP_
#include "rive/generated/animation/keyframe_base.hpp"
namespace rive
{
class CubicInterpolator;

class KeyFrame : public KeyFrameBase
{
private:
    CubicInterpolator* m_Interpolator = nullptr;
    float m_Seconds;

public:
    inline float seconds() const { return m_Seconds; }
    inline CubicInterpolator* interpolator() const { return m_Interpolator; }

    void computeSeconds(int fps);

    StatusCode onAddedDirty(CoreContext* context) override;
    virtual void apply(Core* object, int propertyKey, float mix) = 0;
    virtual void applyInterpolation(Core* object,
                                    int propertyKey,
                                    float seconds,
                                    const KeyFrame* nextFrame,
                                    float mix) = 0;

    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif