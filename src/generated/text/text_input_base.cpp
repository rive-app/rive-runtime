#include "rive/generated/text/text_input_base.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

Core* TextInputBase::clone() const
{
    auto cloned = new TextInput();
    cloned->copy(*this);
    return cloned;
}
