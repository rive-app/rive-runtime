#include "rive/text/text_input_cursor.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

Core* TextInputCursor::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

ShapePaintPath* TextInputCursor::localClockwisePath()
{
#ifdef WITH_RIVE_TEXT
    return textInput()->rawTextInput()->cursorPath();
#else
    return nullptr;
#endif
}