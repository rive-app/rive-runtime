#ifndef _RIVE_TEXT_INPUT_CURSOR_HPP_
#define _RIVE_TEXT_INPUT_CURSOR_HPP_
#include "rive/generated/text/text_input_cursor_base.hpp"
#include <stdio.h>
namespace rive
{
class TextInputCursor : public TextInputCursorBase
{
public:
    Core* hitTest(HitInfo*, const Mat2D&) override;
    ShapePaintPath* localClockwisePath() override;
};
} // namespace rive

#endif