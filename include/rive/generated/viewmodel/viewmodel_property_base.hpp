#ifndef _RIVE_VIEW_MODEL_PROPERTY_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"
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
    uint32_t m_SymbolTypeValue = 0;
    uint32_t m_ComponentProps = 0;

public:
    inline uint32_t symbolTypeValue() const { return m_SymbolTypeValue; }
    void symbolTypeValue(uint32_t value)
    {
        if (m_SymbolTypeValue == value)
        {
            return;
        }
        m_SymbolTypeValue = value;
        symbolTypeValueChanged();
    }

    inline uint32_t componentProps() const { return m_ComponentProps; }
    void componentProps(uint32_t value)
    {
        if (m_ComponentProps == value)
        {
            return;
        }
        m_ComponentProps = value;
        componentPropsChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelPropertyBase& object)
    {
        m_SymbolTypeValue = object.m_SymbolTypeValue;
        m_ComponentProps = object.m_ComponentProps;
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
        return ViewModelComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void symbolTypeValueChanged() {}
    virtual void componentPropsChanged() {}
};
} // namespace rive

#endif