#include "rive/generated/data_bind/converters/data_converter_system_normalizer_base.hpp"
#include "rive/data_bind/converters/data_converter_system_normalizer.hpp"

using namespace rive;

Core* DataConverterSystemNormalizerBase::clone() const
{
    auto cloned = new DataConverterSystemNormalizer();
    cloned->copy(*this);
    return cloned;
}
