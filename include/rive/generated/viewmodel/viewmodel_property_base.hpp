#ifndef _RIVE_VIEW_MODEL_PROPERTY_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelPropertyBase : public ViewModelComponent
{
protected:
    typedef ViewModelComponent Super;

public:
    static const uint16_t typeKey = 430;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t symbolTypeValuePropertyKey = 875;
    static const uint16_t componentPropsPropertyKey = 957;

protected:
    uint8_t m_SymbolTypeValue = 0;
    uint8_t m_ComponentProps = 0;

public:
    inline uint8_t symbolTypeValue() const { return m_SymbolTypeValue; }
    void symbolTypeValue(uint8_t value)
    {
        if (m_SymbolTypeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(symbolTypeValuePropertyKey,
                             &m_SymbolTypeValue,
                             &value);
        m_SymbolTypeValue = value;
        RIVE_EDITOR_CHANGED(symbolTypeValueChanged());
        notifyPropertyChanged(symbolTypeValuePropertyKey);
    }

    inline uint8_t componentProps() const { return m_ComponentProps; }
    void componentProps(uint8_t value)
    {
        if (m_ComponentProps == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(componentPropsPropertyKey,
                             &m_ComponentProps,
                             &value);
        m_ComponentProps = value;
        RIVE_EDITOR_CHANGED(componentPropsChanged());
        notifyPropertyChanged(componentPropsPropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelPropertyBase& object)
    {
        m_SymbolTypeValue = object.m_SymbolTypeValue;
        m_ComponentProps = object.m_ComponentProps;
        RIVE_EDITOR_COPY(object);
        ViewModelComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case symbolTypeValuePropertyKey:
                m_SymbolTypeValue = CoreUintType::deserialize(reader);
                return true;
            case componentPropsPropertyKey:
                m_ComponentProps = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ViewModelComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void symbolTypeValueChanged() {}
    virtual void componentPropsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_property_ext.inl"
#endif
};
} // namespace rive

#endif