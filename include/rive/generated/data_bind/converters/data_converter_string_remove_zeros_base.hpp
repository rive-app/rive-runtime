#ifndef _RIVE_DATA_CONVERTER_STRING_REMOVE_ZEROS_BASE_HPP_
#define _RIVE_DATA_CONVERTER_STRING_REMOVE_ZEROS_BASE_HPP_
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterStringRemoveZerosBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 531;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif