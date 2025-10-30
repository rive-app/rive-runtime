#include "rive/generated/scripted/scripted_data_converter_base.hpp"
#include "rive/scripted/scripted_data_converter.hpp"

using namespace rive;

Core* ScriptedDataConverterBase::clone() const
{
    auto cloned = new ScriptedDataConverter();
    cloned->copy(*this);
    return cloned;
}
