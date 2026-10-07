#ifndef _RIVE_DATA_CONVERTER_BOOLEAN_NEGATE_BASE_HPP_
#define _RIVE_DATA_CONVERTER_BOOLEAN_NEGATE_BASE_HPP_
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterBooleanNegateBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 535;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif