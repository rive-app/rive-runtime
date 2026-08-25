#ifndef _RIVE_VIEW_MODEL_INSTANCE_SYMBOL_LIST_INDEX_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_SYMBOL_LIST_INDEX_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelInstanceSymbolListIndexBase : public ViewModelInstanceSymbol
{
protected:
    typedef ViewModelInstanceSymbol Super;

public:
    static const uint16_t typeKey = 566;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceSymbolListIndexBase::typeKey:
            case ViewModelInstanceSymbolBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 814;

protected:
    uint32_t m_PropertyValue = 0;

public:
    inline uint32_t propertyValue() const { return m_PropertyValue; }
    void propertyValue(uint32_t value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(propertyValuePropertyKey,
                             &m_PropertyValue,
                             &value);
        m_PropertyValue = value;
        RIVE_EDITOR_CHANGED(propertyValueChanged());
        notifyPropertyChanged(propertyValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceSymbolListIndexBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        RIVE_EDITOR_COPY(object);
        ViewModelInstanceSymbol::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ViewModelInstanceSymbol::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_instance_symbol_list_index_ext.inl"
#endif
};
} // namespace rive

#endif