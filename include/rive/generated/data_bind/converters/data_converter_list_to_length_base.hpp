#ifndef _RIVE_DATA_CONVERTER_LIST_TO_LENGTH_BASE_HPP_
#define _RIVE_DATA_CONVERTER_LIST_TO_LENGTH_BASE_HPP_
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterListToLengthBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 591;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif