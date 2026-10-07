#ifndef _RIVE_KEY_FRAME_CALLBACK_BASE_HPP_
#define _RIVE_KEY_FRAME_CALLBACK_BASE_HPP_
#include "rive/animation/keyframe.hpp"
namespace rive
{
class KeyFrameCallbackBase : public KeyFrame
{
protected:
    typedef KeyFrame Super;

public:
    static const uint16_t typeKey = 171;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif