#ifndef _RIVE_LAYER_STATE_BASE_HPP_
#define _RIVE_LAYER_STATE_BASE_HPP_
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class LayerStateBase : public StateMachineLayerComponent
{
protected:
    typedef StateMachineLayerComponent Super;

public:
    static const uint16_t typeKey = 60;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayerStateBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t flagsPropertyKey = 536;

protected:
    uint32_t m_Flags = 0;

public:
    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flagsPropertyKey, &m_Flags, &value);
        m_Flags = value;
        RIVE_EDITOR_CHANGED(flagsChanged());
        notifyPropertyChanged(flagsPropertyKey);
    }

    void copy(const LayerStateBase& object)
    {
        m_Flags = object.m_Flags;
        RIVE_EDITOR_COPY(object);
        StateMachineLayerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return StateMachineLayerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void flagsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/layer_state_ext.inl"
#endif
};
} // namespace rive

#endif