#ifndef _RIVE_LISTENER_INPUT_TYPE_POINTER_BUTTON_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_POINTER_BUTTON_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ListenerInputTypePointerButtonBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 155;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypePointerButtonBase::typeKey:
            case ListenerInputTypeBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t pointerButtonValuePropertyKey = 468;

protected:
    uint8_t m_PointerButtonValue = 0;

public:
    inline uint8_t pointerButtonValue() const { return m_PointerButtonValue; }
    void pointerButtonValue(uint8_t value)
    {
        if (m_PointerButtonValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(pointerButtonValuePropertyKey,
                             &m_PointerButtonValue,
                             &value);
        m_PointerButtonValue = value;
        RIVE_EDITOR_CHANGED(pointerButtonValueChanged());
        notifyPropertyChanged(pointerButtonValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const ListenerInputTypePointerButtonBase& object)
    {
        m_PointerButtonValue = object.m_PointerButtonValue;
        ListenerInputType::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case pointerButtonValuePropertyKey:
                m_PointerButtonValue = CoreUintType::deserialize(reader);
                return true;
        }
        return ListenerInputType::deserialize(propertyKey, reader);
    }

protected:
    virtual void pointerButtonValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_types/listener_input_type_pointer_button_ext.inl"
#endif
};
} // namespace rive

#endif