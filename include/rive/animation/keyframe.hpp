#ifndef _RIVE_KEY_FRAME_HPP_
#define _RIVE_KEY_FRAME_HPP_
#include "rive/generated/animation/keyframe_base.hpp"
namespace rive
{
class KeyFrame : public KeyFrameBase
{
public:
    inline float seconds() const { return m_seconds; }

    void computeSeconds(int fps);

    StatusCode import(ImportStack& importStack) override;

private:
    float m_seconds;
};
} // namespace rive

#endif