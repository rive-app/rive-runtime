#ifndef _RIVE_LISTENER_VIEW_MODEL_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_VIEW_MODEL_CHANGE_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ListenerViewModelChangeBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 487;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerViewModelChangeBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inputValuePropertyKey = 453;
    static const uint16_t inputValueIndexPropertyKey = 454;

protected:
    uint8_t m_InputValue = 0;
    uint8_t m_InputValueIndex = 0;

public:
    inline uint8_t inputValue() const { return m_InputValue; }
    void inputValue(uint8_t value)
    {
        if (m_InputValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(inputValuePropertyKey, &m_InputValue, &value);
        m_InputValue = value;
        RIVE_EDITOR_CHANGED(inputValueChanged());
        notifyPropertyChanged(inputValuePropertyKey);
    }

    inline uint8_t inputValueIndex() const { return m_InputValueIndex; }
    void inputValueIndex(uint8_t value)
    {
        if (m_InputValueIndex == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(inputValueIndexPropertyKey,
                             &m_InputValueIndex,
                             &value);
        m_InputValueIndex = value;
        RIVE_EDITOR_CHANGED(inputValueIndexChanged());
        notifyPropertyChanged(inputValueIndexPropertyKey);
    }

    Core* clone() const override;
    void copy(const ListenerViewModelChangeBase& object)
    {
        m_InputValue = object.m_InputValue;
        m_InputValueIndex = object.m_InputValueIndex;
        RIVE_EDITOR_COPY(object);
        ListenerAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputValuePropertyKey:
                m_InputValue = CoreUintType::deserialize(reader);
                return true;
            case inputValueIndexPropertyKey:
                m_InputValueIndex = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ListenerAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputValueChanged() {}
    virtual void inputValueIndexChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_viewmodel_change_ext.inl"
#endif
};
} // namespace rive

#endif