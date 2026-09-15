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

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

    void copy(const KeyFrameInterpolatorBase& object)
    {
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        return false;
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/keyframe_interpolator_ext.inl"
#endif
};
} // namespace rive

#endif