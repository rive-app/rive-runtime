#ifndef _RIVE_DATA_CONVERTER_GROUP_ITEM_BASE_HPP_
#define _RIVE_DATA_CONVERTER_GROUP_ITEM_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class DataConverterGroupItemBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 498;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    uint32_t m_ConverterId = -1;

public:
    inline uint32_t converterId() const { return m_ConverterId; }
    void converterId(uint32_t value)
    {
        if (m_ConverterId == value)
        {
            return;
        }
        m_ConverterId = value;
        converterIdChanged();
    }

    Core* clone() const override;
    void copy(const DataConverterGroupItemBase& object) { m_ConverterId = object.m_ConverterId; }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case converterIdPropertyKey:
                m_ConverterId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void converterIdChanged() {}
};
} // namespace rive

#endif