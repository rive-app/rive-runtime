#ifndef _RIVE_SCRIPTED_LAYOUT_BASE_HPP_
#define _RIVE_SCRIPTED_LAYOUT_BASE_HPP_
#include "rive/scripted/scripted_drawable.hpp"
namespace rive
{
class ScriptedLayoutBase : public ScriptedDrawable
{
protected:
    typedef ScriptedDrawable Super;

public:
    static const uint16_t typeKey = 637;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptedLayoutBase::typeKey:
            case ScriptedDrawableBase::typeKey:
            case DrawableBase::typeKey:
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