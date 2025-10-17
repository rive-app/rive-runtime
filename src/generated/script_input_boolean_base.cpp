#include "rive/generated/script_input_boolean_base.hpp"
#include "rive/script_input_boolean.hpp"

using namespace rive;

Core* ScriptInputBooleanBase::clone() const
{
    auto cloned = new ScriptInputBoolean();
    cloned->copy(*this);
    return cloned;
}
