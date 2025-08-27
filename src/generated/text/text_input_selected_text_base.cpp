#include "rive/generated/text/text_input_selected_text_base.hpp"
#include "rive/text/text_input_selected_text.hpp"

using namespace rive;

Core* TextInputSelectedTextBase::clone() const
{
    auto cloned = new TextInputSelectedText();
    cloned->copy(*this);
    return cloned;
}
