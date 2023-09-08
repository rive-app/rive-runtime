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

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyFrameCallbackBase::typeKey:
            case KeyFrameBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif