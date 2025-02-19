#ifndef _RIVE_DATA_CONVERTER_INTERPOLATOR_HPP_
#define _RIVE_DATA_CONVERTER_INTERPOLATOR_HPP_
#include "rive/generated/data_bind/converters/data_converter_interpolator_base.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include <stdio.h>
namespace rive
{

struct InterpolatorAnimationData
{
    float elapsedSeconds = 0.0f;
    float from;
    float to;
    float interpolate(float f) const
    {
        float fi = 1.0f - f;
        return to * f + from * fi;
    }
    void copy(const InterpolatorAnimationData& source);
};
class DataConverterInterpolator : public DataConverterInterpolatorBase
{
protected:
    KeyFrameInterpolator* m_interpolator = nullptr;

public:
    void interpolator(KeyFrameInterpolator* interpolator);
    KeyFrameInterpolator* interpolator() const { return m_interpolator; };
    DataType outputType() override { return DataType::number; };
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    bool advance(float elapsedTime) override;
    void copy(const DataConverterInterpolatorBase& object);
    void durationChanged() override;

private:
    DataValueNumber m_output;
    float m_currentValue;
    bool m_isFirstRun = true;

    InterpolatorAnimationData m_animationDataA;
    InterpolatorAnimationData m_animationDataB;
    bool m_isSmoothingAnimation = false;
    InterpolatorAnimationData* currentAnimationData();
    void advanceAnimationData(float elapsedTime);

public:
};
} // namespace rive

#endif