#ifndef _RIVE_NESTED_ARTBOARD_LEAF_BASE_HPP_
#define _RIVE_NESTED_ARTBOARD_LEAF_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/nested_artboard.hpp"
namespace rive
{
class NestedArtboardLeafBase : public NestedArtboard
{
protected:
    typedef NestedArtboard Super;

public:
    static const uint16_t typeKey = 451;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedArtboardLeafBase::typeKey:
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

    static const uint16_t fitPropertyKey = 538;
    static const uint16_t alignmentXPropertyKey = 644;
    static const uint16_t alignmentYPropertyKey = 645;

private:
    uint32_t m_Fit = 0;
    float m_AlignmentX = 0.0f;
    float m_AlignmentY = 0.0f;

public:
    inline uint32_t fit() const { return m_Fit; }
    void fit(uint32_t value)
    {
        if (m_Fit == value)
        {
            return;
        }
        m_Fit = value;
        fitChanged();
    }

    inline float alignmentX() const { return m_AlignmentX; }
    void alignmentX(float value)
    {
        if (m_AlignmentX == value)
        {
            return;
        }
        m_AlignmentX = value;
        alignmentXChanged();
    }

    inline float alignmentY() const { return m_AlignmentY; }
    void alignmentY(float value)
    {
        if (m_AlignmentY == value)
        {
            return;
        }
        m_AlignmentY = value;
        alignmentYChanged();
    }

    Core* clone() const override;
    void copy(const NestedArtboardLeafBase& object)
    {
        m_Fit = object.m_Fit;
        m_AlignmentX = object.m_AlignmentX;
        m_AlignmentY = object.m_AlignmentY;
        NestedArtboard::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case fitPropertyKey:
                m_Fit = CoreUintType::deserialize(reader);
                return true;
            case alignmentXPropertyKey:
                m_AlignmentX = CoreDoubleType::deserialize(reader);
                return true;
            case alignmentYPropertyKey:
                m_AlignmentY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return NestedArtboard::deserialize(propertyKey, reader);
    }

protected:
    virtual void fitChanged() {}
    virtual void alignmentXChanged() {}
    virtual void alignmentYChanged() {}
};
} // namespace rive

#endif