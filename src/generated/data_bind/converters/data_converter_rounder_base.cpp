#include "rive/generated/data_bind/converters/data_converter_rounder_base.hpp"
#include "rive/data_bind/converters/data_converter_rounder.hpp"

using namespace rive;

Core* DataConverterRounderBase::clone() const
{
    auto cloned = new DataConverterRounder();
    cloned->copy(*this);
    return cloned;
}
