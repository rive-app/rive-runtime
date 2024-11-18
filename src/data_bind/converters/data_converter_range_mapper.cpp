#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/animation/data_converter_range_mapper_flags.hpp"

using namespace rive;

DataValueNumber* DataConverterRangeMapper::calculateRange(DataValue* input,
                                                          float minInput,
                                                          float maxInput,
                                                          float minOutput,
                                                          float maxOutput)
{
    auto output = new DataValueNumber();
    if (input->is<DataValueNumber>())
    {
        if (minOutput == maxOutput)
        {
            output->value(minOutput);
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
                output->value(minOutput);
            }
            else if (value > maxInput)
            {
                output->value(maxOutput);
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
                output->value(perc * maxOutput + (1 - perc) * minOutput);
            }
        }
    }
    return output;
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