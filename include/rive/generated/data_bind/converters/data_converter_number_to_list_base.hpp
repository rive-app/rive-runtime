#ifndef _RIVE_DATA_CONVERTER_NUMBER_TO_LIST_BASE_HPP_
#define _RIVE_DATA_CONVERTER_NUMBER_TO_LIST_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
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
    uint32_t m_ViewModelId = -1;

public:
    inline uint32_t viewModelId() const { return m_ViewModelId; }
    void viewModelId(uint32_t value)
    {
        if (m_ViewModelId == value)
        {
            return;
        }
        m_ViewModelId = value;
        viewModelIdChanged();
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
                m_ViewModelId = CoreUintType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelIdChanged() {}
};
} // namespace rive

#endif