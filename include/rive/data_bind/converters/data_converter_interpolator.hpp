#ifndef _RIVE_DATA_CONVERTER_INTERPOLATOR_HPP_
#define _RIVE_DATA_CONVERTER_INTERPOLATOR_HPP_
#include "rive/generated/data_bind/converters/data_converter_interpolator_base.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include <stdio.h>
namespace rive
{

class DataConverterInterpolator;

class InterpolatorAnimationData
{
public:
    float elapsedSeconds = 0.0f;
    DataValue* from = nullptr;
    DataValue* to = nullptr;
    void interpolate(float f, DataValue* store) const
    {
        from->interpolate(to, store, f);
    }
    void copy(const InterpolatorAnimationData& source);
    template <typename T = DataValue> void initialize()
    {
        from = new T();
        to = new T();
    }
    void dispose()
    {
        elapsedSeconds = 0;
        if (from != nullptr)
        {
            delete from;
            from = nullptr;
            delete to;
            to = nullptr;
        }
    }
};

class InterpolatorAdvancer
{
public:
    template <typename T = DataValue>
    void initialize(DataConverterInterpolator* converter)
    {
        m_initialized = true;
        m_converter = converter;

        m_animationDataA.initialize<T>();
        m_animationDataB.initialize<T>();
        m_currentValue = new T();
    }
    bool initialized() { return m_initialized; }
    void dispose();
    void resetValues(DataValue* input)
    {
        auto animationData = currentAnimationData();
        input->copyValue(animationData->from);
        input->copyValue(animationData->to);
        input->copyValue(m_currentValue);
    }
    void reset()
    {
        dispose();
        m_isSmoothingAnimation = false;
        m_initialized = false;
    }
    void updateValues(DataValue* input)
    {
        auto animationData = currentAnimationData();
        if (!input->compare(animationData->to))
        {
            if (animationData->elapsedSeconds != 0)
            {
                if (m_isSmoothingAnimation)
                {
                    m_animationDataA.copy(m_animationDataB);
                }
                m_isSmoothingAnimation = true;
            }
            else
            {
                m_isSmoothingAnimation = false;
            }
            animationData = currentAnimationData();
            m_currentValue->copyValue(animationData->from);
            input->copyValue(animationData->to);
            animationData->elapsedSeconds = 0;
        }
    }
    void copyCurrentValue(DataValue* output)
    {
        m_currentValue->copyValue(output);
    }
    void advanceAnimationData(float elapsedTime);
    bool advance(float elapsedTime);

private:
    InterpolatorAnimationData m_animationDataA;
    InterpolatorAnimationData m_animationDataB;
    InterpolatorAnimationData* currentAnimationData();
    bool m_isSmoothingAnimation = false;
    bool m_initialized = false;
    DataValue* m_currentValue = nullptr;
    DataConverterInterpolator* m_converter = nullptr;
};

class DataConverterInterpolator : public DataConverterInterpolatorBase
{
protected:
    KeyFrameInterpolator* m_interpolator = nullptr;

public:
    ~DataConverterInterpolator();
    void interpolator(KeyFrameInterpolator* interpolator);
    KeyFrameInterpolator* interpolator() const { return m_interpolator; };
    DataType outputType() override { return DataType::input; };
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    bool advance(float elapsedTime) override;
    void reset() override;
    void copy(const DataConverterInterpolatorBase& object);
    void durationChanged() override;
    template <typename T = DataValue> void startAdvancer()
    {
        if (m_output != nullptr)
        {
            delete m_output;
        }
        m_output = new T();
        m_advancer.initialize<T>(this);
    }

private:
    DataValue* m_output = nullptr;
    uint8_t m_advanceCount = 0;
    bool isFirstRun() { return m_advanceCount < 2; }

    InterpolatorAdvancer m_advancer;

public:
};
} // namespace rive

#endif