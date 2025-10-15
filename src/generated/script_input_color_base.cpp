#include "rive/generated/script_input_color_base.hpp"
#include "rive/script_input_color.hpp"

using namespace rive;

Core* ScriptInputColorBase::clone() const
{
    auto cloned = new ScriptInputColor();
    cloned->copy(*this);
    return cloned;
}
