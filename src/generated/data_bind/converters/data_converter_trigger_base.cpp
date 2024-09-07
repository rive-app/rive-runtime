#include "rive/generated/data_bind/converters/data_converter_trigger_base.hpp"
#include "rive/data_bind/converters/data_converter_trigger.hpp"

using namespace rive;

Core* DataConverterTriggerBase::clone() const
{
    auto cloned = new DataConverterTrigger();
    cloned->copy(*this);
    return cloned;
}
