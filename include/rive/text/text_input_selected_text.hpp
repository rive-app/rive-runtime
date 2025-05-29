#ifndef _RIVE_TEXT_INPUT_SELECTED_TEXT_HPP_
#define _RIVE_TEXT_INPUT_SELECTED_TEXT_HPP_
#include "rive/generated/text/text_input_selected_text_base.hpp"
#include <stdio.h>
namespace rive
{
class TextInputSelectedText : public TextInputSelectedTextBase
{
public:
    Core* hitTest(HitInfo*, const Mat2D&) override;
    ShapePaintPath* localClockwisePath() override;
};
} // namespace rive

#endif