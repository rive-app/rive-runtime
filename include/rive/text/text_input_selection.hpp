#ifndef _RIVE_TEXT_INPUT_SELECTION_HPP_
#define _RIVE_TEXT_INPUT_SELECTION_HPP_
#include "rive/generated/text/text_input_selection_base.hpp"

namespace rive
{
class TextInputSelection : public TextInputSelectionBase
{
public:
    Core* hitTest(HitInfo*, const Mat2D&) override;
    ShapePaintPath* localClockwisePath() override;
};
} // namespace rive

#endif