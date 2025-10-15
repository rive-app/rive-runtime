#include "rive/generated/script_input_number_base.hpp"
#include "rive/script_input_number.hpp"

using namespace rive;

Core* ScriptInputNumberBase::clone() const
{
    auto cloned = new ScriptInputNumber();
    cloned->copy(*this);
    return cloned;
}
