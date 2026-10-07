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

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif