#ifndef _RIVE_SKELETAL_COMPONENT_BASE_HPP_
#define _RIVE_SKELETAL_COMPONENT_BASE_HPP_
#include "rive/transform_component.hpp"
namespace rive
{
class SkeletalComponentBase : public TransformComponent
{
protected:
    typedef TransformComponent Super;

public:
    static const uint16_t typeKey = 39;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case SkeletalComponentBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

protected:
};
} // namespace rive

#endif