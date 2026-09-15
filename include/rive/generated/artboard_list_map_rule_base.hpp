#ifndef _RIVE_ARTBOARD_LIST_MAP_RULE_BASE_HPP_
#define _RIVE_ARTBOARD_LIST_MAP_RULE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
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
    Id m_ArtboardId = kEmptyId;
    Id m_ViewModelId = kEmptyId;

public:
    inline Id artboardId() const { return m_ArtboardId; }
    void artboardId(Id value)
    {
        if (m_ArtboardId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(artboardIdPropertyKey, &m_ArtboardId, &value);
        m_ArtboardId = value;
        RIVE_EDITOR_CHANGED(artboardIdChanged());
        notifyPropertyChanged(artboardIdPropertyKey);
    }

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
                m_ArtboardId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void artboardIdChanged() {}
    virtual void viewModelIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/artboard_list_map_rule_ext.inl"
#endif
};
} // namespace rive

#endif