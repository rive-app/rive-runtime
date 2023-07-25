#ifndef _RIVE_TEXT_MODIFIER_RANGE_BASE_HPP_
#define _RIVE_TEXT_MODIFIER_RANGE_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextModifierRangeBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 158;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextModifierRangeBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t modifyFromPropertyKey = 327;
    static const uint16_t modifyToPropertyKey = 336;
    static const uint16_t strengthPropertyKey = 334;
    static const uint16_t unitsValuePropertyKey = 316;
    static const uint16_t typeValuePropertyKey = 325;
    static const uint16_t modeValuePropertyKey = 326;
    static const uint16_t clampPropertyKey = 333;
    static const uint16_t falloffFromPropertyKey = 317;
    static const uint16_t falloffToPropertyKey = 318;
    static const uint16_t offsetPropertyKey = 319;
    static const uint16_t runIdPropertyKey = 378;

private:
    float m_ModifyFrom = 0.0f;
    float m_ModifyTo = 1.0f;
    float m_Strength = 1.0f;
    uint32_t m_UnitsValue = 0;
    uint32_t m_TypeValue = 0;
    uint32_t m_ModeValue = 0;
    bool m_Clamp = false;
    float m_FalloffFrom = 0.0f;
    float m_FalloffTo = 1.0f;
    float m_Offset = 0.0f;
    uint32_t m_RunId = -1;

public:
    inline float modifyFrom() const { return m_ModifyFrom; }
    void modifyFrom(float value)
    {
        if (m_ModifyFrom == value)
        {
            return;
        }
        m_ModifyFrom = value;
        modifyFromChanged();
    }

    inline float modifyTo() const { return m_ModifyTo; }
    void modifyTo(float value)
    {
        if (m_ModifyTo == value)
        {
            return;
        }
        m_ModifyTo = value;
        modifyToChanged();
    }

    inline float strength() const { return m_Strength; }
    void strength(float value)
    {
        if (m_Strength == value)
        {
            return;
        }
        m_Strength = value;
        strengthChanged();
    }

    inline uint32_t unitsValue() const { return m_UnitsValue; }
    void unitsValue(uint32_t value)
    {
        if (m_UnitsValue == value)
        {
            return;
        }
        m_UnitsValue = value;
        unitsValueChanged();
    }

    inline uint32_t typeValue() const { return m_TypeValue; }
    void typeValue(uint32_t value)
    {
        if (m_TypeValue == value)
        {
            return;
        }
        m_TypeValue = value;
        typeValueChanged();
    }

    inline uint32_t modeValue() const { return m_ModeValue; }
    void modeValue(uint32_t value)
    {
        if (m_ModeValue == value)
        {
            return;
        }
        m_ModeValue = value;
        modeValueChanged();
    }

    inline bool clamp() const { return m_Clamp; }
    void clamp(bool value)
    {
        if (m_Clamp == value)
        {
            return;
        }
        m_Clamp = value;
        clampChanged();
    }

    inline float falloffFrom() const { return m_FalloffFrom; }
    void falloffFrom(float value)
    {
        if (m_FalloffFrom == value)
        {
            return;
        }
        m_FalloffFrom = value;
        falloffFromChanged();
    }

    inline float falloffTo() const { return m_FalloffTo; }
    void falloffTo(float value)
    {
        if (m_FalloffTo == value)
        {
            return;
        }
        m_FalloffTo = value;
        falloffToChanged();
    }

    inline float offset() const { return m_Offset; }
    void offset(float value)
    {
        if (m_Offset == value)
        {
            return;
        }
        m_Offset = value;
        offsetChanged();
    }

    inline uint32_t runId() const { return m_RunId; }
    void runId(uint32_t value)
    {
        if (m_RunId == value)
        {
            return;
        }
        m_RunId = value;
        runIdChanged();
    }

    Core* clone() const override;
    void copy(const TextModifierRangeBase& object)
    {
        m_ModifyFrom = object.m_ModifyFrom;
        m_ModifyTo = object.m_ModifyTo;
        m_Strength = object.m_Strength;
        m_UnitsValue = object.m_UnitsValue;
        m_TypeValue = object.m_TypeValue;
        m_ModeValue = object.m_ModeValue;
        m_Clamp = object.m_Clamp;
        m_FalloffFrom = object.m_FalloffFrom;
        m_FalloffTo = object.m_FalloffTo;
        m_Offset = object.m_Offset;
        m_RunId = object.m_RunId;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case modifyFromPropertyKey:
                m_ModifyFrom = CoreDoubleType::deserialize(reader);
                return true;
            case modifyToPropertyKey:
                m_ModifyTo = CoreDoubleType::deserialize(reader);
                return true;
            case strengthPropertyKey:
                m_Strength = CoreDoubleType::deserialize(reader);
                return true;
            case unitsValuePropertyKey:
                m_UnitsValue = CoreUintType::deserialize(reader);
                return true;
            case typeValuePropertyKey:
                m_TypeValue = CoreUintType::deserialize(reader);
                return true;
            case modeValuePropertyKey:
                m_ModeValue = CoreUintType::deserialize(reader);
                return true;
            case clampPropertyKey:
                m_Clamp = CoreBoolType::deserialize(reader);
                return true;
            case falloffFromPropertyKey:
                m_FalloffFrom = CoreDoubleType::deserialize(reader);
                return true;
            case falloffToPropertyKey:
                m_FalloffTo = CoreDoubleType::deserialize(reader);
                return true;
            case offsetPropertyKey:
                m_Offset = CoreDoubleType::deserialize(reader);
                return true;
            case runIdPropertyKey:
                m_RunId = CoreUintType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void modifyFromChanged() {}
    virtual void modifyToChanged() {}
    virtual void strengthChanged() {}
    virtual void unitsValueChanged() {}
    virtual void typeValueChanged() {}
    virtual void modeValueChanged() {}
    virtual void clampChanged() {}
    virtual void falloffFromChanged() {}
    virtual void falloffToChanged() {}
    virtual void offsetChanged() {}
    virtual void runIdChanged() {}
};
} // namespace rive

#endif