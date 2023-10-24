#ifndef _RIVE_KEY_FRAME_INTERPOLATOR_BASE_HPP_
#define _RIVE_KEY_FRAME_INTERPOLATOR_BASE_HPP_
#include "rive/core.hpp"
namespace rive
{
class KeyFrameInterpolatorBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 175;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyFrameInterpolatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    void copy(const KeyFrameInterpolatorBase& object) {}

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override { return false; }

protected:
};
} // namespace rive

#endif