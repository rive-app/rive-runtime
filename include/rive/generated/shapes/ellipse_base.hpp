#ifndef _RIVE_ELLIPSE_BASE_HPP_
#define _RIVE_ELLIPSE_BASE_HPP_
#include "rive/shapes/parametric_path.hpp"
namespace rive
{
class EllipseBase : public ParametricPath
{
protected:
    typedef ParametricPath Super;

public:
    static const uint16_t typeKey = 4;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case EllipseBase::typeKey:
            case ParametricPathBase::typeKey:
            case PathBase::typeKey:
            case NodeBase::typeKey:
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

    Core* clone() const override;

protected:
};
} // namespace rive

#endif