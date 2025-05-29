#ifndef _RIVE_TEXT_INPUT_TEXT_HPP_
#define _RIVE_TEXT_INPUT_TEXT_HPP_
#include "rive/generated/text/text_input_text_base.hpp"
#include <stdio.h>
namespace rive
{
class TextInputText : public TextInputTextBase
{
public:
    Core* hitTest(HitInfo*, const Mat2D&) override;
    ShapePaintPath* localClockwisePath() override;
};
} // namespace rive

#endif