#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ViewModelInstanceListItemBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 427;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

    static const uint16_t useLinkedArtboardPropertyKey = 547;
    static const uint16_t viewModelIdPropertyKey = 549;
    static const uint16_t viewModelInstanceIdPropertyKey = 550;
    static const uint16_t artboardIdPropertyKey = 551;

private:
    bool m_UseLinkedArtboard = true;
    uint32_t m_ViewModelId = -1;
    uint32_t m_ViewModelInstanceId = -1;
    uint32_t m_ArtboardId = -1;

public:
    inline bool useLinkedArtboard() const { return m_UseLinkedArtboard; }
    void useLinkedArtboard(bool value)
    {
        if (m_UseLinkedArtboard == value)
        {
            return;
        }
        m_UseLinkedArtboard = value;
        useLinkedArtboardChanged();
    }

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

    inline uint32_t viewModelInstanceId() const { return m_ViewModelInstanceId; }
    void viewModelInstanceId(uint32_t value)
    {
        if (m_ViewModelInstanceId == value)
        {
            return;
        }
        m_ViewModelInstanceId = value;
        viewModelInstanceIdChanged();
    }

    inline uint32_t artboardId() const { return m_ArtboardId; }
    void artboardId(uint32_t value)
    {
        if (m_ArtboardId == value)
        {
            return;
        }
        m_ArtboardId = value;
        artboardIdChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceListItemBase& object)
    {
        m_UseLinkedArtboard = object.m_UseLinkedArtboard;
        m_ViewModelId = object.m_ViewModelId;
        m_ViewModelInstanceId = object.m_ViewModelInstanceId;
        m_ArtboardId = object.m_ArtboardId;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case useLinkedArtboardPropertyKey:
                m_UseLinkedArtboard = CoreBoolType::deserialize(reader);
                return true;
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreUintType::deserialize(reader);
                return true;
            case viewModelInstanceIdPropertyKey:
                m_ViewModelInstanceId = CoreUintType::deserialize(reader);
                return true;
            case artboardIdPropertyKey:
                m_ArtboardId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void useLinkedArtboardChanged() {}
    virtual void viewModelIdChanged() {}
    virtual void viewModelInstanceIdChanged() {}
    virtual void artboardIdChanged() {}
};
} // namespace rive

#endif