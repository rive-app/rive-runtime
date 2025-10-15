#include "rive/generated/script_input_string_base.hpp"
#include "rive/script_input_string.hpp"

using namespace rive;

Core* ScriptInputStringBase::clone() const
{
    auto cloned = new ScriptInputString();
    cloned->copy(*this);
    return cloned;
}
