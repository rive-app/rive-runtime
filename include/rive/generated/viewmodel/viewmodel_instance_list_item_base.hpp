#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ViewModelInstanceListItemBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 427;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceListItemBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelIdPropertyKey = 549;
    static const uint16_t viewModelInstanceIdPropertyKey = 550;

protected:
    uint32_t m_ViewModelId = -1;
    uint32_t m_ViewModelInstanceId = -1;

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

    inline uint32_t viewModelInstanceId() const
    {
        return m_ViewModelInstanceId;
    }
    void viewModelInstanceId(uint32_t value)
    {
        if (m_ViewModelInstanceId == value)
        {
            return;
        }
        m_ViewModelInstanceId = value;
        viewModelInstanceIdChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceListItemBase& object)
    {
        m_ViewModelId = object.m_ViewModelId;
        m_ViewModelInstanceId = object.m_ViewModelInstanceId;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreUintType::deserialize(reader);
                return true;
            case viewModelInstanceIdPropertyKey:
                m_ViewModelInstanceId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void viewModelIdChanged() {}
    virtual void viewModelInstanceIdChanged() {}
};
} // namespace rive

#endif