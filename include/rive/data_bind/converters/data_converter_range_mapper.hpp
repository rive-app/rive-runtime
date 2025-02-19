#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_HPP_
#include "rive/generated/data_bind/converters/data_converter_range_mapper_base.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterRangeMapper : public DataConverterRangeMapperBase
{
protected:
    KeyFrameInterpolator* m_interpolator;
    DataValueNumber* calculateRange(DataValue* input,
                                    float minInput,
                                    float maxInput,
                                    float minOutput,
                                    float maxOutput);
    DataValueNumber* calculateReverseRange(DataValue* input,
                                           float minInput,
                                           float maxInput,
                                           float minOutput,
                                           float maxOutput);

public:
    void interpolator(KeyFrameInterpolator* interpolator);
    KeyFrameInterpolator* interpolator() const { return m_interpolator; };
    DataType outputType() override { return DataType::number; };
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    void copy(const DataConverterRangeMapperBase& object);
    void minInputChanged() override;
    void maxInputChanged() override;
    void minOutputChanged() override;
    void maxOutputChanged() override;

private:
    DataValueNumber m_output;
};
} // namespace rive

#endif