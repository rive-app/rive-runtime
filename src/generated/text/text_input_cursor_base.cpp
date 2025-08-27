#include "rive/generated/text/text_input_cursor_base.hpp"
#include "rive/text/text_input_cursor.hpp"

using namespace rive;

Core* TextInputCursorBase::clone() const
{
    auto cloned = new TextInputCursor();
    cloned->copy(*this);
    return cloned;
}
