#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/scripted/scripted_interpolator.hpp"

using namespace rive;

void InterpolatorAnimationData::copy(const InterpolatorAnimationData& source)
{
  source.from->copyValue(from);
  source.to->copyValue(to);
  elapsedSeconds = source.elapsedSeconds;
}

void InterpolatorAdvancer::dispose()
{
  m_animationDataA.dispose();
  m_animationDataB.dispose();
  if (m_currentValue != nullptr)
  {
    delete m_currentValue;
    m_currentValue = nullptr;
  }
}

InterpolatorAnimationData* InterpolatorAdvancer::currentAnimationData()
{
  return m_isSmoothingAnimation ? &m_animationDataB : &m_animationDataA;
}

void InterpolatorAdvancer::advanceAnimationData(float elapsedTime)
{
  auto animationData = currentAnimationData();
  if (m_isSmoothingAnimation)
  {
    float f = std::fmin(
        1.0f,
        m_converter->duration() > 0
            ? m_animationDataA.elapsedSeconds / m_converter->duration()
            : 1.0f);
    if (auto* interpolator = m_converter->activeInterpolator())
    {
      f = interpolator->transform(f);
    }
    m_animationDataA.interpolate(f, m_animationDataB.from);
    if (f == 1)
    {
      m_animationDataA.copy(m_animationDataB);
      m_isSmoothingAnimation = false;
      // B now lives in A, so keep advancing A. Advancing B would leave A one
      // frame behind and repeat this frame's value on the next one.
      animationData = currentAnimationData();
    }
    else
    {
      m_animationDataA.elapsedSeconds += elapsedTime;
    }
  }
  if (animationData->elapsedSeconds >= m_converter->duration())
  {
    animationData->to->copyValue(m_currentValue);

    if (m_isSmoothingAnimation)
    {
      m_isSmoothingAnimation = false;
      m_animationDataA.copy(m_animationDataB);
      m_animationDataA.elapsedSeconds = m_animationDataB.elapsedSeconds = 0;
    }
    else
    {
      m_animationDataA.elapsedSeconds = 0;
    }
    return;
  }
  animationData->elapsedSeconds += elapsedTime;
  float f = std::fmin(
      1.0f,
      m_converter->duration() > 0
          ? animationData->elapsedSeconds / m_converter->duration()
          : 1.0f);
  if (auto* interpolator = m_converter->activeInterpolator())
  {
    f = interpolator->transform(f);
  }
  animationData->interpolate(f, m_currentValue);
}

bool InterpolatorAdvancer::advance(float elapsedTime)
{
  auto animationData = currentAnimationData();
  if (animationData->to->compare(m_currentValue) || elapsedTime == 0)
  {
    return false;
  }
  auto prevTime = animationData->elapsedSeconds;
  advanceAnimationData(elapsedTime);

  if (prevTime < m_converter->duration())
  {
    m_converter->markConverterDirty();
  }
  // advanceAnimationData may have ended the smoothing animation.
  if (currentAnimationData()->elapsedSeconds < m_converter->duration())
  {
    return true;
  }
  return false;
}

DataConverterInterpolator::~DataConverterInterpolator()
{
  if (m_output != nullptr)
  {
    delete m_output;
  }
  m_advancer.dispose();
  delete m_scriptedInterpolator;
}

KeyFrameInterpolator* DataConverterInterpolator::activeInterpolator() const
{
  if (m_scriptedInterpolator != nullptr)
  {
    if (auto* clone = m_scriptedInterpolator->get())
    {
      return clone;
    }
  }
  return m_interpolator;
}

void DataConverterInterpolator::bindFromContext(
    DataContext* dataContext,
    DataBind* dataBind)
{
  Super::bindFromContext(dataContext, dataBind);
  if (m_interpolator == nullptr || !m_interpolator->is<ScriptedInterpolator>())
  {
    return;
  }
  auto context = rcp<DataContext>(safe_ref(dataContext));
  if (m_scriptedInterpolator == nullptr)
  {
    m_scriptedInterpolator = new StatefulScriptedInterpolator(
        m_interpolator->as<ScriptedInterpolator>(),
        context);
  }
  m_scriptedInterpolator->bindFromContext(context);
}

void DataConverterInterpolator::unbind()
{
  Super::unbind();
  if (m_scriptedInterpolator != nullptr)
  {
    m_scriptedInterpolator->unbind();
  }
}

void DataConverterInterpolator::update()
{
  Super::update();
  if (m_scriptedInterpolator != nullptr)
  {
    m_scriptedInterpolator->updateDataBinds();
  }
}

void DataConverterInterpolator::copy(
    const DataConverterInterpolatorBase& object)
{
  interpolator(object.as<DataConverterInterpolator>()->interpolator());
  DataConverterInterpolatorBase::copy(object);
}

void DataConverterInterpolator::interpolator(KeyFrameInterpolator* interpolator)
{
  m_interpolator = interpolator;
}

bool DataConverterInterpolator::advance(float elapsedTime)
{
  // Advance can be called multiple times in a single frame.
  // We want to make sure that two advances with time > 0 have elapsed before
  // considering the state as second frame.
  if (m_advanceCount < 2 && elapsedTime > 0)
  {
    m_advanceCount++;
  }
  if (!m_advancer.initialized())
  {
    return true;
  }
  return m_advancer.advance(elapsedTime);
}

DataValue* DataConverterInterpolator::convert(
    DataValue* input,
    DataBind* dataBind)
{
  if (duration() == 0 && m_advancer.initialized())
  {
    m_advancer.resetToStart(input);
    return input;
  }
  if (!m_advancer.initialized())
  {
    if (input->is<DataValueNumber>())
    {
      startAdvancer<DataValueNumber>();
    }
    else if (input->is<DataValueColor>())
    {
      startAdvancer<DataValueColor>();
    }
    else
    {
      return input;
    }
  }
  if (m_output != nullptr &&
      (input->is<DataValueNumber>() || input->is<DataValueColor>()))
  {
    if (isFirstRun())
    {
      m_advancer.resetValues(input);
    }
    else
    {
      m_advancer.updateValues(input);
    }
    m_advancer.copyCurrentValue(m_output);
    return m_output;
  }
  return input;
}

void DataConverterInterpolator::reset()
{
  m_advanceCount = 0;
  m_advancer.reset();
}

DataValue* DataConverterInterpolator::reverseConvert(
    DataValue* input,
    DataBind* dataBind)
{
  return convert(input, dataBind);
}

void DataConverterInterpolator::durationChanged() { markConverterDirty(); }
