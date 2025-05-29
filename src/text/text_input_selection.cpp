#include "rive/text/text_input_selection.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

Core* TextInputSelection::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

ShapePaintPath* TextInputSelection::localClockwisePath()
{
#ifdef WITH_RIVE_TEXT
    return textInput()->rawTextInput()->selectionPath();
#else
    return nullptr;
#endif
}