#ifndef _RIVE_ARTBOARD_LIST_MAP_RULE_BASE_HPP_
#define _RIVE_ARTBOARD_LIST_MAP_RULE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ArtboardListMapRuleBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 648;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ArtboardListMapRuleBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t artboardIdPropertyKey = 934;
    static const uint16_t viewModelIdPropertyKey = 935;

protected:
    uint32_t m_ArtboardId = -1;
    uint32_t m_ViewModelId = -1;

public:
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
    void copy(const ArtboardListMapRuleBase& object)
    {
        m_ArtboardId = object.m_ArtboardId;
        m_ViewModelId = object.m_ViewModelId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case artboardIdPropertyKey:
                m_ArtboardId = CoreUintType::deserialize(reader);
                return true;
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void artboardIdChanged() {}
    virtual void viewModelIdChanged() {}
};
} // namespace rive

#endif