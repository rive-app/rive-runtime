#ifndef _RIVE_ARTBOARD_BASE_HPP_
#define _RIVE_ARTBOARD_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/layout_component.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ArtboardBase : public LayoutComponent
{
protected:
    typedef LayoutComponent Super;

public:
    static const uint16_t typeKey = 1;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ArtboardBase::typeKey:
            case LayoutComponentBase::typeKey:
            case DrawableBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t originXPropertyKey = 11;
    static const uint16_t originYPropertyKey = 12;
    static const uint16_t defaultStateMachineIdPropertyKey = 236;
    static const uint16_t viewModelIdPropertyKey = 583;

protected:
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;
    Id m_DefaultStateMachineId = kEmptyId;
    Id m_ViewModelId = kEmptyId;

public:
    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originXPropertyKey, &m_OriginX, &value);
        m_OriginX = value;
        RIVE_EDITOR_CHANGED(originXChanged());
        notifyPropertyChanged(originXPropertyKey);
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originYPropertyKey, &m_OriginY, &value);
        m_OriginY = value;
        RIVE_EDITOR_CHANGED(originYChanged());
        notifyPropertyChanged(originYPropertyKey);
    }

    inline Id defaultStateMachineId() const { return m_DefaultStateMachineId; }
    void defaultStateMachineId(Id value)
    {
        if (m_DefaultStateMachineId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(defaultStateMachineIdPropertyKey,
                             &m_DefaultStateMachineId,
                             &value);
        m_DefaultStateMachineId = value;
        RIVE_EDITOR_CHANGED(defaultStateMachineIdChanged());
        notifyPropertyChanged(defaultStateMachineIdPropertyKey);
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
    void copy(const ArtboardBase& object)
    {
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_DefaultStateMachineId = object.m_DefaultStateMachineId;
        m_ViewModelId = object.m_ViewModelId;
        RIVE_EDITOR_COPY(object);
        LayoutComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
            case defaultStateMachineIdPropertyKey:
                m_DefaultStateMachineId =
                    CoreIdType::runtimeDeserialize(reader);
                return true;
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return LayoutComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void defaultStateMachineIdChanged() {}
    virtual void viewModelIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/artboard_ext.inl"
#endif
};
} // namespace rive

#endif