#include "rive/text/text_input_selected_text.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

Core* TextInputSelectedText::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

ShapePaintPath* TextInputSelectedText::localClockwisePath()
{
#ifdef WITH_RIVE_TEXT
    return textInput()->rawTextInput()->selectedTextPath();
#else
    return nullptr;
#endif
}