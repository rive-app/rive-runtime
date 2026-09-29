#include "rive/animation/blend_accumulator.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

static uint32_t slotOf(const Core* object, int propertyKey)
{
    // Fibonacci hashing of the pointer, with the key folded in.
    uint64_t h = ((uint64_t)(uintptr_t)object ^
                  ((uint64_t)(uint32_t)propertyKey << 48)) *
                 UINT64_C(0x9E3779B97F4A7C15);
    return (uint32_t)(h >> 32);
}

uint32_t BlendAccumulator::find(Core* object, int propertyKey) const
{
    if (m_index.empty())
    {
        return 0;
    }
    const uint32_t mask = (uint32_t)m_index.size() - 1;
    for (uint32_t i = slotOf(object, propertyKey) & mask;; i = (i + 1) & mask)
    {
        uint32_t entry = m_index[i];
        if (entry == 0)
        {
            return 0;
        }
        const Value& value = m_values[entry - 1];
        if (value.object == object && value.propertyKey == propertyKey)
        {
            return entry;
        }
    }
}

void BlendAccumulator::index(uint32_t valueIndex)
{
    const uint32_t mask = (uint32_t)m_index.size() - 1;
    const Value& value = m_values[valueIndex];
    uint32_t i = slotOf(value.object, value.propertyKey) & mask;
    while (m_index[i] != 0)
    {
        i = (i + 1) & mask;
    }
    m_index[i] = valueIndex + 1;
}

BlendAccumulator::Value& BlendAccumulator::value(Core* object,
                                                 int propertyKey,
                                                 bool isColor,
                                                 bool seeding)
{
    uint32_t entry = find(object, propertyKey);
    if (entry == 0)
    {
        m_values.push_back({object, propertyKey, isColor, 0, 0.0f, 0});
        if (m_values.size() * 2 > m_index.size())
        {
            m_index.assign(m_index.empty() ? 16 : m_index.size() * 2, 0);
            for (uint32_t i = 0; i < (uint32_t)m_values.size(); i++)
            {
                index(i);
            }
        }
        else
        {
            index((uint32_t)m_values.size() - 1);
        }
        entry = (uint32_t)m_values.size();
    }
    Value& value = m_values[entry - 1];
    if (value.frame != m_frame)
    {
        value.frame = m_frame;
        m_written.push_back(entry - 1);
        if (!seeding)
        {
            if (isColor)
            {
                value.colorValue = CoreRegistry::getColor(object, propertyKey);
            }
            else
            {
                value.doubleValue =
                    CoreRegistry::getDouble(object, propertyKey);
            }
        }
    }
    return value;
}

void BlendAccumulator::seedDouble(Core* object, int propertyKey, float value)
{
    this->value(object, propertyKey, false, true).doubleValue = value;
}

void BlendAccumulator::seedColor(Core* object, int propertyKey, ColorInt value)
{
    this->value(object, propertyKey, true, true).colorValue = value;
}

void BlendAccumulator::applyDouble(Core* object,
                                   int propertyKey,
                                   float mix,
                                   float value)
{
    Value& current = this->value(object, propertyKey, false, false);
    if (mix == 1.0f)
    {
        current.doubleValue = value;
    }
    else
    {
        float mixi = 1.0f - mix;
        current.doubleValue = current.doubleValue * mixi + value * mix;
    }
}

void BlendAccumulator::applyColor(Core* object,
                                  int propertyKey,
                                  float mix,
                                  ColorInt value)
{
    Value& current = this->value(object, propertyKey, true, false);
    current.colorValue =
        mix == 1.0f ? value : colorLerp(current.colorValue, value, mix);
}

void BlendAccumulator::flush()
{
    for (uint32_t written : m_written)
    {
        const Value& value = m_values[written];
        if (value.isColor)
        {
            CoreRegistry::setColor(value.object,
                                   value.propertyKey,
                                   value.colorValue);
        }
        else
        {
            CoreRegistry::setDouble(value.object,
                                    value.propertyKey,
                                    value.doubleValue);
        }
    }
    m_written.clear();
    // Wrapping back to a frame some value still carries would skip loading
    // it, so start the count over with every value marked unwritten.
    if (++m_frame == 0)
    {
        for (Value& value : m_values)
        {
            value.frame = 0;
        }
        m_frame = 1;
    }
}
