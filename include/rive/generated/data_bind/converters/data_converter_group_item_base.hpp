#ifndef _RIVE_DATA_CONVERTER_GROUP_ITEM_BASE_HPP_
#define _RIVE_DATA_CONVERTER_GROUP_ITEM_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class DataConverterGroupItemBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 498;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterGroupItemBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t converterIdPropertyKey = 679;

protected:
    Id m_ConverterId = kEmptyId;

public:
    inline Id converterId() const { return m_ConverterId; }
    void converterId(Id value)
    {
        if (m_ConverterId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(converterIdPropertyKey, &m_ConverterId, &value);
        m_ConverterId = value;
        RIVE_EDITOR_CHANGED(converterIdChanged());
        notifyPropertyChanged(converterIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const DataConverterGroupItemBase& object)
    {
        m_ConverterId = object.m_ConverterId;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case converterIdPropertyKey:
                m_ConverterId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void converterIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_group_item_ext.inl"
#endif
};
} // namespace rive

#endif