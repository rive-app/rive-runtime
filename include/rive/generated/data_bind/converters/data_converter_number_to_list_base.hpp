#ifndef _RIVE_DATA_CONVERTER_NUMBER_TO_LIST_BASE_HPP_
#define _RIVE_DATA_CONVERTER_NUMBER_TO_LIST_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterNumberToListBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 568;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterNumberToListBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelIdPropertyKey = 816;

protected:
    Id m_ViewModelId = kEmptyId;

public:
    inline Id viewModelId() const { return m_ViewModelId; }
    void viewModelId(Id value)
    {
        if (m_ViewModelId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelIdPropertyKey, &m_ViewModelId, &value);
        m_ViewModelId = value;
        RIVE_EDITOR_CHANGED(viewModelIdChanged());
        notifyPropertyChanged(viewModelIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const DataConverterNumberToListBase& object)
    {
        m_ViewModelId = object.m_ViewModelId;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_number_to_list_ext.inl"
#endif
};
} // namespace rive

#endif