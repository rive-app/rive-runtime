#ifndef _RIVE_ADVANCEABLE_STATE_BASE_HPP_
#define _RIVE_ADVANCEABLE_STATE_BASE_HPP_
#include "rive/animation/layer_state.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class AdvanceableStateBase : public LayerState
{
protected:
    typedef LayerState Super;

public:
    static const uint16_t typeKey = 145;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AdvanceableStateBase::typeKey:
            case LayerStateBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t speedPropertyKey = 292;

private:
    float m_Speed = 1.0f;

public:
    inline float speed() const { return m_Speed; }
    void speed(float value)
    {
        if (m_Speed == value)
        {
            return;
        }
        m_Speed = value;
        speedChanged();
    }

    void copy(const AdvanceableStateBase& object)
    {
        m_Speed = object.m_Speed;
        LayerState::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case speedPropertyKey:
                m_Speed = CoreDoubleType::deserialize(reader);
                return true;
        }
        return LayerState::deserialize(propertyKey, reader);
    }

protected:
    virtual void speedChanged() {}
};
} // namespace rive

#endif