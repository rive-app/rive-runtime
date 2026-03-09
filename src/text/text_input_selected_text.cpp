#include "rive/text/text_input_selected_text.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

Core* TextInputSelectedText::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

StatusCode TextInputSelectedText::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
#ifdef WITH_RIVE_TEXT
    textInput()->rawTextInput()->separateSelectionText(true);
#endif
    return StatusCode::Ok;
}

ShapePaintPath* TextInputSelectedText::localClockwisePath()
{
#ifdef WITH_RIVE_TEXT
    return textInput()->rawTextInput()->selectedTextPath();
#else
    return nullptr;
#endif
}