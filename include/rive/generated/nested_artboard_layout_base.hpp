#ifndef _RIVE_NESTED_ARTBOARD_LAYOUT_BASE_HPP_
#define _RIVE_NESTED_ARTBOARD_LAYOUT_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/nested_artboard.hpp"
namespace rive
{
class NestedArtboardLayoutBase : public NestedArtboard
{
protected:
    typedef NestedArtboard Super;

public:
    static const uint16_t typeKey = 452;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedArtboardLayoutBase::typeKey:
            case NestedArtboardBase::typeKey:
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

    static const uint16_t instanceWidthPropertyKey = 663;
    static const uint16_t instanceHeightPropertyKey = 664;
    static const uint16_t instanceWidthUnitsValuePropertyKey = 665;
    static const uint16_t instanceHeightUnitsValuePropertyKey = 666;
    static const uint16_t instanceWidthScaleTypePropertyKey = 667;
    static const uint16_t instanceHeightScaleTypePropertyKey = 668;

private:
    float m_InstanceWidth = -1.0f;
    float m_InstanceHeight = -1.0f;
    uint32_t m_InstanceWidthUnitsValue = 1;
    uint32_t m_InstanceHeightUnitsValue = 1;
    uint32_t m_InstanceWidthScaleType = 0;
    uint32_t m_InstanceHeightScaleType = 0;

public:
    inline float instanceWidth() const { return m_InstanceWidth; }
    void instanceWidth(float value)
    {
        if (m_InstanceWidth == value)
        {
            return;
        }
        m_InstanceWidth = value;
        instanceWidthChanged();
    }

    inline float instanceHeight() const { return m_InstanceHeight; }
    void instanceHeight(float value)
    {
        if (m_InstanceHeight == value)
        {
            return;
        }
        m_InstanceHeight = value;
        instanceHeightChanged();
    }

    inline uint32_t instanceWidthUnitsValue() const { return m_InstanceWidthUnitsValue; }
    void instanceWidthUnitsValue(uint32_t value)
    {
        if (m_InstanceWidthUnitsValue == value)
        {
            return;
        }
        m_InstanceWidthUnitsValue = value;
        instanceWidthUnitsValueChanged();
    }

    inline uint32_t instanceHeightUnitsValue() const { return m_InstanceHeightUnitsValue; }
    void instanceHeightUnitsValue(uint32_t value)
    {
        if (m_InstanceHeightUnitsValue == value)
        {
            return;
        }
        m_InstanceHeightUnitsValue = value;
        instanceHeightUnitsValueChanged();
    }

    inline uint32_t instanceWidthScaleType() const { return m_InstanceWidthScaleType; }
    void instanceWidthScaleType(uint32_t value)
    {
        if (m_InstanceWidthScaleType == value)
        {
            return;
        }
        m_InstanceWidthScaleType = value;
        instanceWidthScaleTypeChanged();
    }

    inline uint32_t instanceHeightScaleType() const { return m_InstanceHeightScaleType; }
    void instanceHeightScaleType(uint32_t value)
    {
        if (m_InstanceHeightScaleType == value)
        {
            return;
        }
        m_InstanceHeightScaleType = value;
        instanceHeightScaleTypeChanged();
    }

    Core* clone() const override;
    void copy(const NestedArtboardLayoutBase& object)
    {
        m_InstanceWidth = object.m_InstanceWidth;
        m_InstanceHeight = object.m_InstanceHeight;
        m_InstanceWidthUnitsValue = object.m_InstanceWidthUnitsValue;
        m_InstanceHeightUnitsValue = object.m_InstanceHeightUnitsValue;
        m_InstanceWidthScaleType = object.m_InstanceWidthScaleType;
        m_InstanceHeightScaleType = object.m_InstanceHeightScaleType;
        NestedArtboard::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
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
        return NestedArtboard::deserialize(propertyKey, reader);
    }

protected:
    virtual void instanceWidthChanged() {}
    virtual void instanceHeightChanged() {}
    virtual void instanceWidthUnitsValueChanged() {}
    virtual void instanceHeightUnitsValueChanged() {}
    virtual void instanceWidthScaleTypeChanged() {}
    virtual void instanceHeightScaleTypeChanged() {}
};
} // namespace rive

#endif