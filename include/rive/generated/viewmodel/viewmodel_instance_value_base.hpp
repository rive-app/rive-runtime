#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ViewModelInstanceValueBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 428;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceValueBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelPropertyIdPropertyKey = 554;

private:
    uint32_t m_ViewModelPropertyId = 0;

public:
    inline uint32_t viewModelPropertyId() const { return m_ViewModelPropertyId; }
    void viewModelPropertyId(uint32_t value)
    {
        if (m_ViewModelPropertyId == value)
        {
            return;
        }
        m_ViewModelPropertyId = value;
        viewModelPropertyIdChanged();
    }

    void copy(const ViewModelInstanceValueBase& object)
    {
        m_ViewModelPropertyId = object.m_ViewModelPropertyId;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelPropertyIdPropertyKey:
                m_ViewModelPropertyId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void viewModelPropertyIdChanged() {}
};
} // namespace rive

#endif