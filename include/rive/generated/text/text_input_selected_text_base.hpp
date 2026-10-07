#ifndef _RIVE_TEXT_INPUT_SELECTED_TEXT_BASE_HPP_
#define _RIVE_TEXT_INPUT_SELECTED_TEXT_BASE_HPP_
#include "rive/text/text_input_drawable.hpp"
namespace rive
{
class TextInputSelectedTextBase : public TextInputDrawable
{
protected:
    typedef TextInputDrawable Super;

public:
    static const uint16_t typeKey = 575;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif