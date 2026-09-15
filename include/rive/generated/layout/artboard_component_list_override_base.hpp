#ifndef _RIVE_ARTBOARD_COMPONENT_LIST_OVERRIDE_BASE_HPP_
#define _RIVE_ARTBOARD_COMPONENT_LIST_OVERRIDE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class ArtboardComponentListOverrideBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 606;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ArtboardComponentListOverrideBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t artboardIdPropertyKey = 858;
    static const uint16_t instanceWidthPropertyKey = 859;
    static const uint16_t instanceHeightPropertyKey = 860;
    static const uint16_t instanceWidthUnitsValuePropertyKey = 856;
    static const uint16_t instanceHeightUnitsValuePropertyKey = 861;
    static const uint16_t instanceWidthScaleTypePropertyKey = 862;
    static const uint16_t instanceHeightScaleTypePropertyKey = 863;

protected:
    Id m_ArtboardId = kEmptyId;
    float m_InstanceWidth = -1.0f;
    float m_InstanceHeight = -1.0f;
    uint32_t m_InstanceWidthUnitsValue = 1;
    uint32_t m_InstanceHeightUnitsValue = 1;
    uint32_t m_InstanceWidthScaleType = 0;
    uint32_t m_InstanceHeightScaleType = 0;

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

    inline float instanceWidth() const { return m_InstanceWidth; }
    void instanceWidth(float value)
    {
        if (m_InstanceWidth == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(instanceWidthPropertyKey,
                             &m_InstanceWidth,
                             &value);
        m_InstanceWidth = value;
        RIVE_EDITOR_CHANGED(instanceWidthChanged());
        notifyPropertyChanged(instanceWidthPropertyKey);
    }

    inline float instanceHeight() const { return m_InstanceHeight; }
    void instanceHeight(float value)
    {
        if (m_InstanceHeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(instanceHeightPropertyKey,
                             &m_InstanceHeight,
                             &value);
        m_InstanceHeight = value;
        RIVE_EDITOR_CHANGED(instanceHeightChanged());
        notifyPropertyChanged(instanceHeightPropertyKey);
    }

    inline uint32_t instanceWidthUnitsValue() const
    {
        return m_InstanceWidthUnitsValue;
    }
    void instanceWidthUnitsValue(uint32_t value)
    {
        if (m_InstanceWidthUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(instanceWidthUnitsValuePropertyKey,
                             &m_InstanceWidthUnitsValue,
                             &value);
        m_InstanceWidthUnitsValue = value;
        RIVE_EDITOR_CHANGED(instanceWidthUnitsValueChanged());
        notifyPropertyChanged(instanceWidthUnitsValuePropertyKey);
    }

    inline uint32_t instanceHeightUnitsValue() const
    {
        return m_InstanceHeightUnitsValue;
    }
    void instanceHeightUnitsValue(uint32_t value)
    {
        if (m_InstanceHeightUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(instanceHeightUnitsValuePropertyKey,
                             &m_InstanceHeightUnitsValue,
                             &value);
        m_InstanceHeightUnitsValue = value;
        RIVE_EDITOR_CHANGED(instanceHeightUnitsValueChanged());
        notifyPropertyChanged(instanceHeightUnitsValuePropertyKey);
    }

    inline uint32_t instanceWidthScaleType() const
    {
        return m_InstanceWidthScaleType;
    }
    void instanceWidthScaleType(uint32_t value)
    {
        if (m_InstanceWidthScaleType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(instanceWidthScaleTypePropertyKey,
                             &m_InstanceWidthScaleType,
                             &value);
        m_InstanceWidthScaleType = value;
        RIVE_EDITOR_CHANGED(instanceWidthScaleTypeChanged());
        notifyPropertyChanged(instanceWidthScaleTypePropertyKey);
    }

    inline uint32_t instanceHeightScaleType() const
    {
        return m_InstanceHeightScaleType;
    }
    void instanceHeightScaleType(uint32_t value)
    {
        if (m_InstanceHeightScaleType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(instanceHeightScaleTypePropertyKey,
                             &m_InstanceHeightScaleType,
                             &value);
        m_InstanceHeightScaleType = value;
        RIVE_EDITOR_CHANGED(instanceHeightScaleTypeChanged());
        notifyPropertyChanged(instanceHeightScaleTypePropertyKey);
    }

    Core* clone() const override;
    void copy(const ArtboardComponentListOverrideBase& object)
    {
        m_ArtboardId = object.m_ArtboardId;
        m_InstanceWidth = object.m_InstanceWidth;
        m_InstanceHeight = object.m_InstanceHeight;
        m_InstanceWidthUnitsValue = object.m_InstanceWidthUnitsValue;
        m_InstanceHeightUnitsValue = object.m_InstanceHeightUnitsValue;
        m_InstanceWidthScaleType = object.m_InstanceWidthScaleType;
        m_InstanceHeightScaleType = object.m_InstanceHeightScaleType;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case artboardIdPropertyKey:
                m_ArtboardId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case instanceWidthPropertyKey:
                m_InstanceWidth = CoreDoubleType::deserialize(reader);
                return true;
            case instanceHeightPropertyKey:
                m_InstanceHeight = CoreDoubleType::deserialize(reader);
                return true;
            case instanceWidthUnitsValuePropertyKey:
                m_InstanceWidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case instanceHeightUnitsValuePropertyKey:
                m_InstanceHeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case instanceWidthScaleTypePropertyKey:
                m_InstanceWidthScaleType = CoreUintType::deserialize(reader);
                return true;
            case instanceHeightScaleTypePropertyKey:
                m_InstanceHeightScaleType = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void artboardIdChanged() {}
    virtual void instanceWidthChanged() {}
    virtual void instanceHeightChanged() {}
    virtual void instanceWidthUnitsValueChanged() {}
    virtual void instanceHeightUnitsValueChanged() {}
    virtual void instanceWidthScaleTypeChanged() {}
    virtual void instanceHeightScaleTypeChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/artboard_component_list_override_ext.inl"
#endif
};
} // namespace rive

#endif