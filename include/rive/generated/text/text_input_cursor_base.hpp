#ifndef _RIVE_TEXT_INPUT_CURSOR_BASE_HPP_
#define _RIVE_TEXT_INPUT_CURSOR_BASE_HPP_
#include "rive/text/text_input_drawable.hpp"
namespace rive
{
class TextInputCursorBase : public TextInputDrawable
{
protected:
    typedef TextInputDrawable Super;

public:
    static const uint16_t typeKey = 571;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextInputCursorBase::typeKey:
            case TextInputDrawableBase::typeKey:
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