#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/animation/data_converter_range_mapper_flags.hpp"

using namespace rive;

void DataConverterRangeMapper::copy(const DataConverterRangeMapperBase& object)
{
    interpolator(object.as<DataConverterRangeMapper>()->interpolator());
    DataConverterRangeMapperBase::copy(object);
}

DataValueNumber* DataConverterRangeMapper::calculateRange(DataValue* input,
                                                          float minInput,
                                                          float maxInput,
                                                          float minOutput,
                                                          float maxOutput)
{
    if (input->is<DataValueNumber>())
    {
        if (minOutput == maxOutput)
        {
            m_output.value(minOutput);
        }
        else
        {
            auto flagsValue =
                static_cast<DataConverterRangeMapperFlags>(flags());
            float value = input->as<DataValueNumber>()->value();

            // Clamp value to min input if flag is on
            if (value < minInput &&
                (flagsValue & DataConverterRangeMapperFlags::ClampLower) ==
                    DataConverterRangeMapperFlags::ClampLower)
            {
                value = minInput;
            }
            // Clamp value to max input if flag is on
            else if (value > maxInput &&
                     (flagsValue & DataConverterRangeMapperFlags::ClampUpper) ==
                         DataConverterRangeMapperFlags::ClampUpper)
            {
                value = maxInput;
            }
            if ((value < minInput || value > maxInput) &&
                (flagsValue & DataConverterRangeMapperFlags::Modulo) ==
                    DataConverterRangeMapperFlags::Modulo)
            {
                // apply modulo to value to wrap whithin the min - max input if
                // it exceeds its range
                value =
                    std::abs(fmodf(value, (maxInput - minInput)) + minInput);
            }
            if (value < minInput)
            {
                m_output.value(minOutput);
            }
            else if (value > maxInput)
            {
                m_output.value(maxOutput);
            }
            else
            {
                float perc = (value - minInput) / (maxInput - minInput);
                // If reverse flag is on, flip the values
                if ((flagsValue & DataConverterRangeMapperFlags::Reverse) ==
                    DataConverterRangeMapperFlags::Reverse)
                {
                    perc = 1 - perc;
                }
                // hold keyframe interpolation
                else if (interpolationType() == 0)
                {
                    perc = perc <= 0 ? 0 : 1;
                }
                // Apply interpolator if exists
                if (m_interpolator != nullptr)
                {
                    perc = m_interpolator->transform(perc);
                }
                m_output.value(perc * maxOutput + (1 - perc) * minOutput);
            }
        }
    }
    else
    {
        m_output.value(DataValueNumber::defaultValue);
    }
    return &m_output;
}

DataValueNumber* DataConverterRangeMapper::calculateReverseRange(
    DataValue* input,
    float minInput,
    float maxInput,
    float minOutput,
    float maxOutnput)
{
    return calculateRange(input, minOutput, maxOutnput, minInput, maxInput);
}

void DataConverterRangeMapper::interpolator(KeyFrameInterpolator* interpolator)
{
    m_interpolator = interpolator;
}

DataValue* DataConverterRangeMapper::convert(DataValue* input,
                                             DataBind* dataBind)
{
    return calculateRange(input,
                          minInput(),
                          maxInput(),
                          minOutput(),
                          maxOutput());
}

DataValue* DataConverterRangeMapper::reverseConvert(DataValue* input,
                                                    DataBind* dataBind)
{
    return calculateReverseRange(input,
                                 minInput(),
                                 maxInput(),
                                 minOutput(),
                                 maxOutput());
}

void DataConverterRangeMapper::minInputChanged() { markConverterDirty(); }

void DataConverterRangeMapper::maxInputChanged() { markConverterDirty(); }

void DataConverterRangeMapper::minOutputChanged() { markConverterDirty(); }

void DataConverterRangeMapper::maxOutputChanged() { markConverterDirty(); }