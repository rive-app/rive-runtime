#ifndef _RIVE_ARTBOARD_BASE_HPP_
#define _RIVE_ARTBOARD_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/layout_component.hpp"
namespace rive
{
class ArtboardBase : public LayoutComponent
{
protected:
    typedef LayoutComponent Super;

public:
    static const uint16_t typeKey = 1;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;
    uint32_t m_DefaultStateMachineId = -1;
    uint32_t m_ViewModelId = -1;

public:
    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        m_OriginX = value;
        originXChanged();
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        m_OriginY = value;
        originYChanged();
    }

    inline uint32_t defaultStateMachineId() const { return m_DefaultStateMachineId; }
    void defaultStateMachineId(uint32_t value)
    {
        if (m_DefaultStateMachineId == value)
        {
            return;
        }
        m_DefaultStateMachineId = value;
        defaultStateMachineIdChanged();
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
    void copy(const ArtboardBase& object)
    {
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_DefaultStateMachineId = object.m_DefaultStateMachineId;
        m_ViewModelId = object.m_ViewModelId;
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
                m_DefaultStateMachineId = CoreUintType::deserialize(reader);
                return true;
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreUintType::deserialize(reader);
                return true;
        }
        return LayoutComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void defaultStateMachineIdChanged() {}
    virtual void viewModelIdChanged() {}
};
} // namespace rive

#endif