#include "rive/generated/data_bind/converters/data_converter_interpolator_base.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"

using namespace rive;

Core* DataConverterInterpolatorBase::clone() const
{
    auto cloned = new DataConverterInterpolator();
    cloned->copy(*this);
    return cloned;
}
