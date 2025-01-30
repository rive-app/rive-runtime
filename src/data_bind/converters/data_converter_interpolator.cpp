#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"

using namespace rive;

void DataConverterInterpolator::copy(
    const DataConverterInterpolatorBase& object)
{
    interpolator(object.as<DataConverterInterpolator>()->interpolator());
    DataConverterInterpolatorBase::copy(object);
}

InterpolatorAnimationData* DataConverterInterpolator::currentAnimationData()
{
    return m_isSmoothingAnimation ? &m_animationDataB : &m_animationDataA;
}

void DataConverterInterpolator::interpolator(KeyFrameInterpolator* interpolator)
{
    m_interpolator = interpolator;
}

void DataConverterInterpolator::advanceAnimationData(float elapsedTime)
{
    auto animationData = currentAnimationData();
    if (m_isSmoothingAnimation)
    {
        float f = std::fmin(1.0f,
                            duration() > 0
                                ? m_animationDataA.elapsedSeconds / duration()
                                : 1.0f);
        if (m_interpolator != nullptr)
        {
            f = m_interpolator->transform(f);
        }
        m_animationDataB.from = m_animationDataA.interpolate(f);
        if (f == 1)
        {
            m_animationDataA.copy(m_animationDataB);
            m_isSmoothingAnimation = false;
        }
        else
        {
            m_animationDataA.elapsedSeconds += elapsedTime;
        }
    }
    if (animationData->elapsedSeconds >= duration())
    {
        m_currentValue = animationData->to;

        if (m_isSmoothingAnimation)
        {
            m_isSmoothingAnimation = false;
            m_animationDataA.copy(m_animationDataB);
            m_animationDataA.elapsedSeconds = m_animationDataB.elapsedSeconds =
                0;
        }
        else
        {
            m_animationDataA.elapsedSeconds = 0;
        }
        return;
    }
    float f = std::fmin(
        1.0f,
        duration() > 0 ? animationData->elapsedSeconds / duration() : 1.0f);
    if (m_interpolator != nullptr)
    {
        f = m_interpolator->transform(f);
    }
    auto current = animationData->interpolate(f);
    if (m_currentValue != current)
    {
        m_currentValue = current;
    }

    animationData->elapsedSeconds += elapsedTime;
}

bool DataConverterInterpolator::advance(float elapsedTime)
{
    auto animationData = currentAnimationData();
    if (animationData->to == m_currentValue)
    {
        return false;
    }
    advanceAnimationData(elapsedTime);
    if (animationData->elapsedSeconds < duration())
    {
        addDirt(ComponentDirt::Dependents);
        return true;
    }
    return false;
}

DataValue* DataConverterInterpolator::convert(DataValue* input,
                                              DataBind* dataBind)
{
    if (input->is<DataValueNumber>())
    {
        auto animationData = currentAnimationData();
        auto numberInput = input->as<DataValueNumber>();
        if (m_isFirstRun)
        {
            animationData->from = numberInput->value();
            animationData->to = numberInput->value();
            m_currentValue = numberInput->value();
            m_isFirstRun = false;
        }
        else
        {
            if (animationData->to != numberInput->value())
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
                animationData->from = m_currentValue;
                animationData->to = numberInput->value();
                animationData->elapsedSeconds = 0;
            }
        }
        m_output.value(m_currentValue);
    }
    return &m_output;
}

DataValue* DataConverterInterpolator::reverseConvert(DataValue* input,
                                                     DataBind* dataBind)
{
    return convert(input, dataBind);
}

void InterpolatorAnimationData::copy(const InterpolatorAnimationData& source)
{
    from = source.from;
    to = source.to;
    elapsedSeconds = source.elapsedSeconds;
}
