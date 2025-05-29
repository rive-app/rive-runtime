#ifndef _RIVE_TEXT_STYLE_PAINT_BASE_HPP_
#define _RIVE_TEXT_STYLE_PAINT_BASE_HPP_

#include "rive/text/text_style.hpp"

namespace rive
{
class TextStylePaintBase : public TextStyle
{
protected:
    typedef TextStyle Super;

public:
    static const uint16_t typeKey = 137;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextStylePaintBase::typeKey:
            case TextStyleBase::typeKey:
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