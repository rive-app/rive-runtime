#include "rive/generated/text/text_input_text_base.hpp"
#include "rive/text/text_input_text.hpp"

using namespace rive;

Core* TextInputTextBase::clone() const
{
    auto cloned = new TextInputText();
    cloned->copy(*this);
    return cloned;
}
