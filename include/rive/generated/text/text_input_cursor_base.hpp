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

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif